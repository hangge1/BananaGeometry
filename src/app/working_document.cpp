#include "app/working_document.h"
#include <algorithm>
#include <set>
#include <stdexcept>
namespace drawer {
namespace {
void CheckName(const std::string& name) {
  if (name.empty() || name.size() > 1024 ||
      name.find('\0') != std::string::npos)
    throw std::runtime_error("Name must contain 1..1024 UTF-8 bytes");
  ToJson(Value(name));
}
Document Data(const InstanceEntry& entry) {
  return entry.runtime->session ? entry.runtime->session->SaveData()
                                : entry.runtime->unavailable;
}
}  // namespace
WorkingDocument::WorkingDocument(std::string name) {
  CheckName(name);
  states_.push_back({std::move(name), {}, {}});
  saved_.name = Name();
}
InstanceEntry* WorkingDocument::Find(int id) {
  auto& entries = states_[index_].instances;
  auto found = std::find_if(entries.begin(), entries.end(),
                            [id](const auto& e) { return e.id == id; });
  return found == entries.end() ? nullptr : &*found;
}
const InstanceEntry* WorkingDocument::Find(int id) const {
  for (const auto& entry : Instances())
    if (entry.id == id) return &entry;
  return nullptr;
}
void WorkingDocument::FixActive() {
  if (Find(active_instance)) return;
  active_instance = Instances().empty() ? 0 : Instances().front().id;
  for (const auto& entry : Instances())
    if (entry.runtime->session) {
      active_instance = entry.id;
      break;
    }
}
void WorkingDocument::Push(State state) {
  states_.resize(index_ + 1);
  states_.push_back(std::move(state));
  ++index_;
  // Count each retained runtime once. This is a serialized-data budget, not VM
  // RSS.
  size_t bytes = 0;
  std::set<const RuntimeInstance*> counted;
  for (const auto& version : states_)
    for (const auto& entry : version.instances)
      if (counted.insert(entry.runtime.get()).second) {
        const auto& runtime = *entry.runtime;
        bytes += ToJson(runtime.session ? runtime.session->State()
                                        : runtime.unavailable.state)
                     .size();
      }
  while (states_.size() > 1 &&
         (states_.size() > 101 || bytes > 64 * 1024 * 1024)) {
    states_.erase(states_.begin());
    --index_;
    bytes = 0;
    counted.clear();
    for (const auto& version : states_)
      for (const auto& entry : version.instances)
        if (counted.insert(entry.runtime.get()).second)
          bytes +=
              ToJson(entry.runtime->session ? entry.runtime->session->State()
                                            : entry.runtime->unavailable.state)
                  .size();
  }
  FixActive();
}
int WorkingDocument::Add(std::string name,
                         std::shared_ptr<RuntimeInstance> runtime) {
  CheckName(name);
  if (!runtime || next_id_ >= 2147483646)
    throw std::runtime_error("Cannot create instance");
  auto next = states_[index_];
  const int id = next_id_++;
  next.instances.push_back({id, std::move(name), std::move(runtime)});
  next.label = "添加实例";
  Push(std::move(next));
  active_instance = id;
  return id;
}
void WorkingDocument::Remove(int id) {
  if (!Find(id)) return;
  auto next = states_[index_];
  next.instances.erase(
      std::remove_if(next.instances.begin(), next.instances.end(),
                     [id](const auto& e) { return e.id == id; }),
      next.instances.end());
  next.label = "删除实例";
  Push(std::move(next));
}
void WorkingDocument::RenameInstance(int id, std::string name) {
  CheckName(name);
  if (!Find(id) || Find(id)->name == name) return;
  auto next = states_[index_];
  for (auto& entry : next.instances)
    if (entry.id == id) entry.name = name;
  next.label = "重命名实例";
  Push(std::move(next));
}
void WorkingDocument::Rename(std::string name) {
  CheckName(name);
  if (name == Name()) return;
  auto next = states_[index_];
  next.name = std::move(name);
  next.label = "重命名文档";
  Push(std::move(next));
}
bool WorkingDocument::Undo() {
  if (!CanUndo()) return false;
  --index_;
  FixActive();
  return true;
}
bool WorkingDocument::Redo() {
  if (!CanRedo()) return false;
  ++index_;
  FixActive();
  return true;
}
std::string WorkingDocument::UndoLabel() const {
  return CanUndo() ? states_[index_].label : "";
}
std::string WorkingDocument::RedoLabel() const {
  return CanRedo() ? states_[index_ + 1].label : "";
}
bool WorkingDocument::Dirty() const {
  if (Name() != saved_.name || Instances().size() != saved_.instances.size())
    return true;
  for (size_t i = 0; i < Instances().size(); ++i) {
    const auto& entry = Instances()[i];
    const auto& saved = saved_.instances[i];
    if (entry.id != saved.id || entry.name != saved.name) return true;
    const auto& runtime = *entry.runtime;
    if (runtime.session) {
      if (runtime.session->Editing() ||
          runtime.session->Id() != saved.data.case_id ||
          runtime.session->DataVersion() != saved.data.case_version ||
          runtime.session->State() != saved.data.state)
        return true;
    } else if (runtime.unavailable.state != saved.data.state ||
               runtime.unavailable.case_id != saved.data.case_id ||
               runtime.unavailable.case_version != saved.data.case_version)
      return true;
  }
  return false;
}
WorkspaceDocument WorkingDocument::Snapshot(bool finish_edits) {
  WorkspaceDocument result;
  result.name = Name();
  for (const auto& entry : Instances()) {
    if (finish_edits && entry.runtime->session &&
        !entry.runtime->session->FinishUiEdit())
      throw std::runtime_error(entry.runtime->session->Error());
    result.instances.push_back({entry.id, entry.name, Data(entry)});
  }
  return result;
}
void WorkingDocument::Load(const WorkspaceDocument& data,
                           std::vector<InstanceEntry> entries) {
  states_ = {{data.name, {}, std::move(entries)}};
  index_ = 0;
  next_id_ = 1;
  for (const auto& e : Instances()) next_id_ = std::max(next_id_, e.id + 1);
  legacy = data.imported_legacy;
  FixActive();
  MarkSaved();
}
void WorkingDocument::MarkSaved() {
  saved_ = Snapshot();
  for (const auto& e : Instances())
    if (e.runtime->session) e.runtime->session->MarkSaved();
}
}  // namespace drawer
