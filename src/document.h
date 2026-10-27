#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "value.h"

namespace drawer {
struct Point {
    double x = 0;
    double y = 0;
};
struct View {
    Point center;
    double zoom = 1;
    Point ToScreen(Point world, Point size) const;
    Point ToWorld(Point screen, Point size) const;
    void ZoomAt(Point screen, Point size, double factor);
    void Pan(Point pixels);
};
double Distance(Point a, Point b);
double SegmentDistance(Point point, Point a, Point b);

class History {
public:
    void Reset(Value state);
    void Push(Value state, std::string label);
    bool Undo();
    bool Redo();
    bool CanUndo() const { return index_ > 0; }
    bool CanRedo() const { return index_ + 1 < states_.size(); }
    const Value& Current() const { return states_.at(index_); }
    void MarkSaved() { saved_ = Current(); }
    bool Dirty() const { return Current() != saved_; }
    std::string UndoLabel() const;
    std::string RedoLabel() const;
    size_t Count() const { return states_.size() - 1; }

private:
    std::vector<Value> states_;
    std::vector<std::string> labels_;
    std::vector<size_t> bytes_;
    size_t index_ = 0;
    size_t total_bytes_ = 0;
    Value saved_;
};

struct Document {
    std::string case_id;
    int case_version = 1;
    Value state;
    View view;
};
void ValidateState(const Value& state);
Value EncodeDocument(const Document& document);
Document DecodeDocument(const Value& root);
Document ReadDocument(const std::filesystem::path& path);
void WriteDocument(const std::filesystem::path& path, const Document& document);

}  // namespace drawer
