#ifndef DRAWER_SRC_RENDER_TEXT_METRICS_H_
#define DRAWER_SRC_RENDER_TEXT_METRICS_H_

#include <string>

#include "core/geometry.h"

namespace drawer {
// Requires an active ImGui frame. Zero inherits the current font size;
// explicit sizes use screen pixels, independently of the canvas zoom.
float CanvasFontSize(double requested = 0);
// Matches the font and size used by canvas text rendering.
// Includes multiline/UTF-8 text and treats '##' as literal canvas text.
Point MeasureCanvasText(const std::string& text, double font_size = 0);
}  // namespace drawer

#endif  // DRAWER_SRC_RENDER_TEXT_METRICS_H_
