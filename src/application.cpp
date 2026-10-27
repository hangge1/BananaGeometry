#include "application.h"

#include <windows.h>

#include <commdlg.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <set>
#include <stdexcept>

#include "imgui.h"
#include "renderer.h"

namespace drawer {
namespace {
std::filesystem::path FileDialog(bool save) {
  wchar_t path[32768]{};
  OPENFILENAMEW dialog{};
  dialog.lStructSize = sizeof(dialog);
  dialog.hwndOwner = GetActiveWindow();
  dialog.lpstrFilter =
      L"Drawer document (*.drawer.json)\0*.drawer.json\0JSON "
      L"(*.json)\0*.json\0\0";
  dialog.lpstrFile = path;
  dialog.nMaxFile = 32768;
  dialog.lpstrDefExt = L"drawer.json";
  dialog.Flags = OFN_NOCHANGEDIR | OFN_EXPLORER | OFN_PATHMUSTEXIST |
                 (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
  if (save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog)) return path;
  if (CommDlgExtendedError())
    throw std::runtime_error("System file dialog failed");
  return {};
}
}  // namespace
Application::Application(std::filesystem::path root) : root_(std::move(root)) {
  Scan();
  for (const auto& item : cases_)
    if (item.id == "drawer.triangle") {
      New(item.entry);
      break;
    }
}
void Application::Scan() {
  cases_.clear();
  error_.clear();
  std::set<std::string> ids;
  for (const auto& entry : DiscoverCases(root_ / "cases")) {
    try {
      Session session(root_ / "sdk/drawer.lua", entry);
      if (!ids.insert(session.Id()).second)
        throw std::runtime_error("Duplicate case ID: " + session.Id());
      cases_.push_back({entry, session.Id(), session.Name()});
    } catch (const std::exception& e) {
      error_ += entry.u8string() + ": " + e.what() + "\n";
    }
  }
}
void Application::New(const std::filesystem::path& entry) {
  try {
    auto session = std::make_unique<Session>(root_ / "sdk/drawer.lua", entry);
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
    auto path = FileDialog(false);
    if (path.empty()) return;
    auto document = ReadDocument(path);
    auto found = std::find_if(
        cases_.begin(), cases_.end(),
        [&](const auto& item) { return item.id == document.case_id; });
    if (found == cases_.end())
      throw std::runtime_error("Missing case plugin: " + document.case_id +
                               ". Refresh the case list after installing it.");
    auto session =
        std::make_unique<Session>(root_ / "sdk/drawer.lua", found->entry);
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
    auto path =
        save_as || session->file.empty() ? FileDialog(true) : session->file;
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
      found->session = std::move(candidate);
      notice_ = "重新加载成功：当前实例已重置。";
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
std::vector<Value> Application::Ui(const std::string& op,
                                   const Value::Array& args) {
  auto arg = [&](size_t i) -> const Value& {
    static const Value null;
    return i < args.size() ? args[i] : null;
  };
  auto label = arg(0).StringOr("");
  bool changed = false, finished = false;
  Value value = arg(1);
  if (op == "button") {
    changed = ImGui::Button(label.c_str());
    return {changed, changed, changed};
  }
  if (op == "text")
    ImGui::TextWrapped("%s", label.c_str());
  else if (op == "same_line")
    ImGui::SameLine();
  else if (op == "separator")
    ImGui::Separator();
  else if (op == "slider" || op == "number") {
    double number = value.NumberOr(0), lower = arg(2).NumberOr(0),
           upper = arg(3).NumberOr(100);
    if (op == "slider")
      changed = ImGui::SliderScalar(label.c_str(), ImGuiDataType_Double,
                                    &number, &lower, &upper, "%.2f");
    else
      changed = ImGui::InputDouble(label.c_str(), &number, 1, 10, "%.2f");
    if (!std::isfinite(number)) number = 0;
    value = std::clamp(number, lower, upper);
    finished = ImGui::IsItemDeactivatedAfterEdit();
  } else if (op == "checkbox") {
    bool boolean = value.IsBool() && value.Bool();
    changed = ImGui::Checkbox(label.c_str(), &boolean);
    value = boolean;
    finished = changed;
  } else if (op == "input") {
    char text[4096]{};
    auto string = value.StringOr("");
    memcpy(text, string.data(), std::min(string.size(), sizeof(text) - 1));
    changed = ImGui::InputText(label.c_str(), text, sizeof(text));
    value = std::string(text);
    finished = ImGui::IsItemDeactivatedAfterEdit();
  } else if (op == "combo" && arg(2).IsArray()) {
    const auto& choices = arg(2).Items();
    int current = static_cast<int>(value.NumberOr(1)) - 1;
    const char* preview = current >= 0 &&
                                  current < static_cast<int>(choices.size()) &&
                                  choices[current].IsString()
                              ? choices[current].String().c_str()
                              : "";
    if (ImGui::BeginCombo(label.c_str(), preview)) {
      for (size_t i = 0; i < choices.size(); ++i) {
        auto text = choices[i].StringOr("");
        if (ImGui::Selectable(text.c_str(), static_cast<int>(i) == current)) {
          value = static_cast<int>(i + 1);
          changed = finished = true;
        }
      }
      ImGui::EndCombo();
    }
  } else if (op != "text" && op != "same_line" && op != "separator")
    throw std::runtime_error("Unknown UI operation: " + op);
  return {value, changed, finished};
}
void Application::Controls(Session* session) {
  ImGui::TextUnformatted("用例控件");
  ImGui::Separator();
  session->Controls([&](const auto& operation, const auto& arguments) {
    return Ui(operation, arguments);
  });
  ImGui::Spacing();
  ImGui::Separator();
  ImGui::TextUnformatted("操作历史");
  ImGui::TextWrapped("撤销：%s", session->GetHistory().UndoLabel().c_str());
  ImGui::TextWrapped("重做：%s", session->GetHistory().RedoLabel().c_str());
  ImGui::Spacing();
  ImGui::TextWrapped(
      "左键：用例交互\n中键拖动 / "
      "空格+"
      "左键：平移\n滚轮：以鼠标为中心缩放\nHome：适应内容\nEsc：取消当前操作\nD"
      "elete：删除选中对"
      "象\nCtrl+N/O/S/Z/Y：新建/加载/保存/撤销/重做\nF5：重新加载当前用例");
}
void Application::Canvas(Session* session) {
  auto origin = ImGui::GetCursorScreenPos(),
       size = ImGui::GetContentRegionAvail();
  size.x = std::max(size.x, 1.0f);
  size.y = std::max(size.y, 1.0f);
  ImGui::InvisibleButton("canvas", size,
                         ImGuiButtonFlags_MouseButtonLeft |
                             ImGuiButtonFlags_MouseButtonMiddle |
                             ImGuiButtonFlags_MouseButtonRight);
  bool hovered = ImGui::IsItemHovered();
  auto& io = ImGui::GetIO();
  Point mouse{io.MousePos.x - origin.x, io.MousePos.y - origin.y};
  bool pan = ImGui::IsMouseDown(ImGuiMouseButton_Middle) ||
             (ImGui::IsKeyDown(ImGuiKey_Space) &&
              ImGui::IsMouseDown(ImGuiMouseButton_Left));
  if ((hovered || ImGui::IsItemActive()) && pan && !session->Editing())
    session->view.Pan({io.MouseDelta.x, io.MouseDelta.y});
  if (hovered && io.MouseWheel && !session->Editing())
    session->view.ZoomAt(mouse, {size.x, size.y},
                         std::pow(1.18, io.MouseWheel));
  auto world = session->view.ToWorld(mouse, {size.x, size.y});
  if (hovered) {
    last_x_ = world.x;
    last_y_ = world.y;
  }
  auto send = [&](const char* type, const char* key = "", int button = 0) {
    session->Event(Value::Object{{"type", type},
                                 {"x", world.x},
                                 {"y", world.y},
                                 {"key", key},
                                 {"ctrl", io.KeyCtrl},
                                 {"shift", io.KeyShift},
                                 {"alt", io.KeyAlt},
                                 {"button", button}});
  };
  bool input = !io.WantTextInput && pending_ == Action::kNone;
  if (input) {
    if (hovered && !pan && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
      send("down", "", 1);
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
      send("down", "", 2);
    if ((hovered || session->Editing()) && (io.MouseDelta.x || io.MouseDelta.y))
      send("move");
    if ((hovered || session->Editing()) &&
        ImGui::IsMouseReleased(ImGuiMouseButton_Left))
      send("up", "", 1);
    if (hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
      send("up", "", 2);
    if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
      send("key", "escape");
      session->Cancel();
    }
    if (hovered && ImGui::IsKeyPressed(ImGuiKey_Delete)) send("key", "delete");
    if (hovered && !io.KeyCtrl)
      for (int code = ImGuiKey_NamedKey_BEGIN; code < ImGuiKey_NamedKey_END;
           ++code) {
        auto key = static_cast<ImGuiKey>(code);
        if (key == ImGuiKey_Escape || key == ImGuiKey_Delete ||
            key == ImGuiKey_Home || code >= ImGuiKey_MouseLeft)
          continue;
        std::string name = ImGui::GetKeyName(key);
        std::transform(
            name.begin(), name.end(), name.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (ImGui::IsKeyPressed(key, false)) send("key", name.c_str());
        if (ImGui::IsKeyReleased(key)) send("key_up", name.c_str());
      }
    if (ImGui::IsKeyPressed(ImGuiKey_Home))
      FitView(&session->view, session->State(), {size.x, size.y});
  }
  session->Draw(session->view.zoom);
  RenderCanvas(session->view, session->DrawItems(), origin, size);
  if (session->State().At("objects").Items().empty()) {
    ImGui::GetWindowDrawList()->AddText({origin.x + 20, origin.y + 18},
                                        IM_COL32(140, 160, 185, 255),
                                        "从左侧选择用例，或在画布中开始绘制。");
  }
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
      if (ImGui::Selectable(item.name.c_str())) New(item.entry);
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
  if (ImGui::Button("重载 F5")) Request(Action::kReload, active_);
  ImGui::EndDisabled();
  ImGui::SameLine();
  if (ImGui::Button("刷新用例")) Scan();
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
  ImGui::TextDisabled("同一用例可打开多份");
  for (const auto& item : cases_) {
    ImGui::PushID(item.id.c_str());
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextWrapped("%s", item.name.c_str());
    if (ImGui::Button("新建实例", {-1, 0})) New(item.entry);
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
        Canvas(tab.session.get());
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
  ImGui::Text("X %.2f   Y %.2f   Zoom %.1f%%   %.1f FPS", last_x_, last_y_,
              session ? session->view.zoom * 100 : 100, io.Framerate);
  auto message =
      session && !session->Error().empty() ? session->Error() : error_;
  ImGui::BeginChild("diagnostics", {0, 75}, ImGuiChildFlags_Borders);
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
bool Application::SmokeCheck() {
  if (cases_.size() < 3 || !Active()) return false;
  for (const auto& info : cases_) {
    Session test(root_ / "sdk/drawer.lua", info.entry);
    if (!test.Controls({}) || !test.Draw(1)) return false;
  }
  for (const auto& info : cases_)
    if (info.id == "drawer.gallery") {
      New(info.entry);
      break;
    }
  return true;
}
}  // namespace drawer
