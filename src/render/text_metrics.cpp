#include "render/text_metrics.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/document.h"
#include "imgui.h"

namespace drawer {
float CanvasFontSize(double requested) {
  return requested > 0 ? static_cast<float>(std::clamp(
                             requested, kMinTextFontSize, kMaxTextFontSize))
                       : ImGui::GetFontSize();
}
Point MeasureCanvasText(const std::string& text, double font_size) {
  const auto size = ImGui::GetFont()->CalcTextSizeA(
      CanvasFontSize(font_size), std::numeric_limits<float>::max(), 0,
      text.c_str());
  return {std::ceil(size.x), size.y};
}
}  // namespace drawer
