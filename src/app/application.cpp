#include "app/application.h"

#include <algorithm>
#include <stdexcept>

#include "app/lua_ui.h"
#include "app/workspace_paths.h"
#include "imgui.h"
#include "platform/document_io.h"
#include "platform/desktop_actions.h"
#include "platform/file_dialog.h"

namespace drawer {
Application::Application(std::filesystem::path runtime_root,
                         std::filesystem::path case_directory,
                         DesktopServices desktop)
    : runtime_root_(std::filesystem::absolute(runtime_root).lexically_normal()),
      case_directory_(ResolveCaseDirectory(runtime_root_, case_directory)),
      desktop_(std::move(desktop)) {
  if (!std::filesystem::is_regular_file(runtime_root_ / "sdk/drawer.lua"))
    throw std::runtime_error("Runtime requires sdk/drawer.lua: " +
                             runtime_root_.u8string());
  if (!desktop_.choose_directory)
    desktop_.choose_directory = ChooseCaseDirectory;
  if (!desktop_.edit_script) desktop_.edit_script = EditScriptFile;
  RefreshCases();
  for (const auto& item : cases_)
    if (item.id == "drawer.triangle") {
      NewCase(item.entry);
      break;
    }
  if (!Active() && !cases_.empty()) NewCase(cases_.front().entry);
}
void Application::RefreshCases() {
  try {
    auto library =
        ScanCaseLibrary(runtime_root_ / "sdk/drawer.lua", case_directory_);
    cases_ = std::move(library.cases);
    error_ = std::move(library.diagnostics);
  } catch (const std::exception& error) {
    error_ = error.what();
  }
}
void Application::ReportError(const std::string& message) {
  if (!error_.empty()) error_ += '\n';
  error_ += message;
}
bool Application::SetCaseDirectory(const std::filesystem::path& directory) {
  try {
    const auto selected = ResolveCaseDirectory(runtime_root_, directory);
    auto library = ScanCaseLibrary(runtime_root_ / "sdk/drawer.lua", selected);
    case_directory_ = selected;
    cases_ = std::move(library.cases);
    error_ = std::move(library.diagnostics);
    notice_ = "用例目录已打开；已打开的画布仍绑定原脚本。";
    if (desktop_.remember_directory) {
      const auto bundled = ResolveCaseDirectory(runtime_root_);
      try {
        desktop_.remember_directory(
            selected == bundled ? std::filesystem::path{} : selected);
      } catch (const std::exception& error) {
        ReportError("目录已打开，但无法记住选择：" + std::string(error.what()));
      }
    }
    return true;
  } catch (const std::exception& error) {
    error_ = error.what();
    return false;
  }
}
bool Application::OpenCaseDirectory() {
  try {
    const auto selected = desktop_.choose_directory(case_directory_);
    return !selected.empty() && SetCaseDirectory(selected);
  } catch (const std::exception& error) {
    error_ = error.what();
    return false;
  }
}
bool Application::EditCurrentScript() {
  auto* session = Active();
  if (!session) return false;
  try {
    if (!std::filesystem::is_regular_file(session->Entry()))
      throw std::runtime_error("Script does not exist: " +
                               session->Entry().u8string());
    desktop_.edit_script(session->Entry());
    error_.clear();
    notice_ = "已请求编辑当前脚本；保存后按 F5 应用修改。";
    return true;
  } catch (const std::exception& error) {
    error_ = error.what();
    return false;
  }
}
void Application::NewCase(const std::filesystem::path& entry) {
  try {
    auto session =
        std::make_unique<Session>(runtime_root_ / "sdk/drawer.lua", entry);
    active_ = next_key_++;
    select_key_ = active_;
    tabs_.push_back({active_, std::move(session)});
  } catch (const std::exception& e) {
    error_ = e.what();
  }
}
Session* Application::Active() {
  for (auto& tab : tabs_)
    if (tab.key == active_) return tab.session.get();
  return nullptr;
}
void Application::Open() {
  try {
    auto path = ChooseDocumentPath(DocumentDialog::kOpen);
    if (path.empty()) return;
    auto document = ReadDocument(path);
    auto found = std::find_if(
        cases_.begin(), cases_.end(),
        [&](const auto& item) { return item.id == document.case_id; });
    if (found == cases_.end())
      throw std::runtime_error("Missing case plugin: " + document.case_id +
                               ". Refresh the case list after installing it.");
    auto session = std::make_unique<Session>(runtime_root_ / "sdk/drawer.lua",
                                             found->entry);
    session->Restore(document);
    session->file = path;
    active_ = next_key_++;
    select_key_ = active_;
    tabs_.push_back({active_, std::move(session)});
    notice_ = "文档已加载；撤销历史从此处开始。";
  } catch (const std::exception& e) {
    error_ = e.what();
  }
}
bool Application::Save(Session* session, bool save_as) {
  try {
    session->Cancel();
    auto path = save_as || session->file.empty()
                    ? ChooseDocumentPath(DocumentDialog::kSave)
                    : session->file;
    if (path.empty()) return false;
    WriteDocument(path, session->SaveData());
    session->file = path;
    session->MarkSaved();
    notice_ = "已保存：" + path.u8string();
    return true;
  } catch (const std::exception& e) {
    error_ = e.what();
    return false;
  }
}
void Application::Request(Action action, int key) {
  if (pending_ != Action::kNone) return;
  pending_ = action;
  target_ = key;
}
void Application::RequestExit() { Request(Action::kExit, 0); }
void Application::Complete() {
  auto action = pending_;
  pending_ = Action::kNone;
  if (action == Action::kExit) {
    exit_ = true;
    return;
  }
  auto found = std::find_if(tabs_.begin(), tabs_.end(), [&](const auto& tab) {
    return tab.key == target_;
  });
  if (found == tabs_.end()) return;
  if (action == Action::kClose) {
    tabs_.erase(found);
    active_ = tabs_.empty() ? 0 : tabs_.back().key;
    select_key_ = active_;
  } else if (action == Action::kReload) {
    try {
      auto candidate = std::make_unique<Session>(found->session->Sdk(),
                                                 found->session->Entry());
      if (candidate->Id() != found->session->Id())
        throw std::runtime_error(
            "Case ID changed; open it as a new case instead");
      // A tab can outlive the selected library; equal IDs do not imply equal
      // sources.
      if (candidate->Entry().parent_path().parent_path() == case_directory_) {
        auto metadata = std::find_if(
            cases_.begin(), cases_.end(),
            [&](const auto& item) { return item.entry == candidate->Entry(); });
        if (metadata != cases_.end())
          metadata->name = candidate->Name();
        else
          cases_.push_back(
              {candidate->Entry(), candidate->Id(), candidate->Name()});
      }
      found->session = std::move(candidate);
      error_.clear();
      notice_ = "脚本修改已应用：当前画布及撤销历史已重置，其他画布不变。";
    } catch (const std::exception& e) {
      error_ = "重载失败，原实例已保留：\n" + std::string(e.what());
    }
  }
}
void Application::Confirm() {
  if (pending_ == Action::kNone) return;
  Session* dirty = nullptr;
  for (auto& tab : tabs_)
    if ((pending_ == Action::kExit || tab.key == target_) &&
        tab.session->Dirty()) {
      dirty = tab.session.get();
      break;
    }
  if (!dirty) {
    Complete();
    return;
  }
  ImGui::OpenPopup("未保存的修改");
  if (ImGui::BeginPopupModal("未保存的修改", nullptr,
                             ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::TextUnformatted(dirty->Name().c_str());
    ImGui::TextUnformatted(
        "继续操作将丢弃尚未保存的数据。\n保存只保存当前提示的画布文档。");
    if (ImGui::Button("保存并继续")) {
      if (Save(dirty)) {
        ImGui::CloseCurrentPopup();
        // Exit may encounter another dirty tab on the next frame.
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
void Application::Controls(Session* session) {
  ImGui::TextUnformatted("用例控件");
  ImGui::Separator();
  session->Controls([&](const auto& operation, const auto& arguments) {
    return DrawLuaWidget(operation, arguments);
  });
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::TextUnformatted("操作历史");
  ImGui::TextWrapped("撤销：%s", session->GetHistory().UndoLabel().c_str());
  ImGui::TextWrapped("重做：%s", session->GetHistory().RedoLabel().c_str());
  ImGui::Spacing();
  ImGui::TextWrapped(
      "中键拖动 / "
      "空格+"
      "左键：平移\n滚轮：以鼠标为中心缩放\nHome：适应内容\nEsc：取消当前操作\nD"
      "elete：删除选中对"
      "象\nCtrl+N/O/S/Z/Y：新建/加载/保存/撤销/"
      "重做\nF5：应用脚本修改并重置当前画布");
}
void Application::Frame() {
  auto* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::Begin("Drawer", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings);
  auto* session = Active();
  if (ImGui::Button("新建画布")) ImGui::OpenPopup("选择用例");
  if (ImGui::BeginPopup("选择用例")) {
    for (const auto& item : cases_)
      if (ImGui::Selectable(item.name.c_str())) NewCase(item.entry);
    ImGui::EndPopup();
  }
  ImGui::SameLine();
  if (ImGui::Button("加载")) Open();
  ImGui::SameLine();
  ImGui::BeginDisabled(!session);
  if (ImGui::Button("保存")) Save(session);
  ImGui::SameLine();
  if (ImGui::Button("另存为")) Save(session, true);
  ImGui::SameLine();
  if (ImGui::Button("清空")) session->Clear();
  ImGui::SameLine();
  ImGui::BeginDisabled(!session || !session->GetHistory().CanUndo());
  if (ImGui::Button("撤销")) session->Undo();
  ImGui::EndDisabled();
  ImGui::SameLine();
  ImGui::BeginDisabled(!session || !session->GetHistory().CanRedo());
  if (ImGui::Button("重做")) session->Redo();
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button("应用脚本修改 F5")) Request(Action::kReload, active_);
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip(
        "重新读取当前用例的 Lua 脚本及模块，重置当前画布和撤销历史。\n"
        "未保存时先确认；加载失败保留原实例，其他画布不变。");
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button("更新用例列表")) {
    RefreshCases();
    notice_ = "左侧用例列表已更新，已打开的画布保持原样；应用脚本修改请按 F5。";
  }
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip(
        "重新扫描用例目录，更新左侧名称和新增/移除项。\n"
        "已打开的画布继续运行；修改当前用例的脚本后，请点击“应用脚本修改 "
        "F5”。");
  auto& io = ImGui::GetIO();
  if (!io.WantTextInput && pending_ == Action::kNone) {
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N))
      ImGui::OpenPopup("选择用例");
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) Open();
    if (session) {
      if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S))
        Save(session, io.KeyShift);
      if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
        if (io.KeyShift)
          session->Redo();
        else
          session->Undo();
      }
      if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) session->Redo();
      if (ImGui::IsKeyPressed(ImGuiKey_F5)) Request(Action::kReload, active_);
    }
  }
  ImGui::Separator();
  const float bottom = 135;
  ImGui::BeginChild("cases", {210, -bottom}, ImGuiChildFlags_Borders);
  ImGui::TextUnformatted("绘制用例");
  if (ImGui::Button("打开用例目录", {-1, 0})) OpenCaseDirectory();
  if (ImGui::Button("使用内置用例", {-1, 0})) SetCaseDirectory({});
  const auto directory_label =
      case_directory_.parent_path().filename() / case_directory_.filename();
  ImGui::TextWrapped("目录：%s", directory_label.u8string().c_str());
  if (ImGui::IsItemHovered())
    ImGui::SetTooltip("%s", case_directory_.u8string().c_str());
  ImGui::TextDisabled("同一用例可打开多份");
  for (const auto& item : cases_) {
    ImGui::PushID(item.id.c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextWrapped("%s", item.name.c_str());
    if (ImGui::Button("新建实例", {-1, 0})) NewCase(item.entry);
    ImGui::PopID();
  }
  ImGui::EndChild();
  ImGui::SameLine();
  ImGui::BeginChild("workspace", {0, -bottom});
  if (ImGui::BeginTabBar("documents", ImGuiTabBarFlags_Reorderable |
                                          ImGuiTabBarFlags_AutoSelectNewTabs)) {
    for (auto& tab : tabs_) {
      bool open = true;
      std::string label = tab.session->Name() + " " + std::to_string(tab.key) +
                          "###tab" + std::to_string(tab.key);
      ImGuiTabItemFlags flags = tab.session->Dirty()
                                    ? ImGuiTabItemFlags_UnsavedDocument
                                    : ImGuiTabItemFlags_None;
      if (tab.key == select_key_) flags |= ImGuiTabItemFlags_SetSelected;
      if (ImGui::BeginTabItem(label.c_str(), &open, flags)) {
        active_ = tab.key;
        ImGui::BeginChild("tools", {285, 0}, ImGuiChildFlags_Borders);
        Controls(tab.session.get());
        ImGui::EndChild();
        ImGui::SameLine();
        ImGui::BeginChild(
            "view", {0, 0}, ImGuiChildFlags_Borders,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        canvas_.Draw(tab.session.get(), pending_ == Action::kNone);
        ImGui::EndChild();
        ImGui::EndTabItem();
      }
      if (!open) Request(Action::kClose, tab.key);
    }
    ImGui::EndTabBar();
    select_key_ = 0;
  }
  ImGui::EndChild();
  session = Active();
  ImGui::Separator();
  ImGui::Text("X %.2f   Y %.2f   Zoom %.1f%%   %.1f FPS",
              canvas_.MouseWorld().x, canvas_.MouseWorld().y,
              session ? session->view.zoom * 100 : 100, io.Framerate);
  auto message =
      session && !session->Error().empty() ? session->Error() : error_;
  ImGui::BeginChild("diagnostics", {0, 75}, ImGuiChildFlags_Borders);
  if (session) {
    const auto path = session->Entry().u8string();
    if (ImGui::SmallButton("编辑当前脚本")) EditCurrentScript();
    if (ImGui::IsItemHovered())
      ImGui::SetTooltip(
          "用编辑器打开当前画布绑定的脚本；保存后按 F5 应用修改。");
    ImGui::SameLine();
    if (ImGui::SmallButton("复制脚本路径"))
      ImGui::SetClipboardText(path.c_str());
    ImGui::SameLine();
    ImGui::TextWrapped("当前脚本：%s", path.c_str());
  }
  if (!message.empty())
    ImGui::TextWrapped("%s", message.c_str());
  else
    ImGui::TextWrapped("%s", notice_.empty()
                                 ? "就绪 | Lua SDK v1 | 数学坐标 X 向右、Y 向上"
                                 : notice_.c_str());
  ImGui::EndChild();
  Confirm();
  ImGui::End();
}
bool Application::SmokeCheck(const std::string& case_id,
                             const std::filesystem::path& fixture) {
  if (cases_.empty()) {
    if (!error_.empty()) throw std::runtime_error(error_);
    return false;
  }
  for (const auto& info : cases_) {
    Session test(runtime_root_ / "sdk/drawer.lua", info.entry);
    if (!test.Controls({}) || !test.Draw(1)) return false;
  }
  for (const auto& info : cases_)
    if (info.id == case_id) {
      if (!Active() || Active()->Entry() != info.entry) NewCase(info.entry);
      break;
    }
  if (!Active() || Active()->Id() != case_id) return false;
  if (!fixture.empty() && !Active()->ExecuteFile(fixture))
    throw std::runtime_error(Active()->Error());
  return true;
}
}  // namespace drawer
