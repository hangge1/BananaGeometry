#ifndef DRAWER_SRC_CORE_HISTORY_H_
#define DRAWER_SRC_CORE_HISTORY_H_
#include <string>
#include <vector>
#include "core/value.h"
namespace drawer {
// Stores committed snapshots independently of view and interaction state.
// Reset must seed the history before any other operation. References returned
// by Current may be invalidated by Reset or Push; callers must not retain them.
class History {
 public:
  void Reset(Value state);
  void Push(Value state, std::string label);
  bool Undo();
  bool Redo();
  bool CanUndo() const { return index_ > 0; }
  bool CanRedo() const { return index_ + 1 < states_.size(); }
  const Value& Current() const { return states_.at(index_); }
  // Save points compare content, so undoing to saved data clears the dirty
  // flag.
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

}  // namespace drawer
#endif  // DRAWER_SRC_CORE_HISTORY_H_
