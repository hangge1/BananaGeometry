#include "app/application.h"
#include "imgui.h"
namespace drawer {
void Application::FitSelection() {
  auto* s = Active();
  if (!s) return;
  auto state = s->State();
  const auto id = s->SelectedObject();
  if (selection_ == Selection::kObject && id) {
    for (const auto& object : s->State().At("objects").Items())
      if (object.At("id").Number() == id) {
        auto copy = object;
        copy["points"] = s->ObjectBounds(id);
        state["objects"] = Value::Array{copy};
        break;
      }
  }
  FitView(&s->view, state, canvas_size_);
}
void Application::Menus() {
  if (!ImGui::BeginMenuBar()) return;
  auto* d = ActiveDocument();
  auto* s = Active();
  auto* e = ActiveEntry();
  auto* preview = Preview();
  if (ImGui::BeginMenu("文件")) {
    if (ImGui::MenuItem("新建文档", "Ctrl+N")) Queue([this] { NewDocument(); });
    if (ImGui::MenuItem("打开…", "Ctrl+O")) Queue([this] { Open(); });
    ImGui::Separator();
    if (ImGui::MenuItem("保存整个文档", "Ctrl+S", false, d != nullptr))
      Queue([this] { Save(); });
    if (ImGui::MenuItem("另存为…", "Ctrl+Shift+S", false, d != nullptr))
      Queue([this] { Save(true); });
    ImGui::Separator();
    if (ImGui::MenuItem("关闭当前文档", nullptr, false, d != nullptr))
      Request(Action::kClose);
    if (ImGui::MenuItem("退出")) RequestExit();
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("编辑")) {
    const auto undo =
        "撤销当前实例" + (s ? "：" + s->GetHistory().UndoLabel() : "");
    const auto redo =
        "重做当前实例" + (s ? "：" + s->GetHistory().RedoLabel() : "");
    if (ImGui::MenuItem(undo.c_str(), "Ctrl+Z", false,
                        s && s->GetHistory().CanUndo()))
      Queue([this] {
        if (Active()) Active()->Undo();
      });
    if (ImGui::MenuItem(redo.c_str(), "Ctrl+Y", false,
                        s && s->GetHistory().CanRedo()))
      Queue([this] {
        if (Active()) Active()->Redo();
      });
    ImGui::Separator();
    const auto manage_undo = "撤销文档管理" + (d ? "：" + d->UndoLabel() : "");
    const auto manage_redo = "重做文档管理" + (d ? "：" + d->RedoLabel() : "");
    if (ImGui::MenuItem(manage_undo.c_str(), nullptr, false, d && d->CanUndo()))
      Queue([this] {
        if (!FinishActiveEdit()) return;
        ActiveDocument()->Undo();
        SelectDocument();
      });
    if (ImGui::MenuItem(manage_redo.c_str(), nullptr, false, d && d->CanRedo()))
      Queue([this] {
        if (!FinishActiveEdit()) return;
        ActiveDocument()->Redo();
        SelectDocument();
      });
    ImGui::Separator();
    if (ImGui::MenuItem(
            "删除所选对象", "Delete", false,
            s && !s->EditorMode().empty() && s->SelectedObject() != 0))
      Queue([this] {
        if (Active()) Active()->DeleteSelectedObject();
      });
    if (ImGui::MenuItem("清空当前实例", nullptr, false, s != nullptr))
      Queue([this] {
        if (Active()) Active()->Clear();
      });
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("用例")) {
    if (ImGui::BeginMenu("添加用例实例", !cases_.empty())) {
      for (const auto& c : cases_)
        if (ImGui::MenuItem((c.name + "###" + c.id).c_str()))
          Queue([this, path = c.entry] { NewCase(path); });
      ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("编辑当前实例脚本", nullptr, false, s != nullptr))
      Queue([this] { EditCurrentScript(); });
    if (ImGui::MenuItem("应用脚本修改并重置当前实例", "F5", false,
                        s != nullptr))
      Request(Action::kReload);
    if (ImGui::MenuItem("重试加载不可用实例", nullptr, false, e && !s))
      Queue([this] { RetryInstance(); });
    ImGui::Separator();
    if (ImGui::MenuItem("打开用例目录…"))
      Queue([this] { OpenCaseDirectory(); });
    if (ImGui::MenuItem("使用内置用例"))
      Queue([this] { SetCaseDirectory({}); });
    if (ImGui::MenuItem("更新用例列表")) Queue([this] { RefreshCases(); });
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("视图")) {
    if (ImGui::MenuItem("定位当前选择", nullptr, false, s != nullptr))
      Queue([this] { FitSelection(); });
    if (ImGui::MenuItem("适应画布内容", "Home", false, preview != nullptr))
      Queue([this] {
        if (Preview())
          FitView(&Preview()->view, Preview()->State(), canvas_size_);
      });
    if (ImGui::MenuItem("复位缩放", nullptr, false, preview != nullptr))
      Queue([this] {
        if (Preview()) Preview()->view.zoom = 1;
      });
    ImGui::Separator();
    ImGui::MenuItem("管理器", nullptr, &show_tree_);
    ImGui::MenuItem("属性", nullptr, &show_properties_);
    ImGui::MenuItem("诊断", nullptr, &show_diagnostics_);
    if (ImGui::MenuItem("恢复默认布局")) {
      show_tree_ = show_properties_ = true;
      show_diagnostics_ = false;
      tree_width_ = 245;
      properties_width_ = 325;
    }
    ImGui::EndMenu();
  }
  if (ImGui::BeginMenu("帮助")) {
    if (ImGui::MenuItem("操作说明与快捷键")) show_help_ = true;
    if (ImGui::MenuItem("关于 Drawer")) {
      show_help_ = true;
    }
    ImGui::EndMenu();
  }
  ImGui::EndMenuBar();
}
void Application::Shortcuts() {
  auto& io = ImGui::GetIO();
  if (pending_ != Action::kNone ||
      ImGui::IsPopupOpen(nullptr, ImGuiPopupFlags_AnyPopupId))
    return;
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S)) Save(io.KeyShift);
  if (io.WantTextInput) return;
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N)) NewDocument();
  if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) Open();
  auto* s = Active();
  if (s) {
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
      if (io.KeyShift)
        s->Redo();
      else
        s->Undo();
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) s->Redo();
    if (ImGui::IsKeyPressed(ImGuiKey_F5)) Request(Action::kReload);
    if (ImGui::IsKeyPressed(ImGuiKey_Delete) && !s->EditorMode().empty() &&
        s->SelectedObject() != 0)
      s->DeleteSelectedObject();
  }
}
}  // namespace drawer
