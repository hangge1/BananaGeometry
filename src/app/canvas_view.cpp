#include "app/canvas_view.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

#include "imgui.h"
#include "app/canvas_feedback.h"
#include "render/canvas_renderer.h"
#include "render/text_metrics.h"
#include "runtime/session.h"

namespace drawer {
void CanvasView::Draw(Session* session, bool input_enabled,
                      bool interaction_enabled) {
  crosshair_visible_ = false;
  session->SetTextMeasurer(MeasureCanvasText);
  const auto origin = ImGui::GetCursorScreenPos();
  auto size = ImGui::GetContentRegionAvail();
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
  const bool input = !io.WantTextInput && input_enabled;
  const bool panning = input && !session->Editing() &&
                       (pan || (hovered && ImGui::IsKeyDown(ImGuiKey_Space)));
  if (input && (hovered || ImGui::IsItemActive()) && pan && !session->Editing())
    session->view.Pan({io.MouseDelta.x, io.MouseDelta.y});
  if (input && hovered && io.MouseWheel && !session->Editing())
    session->view.ZoomAt(mouse, {size.x, size.y},
                         std::pow(1.18, io.MouseWheel));
  auto world = session->view.ToWorld(mouse, {size.x, size.y});
  if (hovered) {
    mouse_world_.x = world.x;
    mouse_world_.y = world.y;
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
  if (input && interaction_enabled) {
    if (hovered && !pan && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
      send("down", "", 1);
    if (hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
      send("down", "", 2);
    if ((hovered || session->Editing()) &&
        (io.MouseDelta.x || io.MouseDelta.y || io.MouseWheel))
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
        const bool arrow = key == ImGuiKey_LeftArrow ||
                           key == ImGuiKey_RightArrow ||
                           key == ImGuiKey_UpArrow || key == ImGuiKey_DownArrow;
        if (ImGui::IsKeyPressed(key, arrow)) send("key", name.c_str());
        if (ImGui::IsKeyReleased(key)) send("key_up", name.c_str());
      }
  }
  if (input && ImGui::IsKeyPressed(ImGuiKey_Home))
    FitView(&session->view, session->State(), {size.x, size.y});
  session->Draw(session->view.zoom);
  RenderCanvas(session->view, session->DrawItems(), origin, size);
  const auto feedback =
      interaction_enabled
          ? session->GetModeFeedback()
          : Session::ModeFeedback{"preview", "文档预览 · 未选择交互实例",
                                  "在管理树中选择实例或对象，开始绘制或调整"};
  DrawCanvasFeedback(feedback, {origin.x, origin.y}, {size.x, size.y}, panning);
  if (input && (hovered || session->Editing())) {
    if (!interaction_enabled) {
      ImGui::SetMouseCursor(panning ? ImGuiMouseCursor_ResizeAll
                                    : ImGuiMouseCursor_Arrow);
      return;
    }
    const auto hint = session->GetPointerHint();
    if (panning || hint == Session::PointerHint::kMove ||
        feedback.cursor == "move")
      ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
    else if (hint == Session::PointerHint::kHandle || feedback.cursor == "hand")
      ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    else if (feedback.cursor == "crosshair" && hovered) {
      crosshair_visible_ = true;
      ImGui::SetMouseCursor(ImGuiMouseCursor_None);
      DrawCanvasCrosshair({io.MousePos.x, io.MousePos.y}, {origin.x, origin.y},
                          {size.x, size.y});
    } else
      ImGui::SetMouseCursor(ImGuiMouseCursor_Arrow);
    if (hovered && !ImGui::IsMouseDown(ImGuiMouseButton_Left) &&
        ImGui::IsItemHovered(ImGuiHoveredFlags_ForTooltip)) {
      if (hint == Session::PointerHint::kMove)
        ImGui::SetTooltip("拖动图形：整体平移\nShift：水平/垂直；方向键：微移");
      else if (hint == Session::PointerHint::kHandle)
        ImGui::SetTooltip("拖动控制点：修改形状\nAlt+拖动：整体平移");
    }
  }
}
}  // namespace drawer
