#include "app/application.h"
#include <algorithm>
#include "imgui.h"
namespace drawer {
Value Application::Layout() const {
  return Value::Object{{"tree", show_tree_},
                       {"properties", show_properties_},
                       {"diagnostics", show_diagnostics_},
                       {"tree_width", double(tree_width_)},
                       {"properties_width", double(properties_width_)}};
}
void Application::RememberLayout() {
  auto layout = Layout();
  if (layout == saved_layout_ || ImGui::IsMouseDown(ImGuiMouseButton_Left))
    return;
  saved_layout_ = layout;
  if (desktop_.save_layout) {
    try {
      desktop_.save_layout(layout);
    } catch (const std::exception& e) {
      ReportError(e.what());
    }
  }
}
namespace {
void Splitter(const char* id, float height, float* width, bool right) {
  ImGui::InvisibleButton(id, {6, std::max(1.f, height)});
  if (ImGui::IsItemHovered() || ImGui::IsItemActive())
    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
  if (ImGui::IsItemActive())
    *width += ImGui::GetIO().MouseDelta.x * (right ? -1.f : 1.f);
}
}  // namespace
void Application::Frame() {
  auto* viewport = ImGui::GetMainViewport();
  ImGui::SetNextWindowPos(viewport->WorkPos);
  ImGui::SetNextWindowSize(viewport->WorkSize);
  ImGui::Begin("Drawer", nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar);
  Menus();
  Shortcuts();
  if (ImGui::BeginTabBar("documents", ImGuiTabBarFlags_Reorderable)) {
    const int requested = select_key_;
    for (const auto& tab : tabs_) {
      bool open = true;
      const auto label =
          tab.document->Name() + "###document" + std::to_string(tab.key);
      ImGuiTabItemFlags flags = tab.document->Dirty()
                                    ? ImGuiTabItemFlags_UnsavedDocument
                                    : ImGuiTabItemFlags_None;
      if (tab.key == select_key_) flags |= ImGuiTabItemFlags_SetSelected;
      if (ImGui::BeginTabItem(label.c_str(), &open, flags)) {
        if (active_ != tab.key && (!requested || requested == tab.key))
          Activate(tab.key, 0);
        ImGui::EndTabItem();
      }
      if (!open) Request(Action::kClose, tab.key);
    }
    ImGui::EndTabBar();
    select_key_ = 0;
  }
  const float width = ImGui::GetContentRegionAvail().x;
  const float height = std::max(80.f, ImGui::GetContentRegionAvail().y -
                                          (show_diagnostics_ ? 150.f : 50.f));
  tree_width_ = std::clamp(tree_width_, 150.f, std::max(150.f, width * .32f));
  properties_width_ =
      std::clamp(properties_width_, 220.f, std::max(220.f, width * .4f));
  if (show_tree_) {
    ImGui::BeginChild("manager", {tree_width_, height},
                      ImGuiChildFlags_Borders);
    Tree();
    ImGui::EndChild();
    ImGui::SameLine();
    Splitter("tree-split", height, &tree_width_, false);
    ImGui::SameLine();
  }
  const float remaining = ImGui::GetContentRegionAvail().x;
  const float canvas_width = std::max(
      40.f, remaining - (show_properties_ ? properties_width_ + 22 : 0));
  ImGui::BeginChild(
      "view", {canvas_width, height}, ImGuiChildFlags_Borders,
      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
  auto size = ImGui::GetContentRegionAvail();
  canvas_size_ = {size.x, size.y};
  auto* s = Preview();
  if (s) {
    canvas_.Draw(s, pending_ == Action::kNone, Active() != nullptr);
    if (Active() && s->SelectedObject() != 0)
      selection_ = Selection::kObject;
    else if (selection_ == Selection::kObject)
      selection_ = Selection::kInstance;
  } else {
    ImGui::TextWrapped(
        "%s", PreviewEntry()
                  ? "此实例暂不可用，请在属性面板查看原因并重试加载。"
                  : "用例 → 添加用例实例，开始绘制。每个实例保留独立画布。");
  }
  ImGui::EndChild();
  if (show_properties_) {
    ImGui::SameLine();
    Splitter("properties-split", height, &properties_width_, true);
    ImGui::SameLine();
    ImGui::BeginChild("inspector", {0, height}, ImGuiChildFlags_Borders);
    Inspector();
    ImGui::EndChild();
  }
  ImGui::Separator();
  auto* e = PreviewEntry();
  s = Preview();
  const auto target = Active()        ? "交互实例：" + e->name
                      : ActiveEntry() ? "实例不可用：" + e->name
                      : e             ? "未选择交互实例 · 预览：" + e->name
                                      : std::string("无活动实例");
  ImGui::Text("%s | X %.2f  Y %.2f | %.1f%%", target.c_str(),
              canvas_.MouseWorld().x, canvas_.MouseWorld().y,
              s ? s->view.zoom * 100 : 100);
  auto message = s && !s->Error().empty() ? s->Error() : error_;
  if (!message.empty()) {
    ImGui::SameLine();
    ImGui::TextColored({1, .5f, .4f, 1}, "错误（视图 → 诊断）");
  }
  if (show_diagnostics_) {
    ImGui::BeginChild("diagnostics", {0, 0}, ImGuiChildFlags_Borders);
    ImGui::TextWrapped("%s",
                       message.empty() ? notice_.c_str() : message.c_str());
    ImGui::TextWrapped("用例目录：%s", case_directory_.u8string().c_str());
    if (s) ImGui::TextWrapped("当前脚本：%s", s->Entry().u8string().c_str());
    ImGui::EndChild();
  }
  if (show_help_) {
    ImGui::SetNextWindowSize({510, 350}, ImGuiCond_FirstUseEver);
    if (ImGui::Begin("操作说明与快捷键", &show_help_)) {
      ImGui::TextWrapped(
          "Drawer · "
          "二维交互测试工具\n\n文档节点仅预览，选择实例或对象后启用交互。\n"
          "左树双击对象定位。\n对象属"
          "性及用例工具在右侧，可通过视图菜单隐藏。\nCtrl+N/O/S：新建/打开/"
          "保存整个文档\nCtrl+Z/Y：当前实例撤销/"
          "重做\nDelete：删除对象；Home：适应内容\n中键 / "
          "空格+"
          "左键：平移；滚轮缩放\nF5：重置当前实例；文档管理撤销位于编辑菜单。");
    }
    ImGui::End();
  }
  ImGui::End();
  RememberLayout();
  if (queued_) {
    auto action = std::move(queued_);
    queued_ = {};
    try {
      action();
    } catch (const std::exception& error) {
      ReportError(error.what());
    }
  }
  // Modal IDs belong to the root window, so confirmation runs in its scope.
  ImGui::Begin("Drawer");
  Confirm();
  ImGui::End();
}
}  // namespace drawer
