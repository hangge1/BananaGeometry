#include "core/document.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <set>
#include <stdexcept>
#include "core/bezier.h"
namespace drawer {
namespace {
void ValidatePlainData(const Value& value) {
  if (value.IsNull())
    throw std::runtime_error("Null values are not representable in Lua state");
  if (value.IsArray())
    for (const auto& item : value.Items()) ValidatePlainData(item);
  if (value.IsObject())
    for (const auto& field : value.Fields()) ValidatePlainData(field.second);
}
}  // namespace
void ValidateState(const Value& state) {
  // Serializing first enforces the depth limit before recursive shape
  // validation.
  if (ToJson(state).size() > 16 * 1024 * 1024)
    throw std::runtime_error("State exceeds 16 MiB");
  ValidatePlainData(state);
  if (!state.IsObject() || !state.At("objects").IsArray() ||
      !state.At("params").IsObject())
    throw std::runtime_error(
        "State must contain objects array and params object");
  if (state.At("objects").Items().size() > 10000)
    throw std::runtime_error("Object limit is 10000");
  std::set<double> ids;
  double highest = 0;
  for (const auto& shape : state.At("objects").Items()) {
    auto id = shape.At("id").NumberOr(-1);
    if (!shape.IsObject() || !shape.At("kind").IsString() || id < 1 ||
        std::floor(id) != id || !ids.insert(id).second)
      throw std::runtime_error("Invalid object or duplicate object ID");
    const auto& points = shape.At("points");
    if (!points.IsNull() && !points.IsArray())
      throw std::runtime_error("Object points must be an array");
    if (points.IsArray())
      for (const auto& point : points.Items()) {
        if (!point.At("x").IsNumber() || !point.At("y").IsNumber() ||
            std::abs(point.At("x").Number()) > 1e12 ||
            std::abs(point.At("y").Number()) > 1e12)
          throw std::runtime_error(
              "Invalid point (coordinate range is +/- 1e12)");
      }
    highest = std::max(highest, id);
    const auto kind = shape.At("kind").String();
    if (kind == "text" && !shape.At("font_size").IsNull()) {
      const auto& size = shape.At("font_size");
      if (!size.IsNumber() || size.Number() < kMinTextFontSize ||
          size.Number() > kMaxTextFontSize)
        throw std::runtime_error("Text font_size must be within 6..256 pixels");
    }
    const size_t count = points.IsArray() ? points.Items().size() : 0;
    if (((kind == "point" || kind == "text") && count < 1) ||
        ((kind == "line" || kind == "polyline" || kind == "rect" ||
          kind == "circle" || kind == "ellipse") &&
         count < 2) ||
        ((kind == "triangle" || kind == "polygon") && count < 3) ||
        (kind == "bezier" && (count < 2 || count > kMaxBezierDegree + 1)))
      throw std::runtime_error("Invalid point count for " + kind);
    if (kind == "bezier") {
      const auto& closed = shape.At("closed");
      const auto& smooth = shape.At("smooth");
      if (!closed.IsNull() && !closed.IsBool())
        throw std::runtime_error("Bezier closed must be a boolean");
      if (!smooth.IsNull() && !smooth.IsBool())
        throw std::runtime_error("Bezier smooth must be a boolean");
      std::vector<Point> controls;
      for (const auto& point : points.Items())
        controls.push_back({point.At("x").Number(), point.At("y").Number()});
      ValidateBezierJoin(controls, closed.IsBool() && closed.Bool(),
                         smooth.IsBool() && smooth.Bool());
    }
  }
  auto next = state.At("next_id").NumberOr(0);
  if (next <= highest || next > 9007199254740991.0 || std::floor(next) != next)
    throw std::runtime_error("Invalid next object ID");
}
Value EncodeDocument(const Document& d) {
  ValidateState(d.state);
  return Value::Object{{"format", "drawer"},
                       {"format_version", 1},
                       {"case_id", d.case_id},
                       {"case_version", d.case_version},
                       {"state", d.state},
                       {"view", Value::Object{{"x", d.view.center.x},
                                              {"y", d.view.center.y},
                                              {"zoom", d.view.zoom}}}};
}
Document DecodeDocument(const Value& root) {
  if (root.At("format").StringOr("") != "drawer" ||
      root.At("format_version").NumberOr(0) != 1)
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
  if (!view.At("x").IsNumber() || !view.At("y").IsNumber() ||
      !view.At("zoom").IsNumber())
    throw std::runtime_error("Missing view coordinates");
  result.view = {{view.At("x").Number(), view.At("y").Number()},
                 view.At("zoom").Number()};
  if (!std::isfinite(result.view.zoom) || result.view.zoom < .0001 ||
      result.view.zoom > 10000 || !std::isfinite(result.view.center.x) ||
      !std::isfinite(result.view.center.y) ||
      std::abs(result.view.center.x) > 1e12 ||
      std::abs(result.view.center.y) > 1e12)
    throw std::runtime_error("Invalid document view");
  return result;
}
void FitView(View* view, const Value& state, Point size) {
  Point lower{std::numeric_limits<double>::max(),
              std::numeric_limits<double>::max()};
  Point upper{-lower.x, -lower.y};
  auto include = [&](Point p) {
    lower.x = std::min(lower.x, p.x);
    lower.y = std::min(lower.y, p.y);
    upper.x = std::max(upper.x, p.x);
    upper.y = std::max(upper.y, p.y);
  };
  for (const auto& object : state.At("objects").Items()) {
    const auto& points = object.At("points");
    if (!points.IsArray()) continue;
    for (const auto& p : points.Items())
      include({p.At("x").NumberOr(0), p.At("y").NumberOr(0)});
    auto kind = object.At("kind").StringOr("");
    if ((kind == "circle" || kind == "ellipse") && points.Items().size() >= 2) {
      Point a{points.Items()[0].At("x").Number(),
              points.Items()[0].At("y").Number()};
      Point b{points.Items()[1].At("x").Number(),
              points.Items()[1].At("y").Number()};
      double rx = kind == "circle" ? Distance(a, b) : std::abs(a.x - b.x);
      double ry = kind == "circle" ? rx : std::abs(a.y - b.y);
      include({a.x - rx, a.y - ry});
      include({a.x + rx, a.y + ry});
    }
  }
  if (lower.x > upper.x) {
    *view = {};
    return;
  }
  view->center = {(lower.x + upper.x) * .5, (lower.y + upper.y) * .5};
  view->zoom = std::clamp(
      std::min(std::max(1.0, size.x - 100) / std::max(1.0, upper.x - lower.x),
               std::max(1.0, size.y - 100) / std::max(1.0, upper.y - lower.y)),
      .0001, 10000.0);
}
}  // namespace drawer
