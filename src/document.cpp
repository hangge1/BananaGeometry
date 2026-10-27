#include "document.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>

namespace drawer {
namespace {
void ValidatePlainData(const Value& value) {
    if (value.IsNull()) throw std::runtime_error("Null values are not representable in Lua state");
    if (value.IsArray())
        for (const auto& item : value.Items()) ValidatePlainData(item);
    if (value.IsObject())
        for (const auto& field : value.Fields()) ValidatePlainData(field.second);
}
}  // namespace
Point View::ToScreen(Point p, Point size) const {
    return {size.x * .5 + (p.x - center.x) * zoom, size.y * .5 - (p.y - center.y) * zoom};
}
Point View::ToWorld(Point p, Point size) const {
    return {center.x + (p.x - size.x * .5) / zoom, center.y - (p.y - size.y * .5) / zoom};
}
void View::ZoomAt(Point screen, Point size, double factor) {
    auto before = ToWorld(screen, size);
    zoom = std::clamp(zoom * factor, .0001, 10000.0);
    auto after = ToWorld(screen, size);
    center.x += before.x - after.x;
    center.y += before.y - after.y;
}
void View::Pan(Point pixels) {
    center.x -= pixels.x / zoom;
    center.y += pixels.y / zoom;
}
double Distance(Point a, Point b) { return std::hypot(a.x - b.x, a.y - b.y); }
double SegmentDistance(Point p, Point a, Point b) {
    double dx = b.x - a.x, dy = b.y - a.y;
    double denominator = dx * dx + dy * dy;
    if (denominator == 0) return Distance(p, a);
    double t = std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / denominator, 0.0, 1.0);
    return Distance(p, {a.x + t * dx, a.y + t * dy});
}

void History::Reset(Value state) {
    total_bytes_ = ToJson(state).size();
    states_ = {std::move(state)};
    labels_ = {""};
    bytes_ = {total_bytes_};
    index_ = 0;
    MarkSaved();
}
void History::Push(Value state, std::string label) {
    if (state == Current()) return;
    const size_t bytes = ToJson(state).size();
    for (size_t i = index_ + 1; i < bytes_.size(); ++i) total_bytes_ -= bytes_[i];
    states_.resize(index_ + 1);
    labels_.resize(index_ + 1);
    bytes_.resize(index_ + 1);
    states_.push_back(std::move(state));
    labels_.push_back(std::move(label));
    bytes_.push_back(bytes);
    total_bytes_ += bytes;
    ++index_;
    while (states_.size() > 101 || (total_bytes_ > 64 * 1024 * 1024 && states_.size() > 2)) {
        total_bytes_ -= bytes_.front();
        states_.erase(states_.begin());
        labels_.erase(labels_.begin());
        bytes_.erase(bytes_.begin());
        --index_;
    }
}
bool History::Undo() {
    if (!CanUndo()) return false;
    --index_;
    return true;
}
bool History::Redo() {
    if (!CanRedo()) return false;
    ++index_;
    return true;
}
std::string History::UndoLabel() const { return CanUndo() ? labels_[index_] : ""; }
std::string History::RedoLabel() const { return CanRedo() ? labels_[index_ + 1] : ""; }

void ValidateState(const Value& state) {
    // Serializing first enforces the depth limit before recursive shape validation.
    if (ToJson(state).size() > 16 * 1024 * 1024) throw std::runtime_error("State exceeds 16 MiB");
    ValidatePlainData(state);
    if (!state.IsObject() || !state.At("objects").IsArray() || !state.At("params").IsObject())
        throw std::runtime_error("State must contain objects array and params object");
    if (state.At("objects").Items().size() > 10000)
        throw std::runtime_error("Object limit is 10000");
    std::set<double> ids;
    double highest = 0;
    for (const auto& shape : state.At("objects").Items()) {
        auto id = shape.At("id").NumberOr(-1);
        if (!shape.IsObject() || !shape.At("kind").IsString() || id < 1 || std::floor(id) != id ||
            !ids.insert(id).second)
            throw std::runtime_error("Invalid object or duplicate object ID");
        const auto& points = shape.At("points");
        if (!points.IsNull() && !points.IsArray())
            throw std::runtime_error("Object points must be an array");
        if (points.IsArray())
            for (const auto& point : points.Items()) {
                if (!point.At("x").IsNumber() || !point.At("y").IsNumber() ||
                    std::abs(point.At("x").Number()) > 1e12 ||
                    std::abs(point.At("y").Number()) > 1e12)
                    throw std::runtime_error("Invalid point (coordinate range is +/- 1e12)");
            }
        highest = std::max(highest, id);
        const auto kind = shape.At("kind").String();
        const size_t count = points.IsArray() ? points.Items().size() : 0;
        if (((kind == "point" || kind == "text") && count < 1) ||
            ((kind == "line" || kind == "polyline" || kind == "rect" || kind == "circle" ||
              kind == "ellipse") &&
             count < 2) ||
            ((kind == "triangle" || kind == "polygon") && count < 3) ||
            (kind == "bezier" && count != 4))
            throw std::runtime_error("Invalid point count for " + kind);
    }
    auto next = state.At("next_id").NumberOr(0);
    if (next <= highest || next > 9007199254740991.0 || std::floor(next) != next)
        throw std::runtime_error("Invalid next object ID");
}
Value EncodeDocument(const Document& d) {
    ValidateState(d.state);
    return Value::Object{
        {"format", "drawer"},
        {"format_version", 1},
        {"case_id", d.case_id},
        {"case_version", d.case_version},
        {"state", d.state},
        {"view",
         Value::Object{{"x", d.view.center.x}, {"y", d.view.center.y}, {"zoom", d.view.zoom}}}};
}
Document DecodeDocument(const Value& root) {
    if (root.At("format").StringOr("") != "drawer" || root.At("format_version").NumberOr(0) != 1)
        throw std::runtime_error("Unsupported Drawer document format");
    Document result;
    result.case_id = root.At("case_id").StringOr("");
    double version = root.At("case_version").NumberOr(0);
    if (result.case_id.empty() || version < 1 || version > 1000000 ||
        std::floor(version) != version)
        throw std::runtime_error("Invalid case identifier/version");
    result.case_version = static_cast<int>(version);
    result.state = root.At("state");
    ValidateState(result.state);
    const auto& view = root.At("view");
    if (!view.At("x").IsNumber() || !view.At("y").IsNumber() || !view.At("zoom").IsNumber())
        throw std::runtime_error("Missing view coordinates");
    result.view = {{view.At("x").Number(), view.At("y").Number()}, view.At("zoom").Number()};
    if (!std::isfinite(result.view.zoom) || result.view.zoom < .0001 || result.view.zoom > 10000 ||
        !std::isfinite(result.view.center.x) || !std::isfinite(result.view.center.y) ||
        std::abs(result.view.center.x) > 1e12 || std::abs(result.view.center.y) > 1e12)
        throw std::runtime_error("Invalid document view");
    return result;
}
Document ReadDocument(const std::filesystem::path& path) {
    if (std::filesystem::file_size(path) > 16 * 1024 * 1024)
        throw std::runtime_error("File exceeds 16 MiB");
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("Cannot open document");
    std::string text((std::istreambuf_iterator<char>(input)), {});
    return DecodeDocument(ParseJson(text));
}
void WriteDocument(const std::filesystem::path& path, const Document& document) {
    std::string text = ToJson(EncodeDocument(document));
    if (text.size() > 16 * 1024 * 1024) throw std::runtime_error("Document exceeds 16 MiB");
    // Write beside the destination so replacement stays on the same filesystem.
    static std::atomic<unsigned> sequence{0};
    auto temporary = path;
    temporary +=
        L".tmp." + std::to_wstring(GetCurrentProcessId()) + L"." + std::to_wstring(++sequence);
    try {
        {
            std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
            output.exceptions(std::ios::failbit | std::ios::badbit);
            output.write(text.data(), static_cast<std::streamsize>(text.size()));
            output.flush();
            output.close();
        }
        if (!MoveFileExW(temporary.c_str(), path.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
            throw std::runtime_error("Cannot replace destination document (Windows error " +
                                     std::to_string(GetLastError()) + ")");
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}
}  // namespace drawer
