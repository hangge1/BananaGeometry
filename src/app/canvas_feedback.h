#ifndef DRAWER_SRC_APP_CANVAS_FEEDBACK_H_
#define DRAWER_SRC_APP_CANVAS_FEEDBACK_H_

#include "core/geometry.h"
#include "runtime/session.h"

namespace drawer {
// Draw-list overlays only: no items, focus, IDs or document changes.
void DrawCanvasFeedback(const Session::ModeFeedback& feedback, Point origin,
                        Point size, bool panning);
void DrawCanvasCrosshair(Point position, Point origin, Point size);
}  // namespace drawer

#endif  // DRAWER_SRC_APP_CANVAS_FEEDBACK_H_
