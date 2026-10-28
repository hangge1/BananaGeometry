#include "app/canvas_feedback.h"

#include <algorithm>

#include "imgui.h"

namespace drawer {
void DrawCanvasFeedback(const Session::ModeFeedback& feedback, Point origin,
                        Point size, bool panning) {
  const auto title = panning ? "临时平移 · " + feedback.title : feedback.title;
  const auto hint = panning ? std::string(feedback.id == "preview"
                                              ? "松开中键 / 空格后继续预览"
                                              : "松开中键 / 空格后恢复当前模式")
                            : feedback.hint;
  const float wrap = std::max(1.f, std::min(420.f, float(size.x) - 44));
  const auto title_size =
      ImGui::CalcTextSize(title.c_str(), nullptr, false, wrap);
  const auto hint_size =
      ImGui::CalcTextSize(hint.c_str(), nullptr, false, wrap);
  const auto notice_size =
      feedback.notice.empty()
          ? ImVec2{}
          : ImGui::CalcTextSize(feedback.notice.c_str(), nullptr, false, wrap);
  const float width = std::max({title_size.x, hint_size.x, notice_size.x}) + 20;
  const float height = title_size.y + hint_size.y + 25 +
                       (feedback.notice.empty() ? 0 : notice_size.y + 5);
  const ImVec2 start{float(origin.x) + 12, float(origin.y) + 12};
  auto* draw = ImGui::GetWindowDrawList();
  draw->PushClipRect({float(origin.x), float(origin.y)},
                     {float(origin.x + size.x), float(origin.y + size.y)},
                     true);
  draw->AddRectFilled(start, {start.x + width, start.y + height},
                      IM_COL32(14, 22, 33, 225), 6);
  const auto accent = feedback.id == "edit" ? IM_COL32(255, 209, 64, 255)
                                            : IM_COL32(100, 195, 255, 255);
  draw->AddRect(start, {start.x + width, start.y + height}, accent, 6);
  ImVec2 text{start.x + 10, start.y + 10};
  draw->AddText(nullptr, ImGui::GetFontSize(), text, accent, title.c_str(),
                nullptr, wrap);
  text.y += title_size.y + 5;
  draw->AddText(nullptr, ImGui::GetFontSize(), text,
                IM_COL32(225, 231, 239, 255), hint.c_str(), nullptr, wrap);
  if (!feedback.notice.empty()) {
    text.y += hint_size.y + 5;
    draw->AddText(nullptr, ImGui::GetFontSize(), text,
                  IM_COL32(255, 209, 64, 255), feedback.notice.c_str(), nullptr,
                  wrap);
  }
  draw->PopClipRect();
}
void DrawCanvasCrosshair(Point position, Point origin, Point size) {
  auto* draw = ImGui::GetForegroundDrawList();
  draw->PushClipRect({float(origin.x), float(origin.y)},
                     {float(origin.x + size.x), float(origin.y + size.y)},
                     true);
  const float radius = std::max(6.f, ImGui::GetFontSize() * .45f);
  const ImVec2 left{float(position.x) - radius, float(position.y)};
  const ImVec2 right{float(position.x) + radius, float(position.y)};
  const ImVec2 top{float(position.x), float(position.y) - radius};
  const ImVec2 bottom{float(position.x), float(position.y) + radius};
  for (const auto color :
       {IM_COL32(0, 0, 0, 255), IM_COL32(245, 250, 255, 255)}) {
    const float thickness = color == IM_COL32(0, 0, 0, 255) ? 3.f : 1.f;
    draw->AddLine(left, right, color, thickness);
    draw->AddLine(top, bottom, color, thickness);
  }
  draw->PopClipRect();
}
}  // namespace drawer
