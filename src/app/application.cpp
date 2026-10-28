#include "app/application.h"
#include <algorithm>
#include <stdexcept>
#include "app/workspace_paths.h"
#include "platform/desktop_actions.h"
namespace drawer {
Application::Application(std::filesystem::path runtime_root,
                         std::filesystem::path case_directory,
                         DesktopServices desktop)
    : runtime_root_(std::filesystem::absolute(runtime_root).lexically_normal()),
      case_directory_(ResolveCaseDirectory(runtime_root_, case_directory)),
      desktop_(std::move(desktop)) {
  if (!std::filesystem::is_regular_file(runtime_root_ / "sdk/drawer.lua"))
    throw std::runtime_error("Runtime requires sdk/drawer.lua");
  if (!desktop_.choose_directory)
    desktop_.choose_directory = ChooseCaseDirectory;
  if (!desktop_.edit_script) desktop_.edit_script = EditScriptFile;
  if (desktop_.load_layout) {
    try {
      auto layout = desktop_.load_layout();
      for (auto pair : {std::pair<const char*, bool*>{"tree", &show_tree_},
                        {"properties", &show_properties_},
                        {"diagnostics", &show_diagnostics_}})
        if (layout.At(pair.first).IsBool())
          *pair.second = layout.At(pair.first).Bool();
      tree_width_ = static_cast<float>(
          std::clamp(layout.At("tree_width").NumberOr(245), 150., 1000.));
      properties_width_ = static_cast<float>(
          std::clamp(layout.At("properties_width").NumberOr(325), 220., 1000.));
    } catch (const std::exception& e) {
      ReportError(e.what());
    }
  }
  saved_layout_ = Layout();
  RefreshCases();
  for (const auto& item : cases_)
    if (item.id == "drawer.triangle") {
      NewCase(item.entry);
      break;
    }
  if (!Active() && !cases_.empty()) NewCase(cases_.front().entry);
  if (ActiveDocument()) ActiveDocument()->MarkSaved();
  SelectDocument();
}
void Application::ReportError(const std::string& message) {
  if (!error_.empty()) error_ += '\n';
  error_ += message;
}
void Application::RefreshCases() {
  try {
    auto library =
        ScanCaseLibrary(runtime_root_ / "sdk/drawer.lua", case_directory_);
    cases_ = std::move(library.cases);
    error_ = std::move(library.diagnostics);
  } catch (const std::exception& e) {
    error_ = e.what();
  }
}
bool Application::SetCaseDirectory(const std::filesystem::path& directory) {
  try {
    auto selected = ResolveCaseDirectory(runtime_root_, directory);
    auto library = ScanCaseLibrary(runtime_root_ / "sdk/drawer.lua", selected);
    case_directory_ = selected;
    cases_ = std::move(library.cases);
    error_ = std::move(library.diagnostics);
    notice_ = "用例目录已打开；已有实例继续绑定原脚本。";
    if (desktop_.remember_directory) {
      try {
        desktop_.remember_directory(selected ==
                                            ResolveCaseDirectory(runtime_root_)
                                        ? std::filesystem::path{}
                                        : selected);
      } catch (const std::exception& e) {
        ReportError("无法记住目录：" + std::string(e.what()));
      }
    }
    return true;
  } catch (const std::exception& e) {
    error_ = e.what();
    return false;
  }
}
bool Application::OpenCaseDirectory() {
  try {
    auto path = desktop_.choose_directory(case_directory_);
    return !path.empty() && SetCaseDirectory(path);
  } catch (const std::exception& e) {
    error_ = e.what();
    return false;
  }
}
bool Application::EditCurrentScript() {
  auto* s = Active();
  if (!s) return false;
  try {
    if (!std::filesystem::is_regular_file(s->Entry()))
      throw std::runtime_error("Script does not exist: " +
                               s->Entry().u8string());
    desktop_.edit_script(s->Entry());
    error_.clear();
    notice_ = "已请求编辑当前脚本；保存后按 F5 应用修改。";
    return true;
  } catch (const std::exception& e) {
    error_ = e.what();
    return false;
  }
}
WorkingDocument* Application::FindDocument(int key) {
  for (auto& tab : tabs_)
    if (tab.key == key) return tab.document.get();
  return nullptr;
}
WorkingDocument* Application::ActiveDocument() { return FindDocument(active_); }
InstanceEntry* Application::ActiveEntry() {
  return selection_ == Selection::kDocument ? nullptr : PreviewEntry();
}
InstanceEntry* Application::PreviewEntry() {
  auto* d = ActiveDocument();
  return d ? d->Find(d->active_instance) : nullptr;
}
Session* Application::Active() {
  auto* e = ActiveEntry();
  return e ? e->runtime->session.get() : nullptr;
}
Session* Application::Preview() {
  auto* e = PreviewEntry();
  return e ? e->runtime->session.get() : nullptr;
}
bool Application::SelectDocument() { return Activate(active_, 0); }
bool Application::SelectInstance(int instance) {
  return instance > 0 && Activate(active_, instance);
}
bool Application::FinishActiveEdit() {
  if (rename_edit_) {
    auto edit = std::move(rename_edit_);
    rename_edit_ = {};
    try {
      edit();
    } catch (const std::exception& e) {
      error_ = e.what();
      return false;
    }
  }
  auto* s = Active();
  if (!s || s->FinishUiEdit()) return true;
  error_ = s->Error();
  return false;
}
void Application::NewDocument() {
  if (!FinishActiveEdit()) return;
  active_ = next_key_++;
  select_key_ = active_;
  tabs_.push_back({active_, std::make_unique<WorkingDocument>(
                                "未命名文档 " + std::to_string(active_))});
  selection_ = Selection::kDocument;
}
void Application::NewCase(const std::filesystem::path& entry) {
  try {
    auto r = std::make_shared<RuntimeInstance>();
    r->session =
        std::make_unique<Session>(runtime_root_ / "sdk/drawer.lua", entry);
    if (!ActiveDocument()) NewDocument();
    if (!FinishActiveEdit()) return;
    auto* d = ActiveDocument();
    d->Add(r->session->Name() + " " + std::to_string(d->Instances().size() + 1),
           r);
    selection_ = Selection::kInstance;
  } catch (const std::exception& e) {
    error_ = e.what();
  }
}
bool Application::Activate(int document, int instance, double object) {
  auto* d = FindDocument(document);
  if (!d || (instance && !d->Find(instance))) return false;
  if (!FinishActiveEdit()) return false;
  active_ = document;
  select_key_ = document;
  if (instance) d->active_instance = instance;
  selection_ = instance ? Selection::kInstance : Selection::kDocument;
  if (Preview() && !object && !Preview()->SelectObject(0)) return false;
  if (object && Active()) {
    if (!Active()->SelectObject(object)) return false;
    selection_ = Selection::kObject;
  }
  return true;
}
bool Application::SmokeCheck(const std::string& case_id,
                             const std::filesystem::path& fixture) {
  if (cases_.empty()) {
    if (!error_.empty()) throw std::runtime_error(error_);
    return false;
  }
  for (const auto& info : cases_) {
    Session s(runtime_root_ / "sdk/drawer.lua", info.entry);
    if (!s.Controls({}) || !s.Draw(1)) throw std::runtime_error(s.Error());
  }
  for (const auto& info : cases_)
    if (info.id == case_id) {
      if (!Preview() || Preview()->Entry() != info.entry)
        NewCase(info.entry);
      else
        SelectInstance(ActiveDocument()->active_instance);
      break;
    }
  if (!Active() || Active()->Id() != case_id) return false;
  if (!fixture.empty() && !Active()->ExecuteFile(fixture))
    throw std::runtime_error(Active()->Error());
  return true;
}
}  // namespace drawer
