#ifndef DRAWER_SRC_RENDER_TEXT_METRICS_H_
#define DRAWER_SRC_RENDER_TEXT_METRICS_H_

#include <string>

#include "core/geometry.h"

namespace drawer {
// Requires an active ImGui frame; matches AddText's current font and size.
// Includes multiline/UTF-8 text and treats '##' as literal canvas text.
Point MeasureCanvasText(const std::string& text);
}  // namespace drawer

#endif  // DRAWER_SRC_RENDER_TEXT_METRICS_H_
