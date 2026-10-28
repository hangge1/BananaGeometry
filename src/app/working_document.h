#ifndef DRAWER_SRC_APP_WORKING_DOCUMENT_H_
#define DRAWER_SRC_APP_WORKING_DOCUMENT_H_
#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include "core/workspace_document.h"
#include "runtime/session.h"
namespace drawer {
struct RuntimeInstance {
  std::unique_ptr<Session> session;
  Document unavailable;
  std::string error;
};
struct InstanceEntry {
  int id;
  std::string name;
  std::shared_ptr<RuntimeInstance> runtime;
};
// Management snapshots share live runtimes: undoing a rename must not undo
// geometry.
class WorkingDocument {
 public:
  explicit WorkingDocument(std::string name);
  const std::string& Name() const { return states_[index_].name; }
  const std::vector<InstanceEntry>& Instances() const {
    return states_[index_].instances;
  }
  InstanceEntry* Find(int id);
  const InstanceEntry* Find(int id) const;
  int Add(std::string name, std::shared_ptr<RuntimeInstance> runtime);
  void Remove(int id);
  void RenameInstance(int id, std::string name);
  void Rename(std::string name);
  bool Undo();
  bool Redo();
  bool CanUndo() const { return index_ > 0; }
  bool CanRedo() const { return index_ + 1 < states_.size(); }
  std::string UndoLabel() const;
  std::string RedoLabel() const;
  bool Dirty() const;
  // With finish_edits, commit property input and cancel canvas drafts first.
  // Throws on serialization or script failure; the caller must not mark saved.
  WorkspaceDocument Snapshot(bool finish_edits = false);
  void Load(const WorkspaceDocument& data, std::vector<InstanceEntry> entries);
  void MarkSaved();
  // Remembered preview; Application separately requires an explicit selection.
  int active_instance = 0;
  std::filesystem::path file;
  bool legacy = false;

 private:
  struct State {
    std::string name, label;
    std::vector<InstanceEntry> instances;
  };
  void Push(State state);
  void FixActive();
  std::vector<State> states_;
  size_t index_ = 0;
  int next_id_ = 1;
  WorkspaceDocument saved_;
};
}  // namespace drawer
#endif  // DRAWER_SRC_APP_WORKING_DOCUMENT_H_
