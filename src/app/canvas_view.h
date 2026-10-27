#ifndef DRAWER_SRC_APP_CANVAS_VIEW_H_
#define DRAWER_SRC_APP_CANVAS_VIEW_H_

#include "core/geometry.h"

namespace drawer {
class Session;
// Translates ImGui input into case events and composes canvas rendering.
// Borrows the session only during Draw; owns no document or undo state.
class CanvasView {
 public:
  void Draw(Session* session, bool input_enabled);
  Point MouseWorld() const { return mouse_world_; }

 private:
  Point mouse_world_;
};
}  // namespace drawer

#endif  // DRAWER_SRC_APP_CANVAS_VIEW_H_
