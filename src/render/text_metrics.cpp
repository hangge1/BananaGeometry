#include "render/text_metrics.h"

#include "imgui.h"

namespace drawer {
Point MeasureCanvasText(const std::string& text) {
  const auto size = ImGui::CalcTextSize(text.c_str(), nullptr, false);
  return {size.x, size.y};
}
}  // namespace drawer
