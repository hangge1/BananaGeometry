#include "app/application.h"
#include <algorithm>
#include <stdexcept>
#include "imgui.h"
#include "platform/document_io.h"
#include "platform/file_dialog.h"
namespace drawer {
std::shared_ptr<RuntimeInstance> Application::RestoreInstance(
    const Document& data) {
  auto result = std::make_shared<RuntimeInstance>();
  result->unavailable = data;
  try {
    auto found = std::find_if(cases_.begin(), cases_.end(), [&](const auto& c) {
      return c.id == data.case_id;
    });
    if (found == cases_.end())
      throw std::runtime_error("Missing case plugin: " + data.case_id);
    auto session = std::make_unique<Session>(runtime_root_ / "sdk/drawer.lua",
                                             found->entry);
    session->Restore(data);
    result->session = std::move(session);
  } catch (const std::exception& e) {
    result->error = e.what();
  }
  return result;
}
bool Application::OpenDocument(const std::filesystem::path& path) {
  try {
    auto data = ReadWorkspace(path);
    auto d = std::make_unique<WorkingDocument>(data.name);
    std::vector<InstanceEntry> entries;
    for (const auto& entry : data.instances)
      entries.push_back({entry.id, entry.name, RestoreInstance(entry.data)});
    d->Load(data, std::move(entries));
    d->file = path;
    if (!FinishActiveEdit()) return false;
    active_ = next_key_++;
    select_key_ = active_;
    tabs_.push_back({active_, std::move(d)});
    selection_ = Selection::kDocument;
    error_.clear();
    notice_ = data.imported_legacy
                  ? "旧版文档已导入；首次保存默认另存为版本 2。"
                  : "文档已打开；各实例历史重新开始。";
    return true;
  } catch (const std::exception& e) {
    error_ = e.what();
    return false;
  }
}
void Application::Open() {
  auto path = ChooseDocumentPath(DocumentDialog::kOpen);
  if (!path.empty()) OpenDocument(path);
}
bool Application::SaveDocument(const std::filesystem::path& path) {
  auto* d = ActiveDocument();
  if (!d || path.empty()) return false;
  if (!FinishActiveEdit()) return false;
  try {
    auto snapshot = d->Snapshot(true);
    WriteWorkspace(path, snapshot);
    d->file = path;
    d->legacy = false;
    d->MarkSaved();
    error_.clear();
    notice_ = "整份文档已保存：" + path.u8string();
    return true;
  } catch (const std::exception& e) {
    error_ = e.what();
    return false;
  }
}
bool Application::Save(bool save_as) {
  auto* d = ActiveDocument();
  if (!d) return false;
  auto path = save_as || d->legacy || d->file.empty()
                  ? ChooseDocumentPath(DocumentDialog::kSave)
                  : d->file;
  return SaveDocument(path);
}
bool Application::RetryInstance() {
  auto* e = ActiveEntry();
  if (!e || e->runtime->session) return false;
  auto candidate = RestoreInstance(e->runtime->unavailable);
  if (!candidate->session) {
    e->runtime->error = candidate->error;
    error_ = candidate->error;
    return false;
  }
  // Preserve the runtime identity referenced by management history.
  e->runtime->session = std::move(candidate->session);
  e->runtime->error.clear();
  error_.clear();
  notice_ = "实例已恢复，原始数据已保留。";
  return true;
}
void Application::Request(Action action, int document, int instance) {
  if (pending_ != Action::kNone) return;
  if ((!document || document == active_) && !FinishActiveEdit()) return;
  pending_ = action;
  target_ = document ? document : active_;
  auto* d = FindDocument(target_);
  target_instance_ = instance ? instance : (d ? d->active_instance : 0);
}
void Application::RequestExit() { Request(Action::kExit); }
void Application::Complete() {
  auto action = pending_;
  pending_ = Action::kNone;
  if (action == Action::kExit) {
    exit_ = true;
    return;
  }
  auto* d = FindDocument(target_);
  if (!d) return;
  if (action == Action::kClose) {
    tabs_.erase(std::remove_if(tabs_.begin(), tabs_.end(),
                               [&](const auto& t) { return t.key == target_; }),
                tabs_.end());
    if (active_ == target_) {
      active_ = tabs_.empty() ? 0 : tabs_.back().key;
      select_key_ = active_;
      selection_ = Selection::kDocument;
      if (Preview()) Preview()->SelectObject(0);
    }
    return;
  }
  auto* entry = d->Find(target_instance_);
  if (!entry) return;
  if (action == Action::kRemoveInstance) {
    if (entry->runtime->session && !entry->runtime->session->FinishUiEdit()) {
      error_ = entry->runtime->session->Error();
      return;
    }
    d->Remove(entry->id);
    if (target_ == active_) SelectDocument();
    return;
  }
  if (action == Action::kReload && entry->runtime->session) {
    try {
      auto& old = *entry->runtime->session;
      auto candidate = std::make_unique<Session>(old.Sdk(), old.Entry());
      if (candidate->Id() != old.Id())
        throw std::runtime_error(
            "Case ID changed; create a new instance instead");
      if (candidate->Entry().parent_path().parent_path() == case_directory_) {
        auto found = std::find_if(
            cases_.begin(), cases_.end(),
            [&](const auto& c) { return c.entry == candidate->Entry(); });
        if (found != cases_.end())
          found->name = candidate->Name();
        else
          cases_.push_back(
              {candidate->Entry(), candidate->Id(), candidate->Name()});
      }
      entry->runtime->session = std::move(candidate);
      error_.clear();
      notice_ = "当前实例已重置，其他实例不变。";
    } catch (const std::exception& e) {
      error_ = "重载失败，原实例已保留：\n" + std::string(e.what());
    }
  }
}
void Application::Confirm() {
  if (pending_ == Action::kNone) return;
  auto* d = FindDocument(target_);
  if (pending_ == Action::kExit) {
    d = nullptr;
    for (auto& t : tabs_)
      if (t.document->Dirty()) {
        d = t.document.get();
        target_ = t.key;
        break;
      }
    if (!d) {
      Complete();
      return;
    }
  }
  if (!d) {
    pending_ = Action::kNone;
    return;
  }
  auto* e = d->Find(target_instance_);
  const bool dirty =
      pending_ == Action::kReload
          ? (e && e->runtime->session && e->runtime->session->Dirty())
          : d->Dirty();
  if (!dirty && pending_ != Action::kRemoveInstance) {
    Complete();
    return;
  }
  ImGui::OpenPopup("未保存的修改");
  if (ImGui::BeginPopupModal("未保存的修改", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted(d->Name().c_str());
    ImGui::TextWrapped(
        "保存会保存整份文档。删除实例可通过“撤销文档管理”恢复。");
    if (ImGui::Button("保存并继续")) {
      int previous = active_;
      active_ = target_;
      bool saved = Save();
      active_ = previous;
      if (saved) {
        ImGui::CloseCurrentPopup();
        if (pending_ != Action::kExit) Complete();
      }
    }
    ImGui::SameLine();
    if (ImGui::Button(pending_ == Action::kExit ? "放弃全部并退出"
                                                : "放弃并继续")) {
      ImGui::CloseCurrentPopup();
      Complete();
    }
    ImGui::SameLine();
    if (ImGui::Button("取消")) {
      pending_ = Action::kNone;
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
}
}  // namespace drawer
