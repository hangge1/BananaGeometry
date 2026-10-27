#include "core/history.h"
#include <utility>
namespace drawer {
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
  while (states_.size() > 101 ||
         (total_bytes_ > 64 * 1024 * 1024 && states_.size() > 2)) {
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
std::string History::UndoLabel() const {
  return CanUndo() ? labels_[index_] : "";
}
std::string History::RedoLabel() const {
  return CanRedo() ? labels_[index_ + 1] : "";
}

}  // namespace drawer
