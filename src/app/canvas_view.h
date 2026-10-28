#ifndef DRAWER_SRC_APP_CANVAS_VIEW_H_
#define DRAWER_SRC_APP_CANVAS_VIEW_H_

#include "core/geometry.h"

namespace drawer {
class Session;
// Translates ImGui input into case events and composes canvas rendering.
// Borrows the session only during Draw; owns no document or undo state.
class CanvasView {
 public:
  // Preview allows view navigation but suppresses case events and mode UI.
  void Draw(Session* session, bool input_enabled,
            bool interaction_enabled = true);
  Point MouseWorld() const { return mouse_world_; }
  bool CrosshairVisible() const { return crosshair_visible_; }

 private:
  Point mouse_world_;
  bool crosshair_visible_ = false;
};
}  // namespace drawer

#endif  // DRAWER_SRC_APP_CANVAS_VIEW_H_
