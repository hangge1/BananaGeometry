#pragma once

#include "document.h"
#include "imgui.h"

namespace drawer {
void RenderCanvas(const View& view, const std::vector<Value>& items,
                  ImVec2 origin, ImVec2 size);
void FitView(View* view, const Value& state, Point size);
}  // namespace drawer
