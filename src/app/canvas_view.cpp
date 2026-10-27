#include "app/canvas_view.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

#include "imgui.h"
#include "render/canvas_renderer.h"
#include "runtime/session.h"

namespace drawer {
void CanvasView::Draw(Session* session, bool input_enabled) {
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
  if ((hovered || ImGui::IsItemActive()) && pan && !session->Editing())
    session->view.Pan({io.MouseDelta.x, io.MouseDelta.y});
  if (hovered && io.MouseWheel && !session->Editing())
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
  bool input = !io.WantTextInput && input_enabled;
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
}  // namespace drawer
