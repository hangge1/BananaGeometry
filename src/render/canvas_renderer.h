#ifndef DRAWER_SRC_RENDER_CANVAS_RENDERER_H_
#define DRAWER_SRC_RENDER_CANVAS_RENDERER_H_

#include "core/document.h"
#include "imgui.h"

namespace drawer {
// Appends canvas-local draw data to the current ImGui window. The host supplies
// the screen origin; world coordinates use the same View as hit testing.
void RenderCanvas(const View& view, const std::vector<Value>& items,
                  ImVec2 origin, ImVec2 size);
}  // namespace drawer

#endif  // DRAWER_SRC_RENDER_CANVAS_RENDERER_H_
