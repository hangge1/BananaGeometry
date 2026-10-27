#include "core/geometry.h"
#include <algorithm>
#include <cmath>
namespace drawer {
Point View::ToScreen(Point p, Point size) const {
  return {size.x * .5 + (p.x - center.x) * zoom,
          size.y * .5 - (p.y - center.y) * zoom};
}
Point View::ToWorld(Point p, Point size) const {
  return {center.x + (p.x - size.x * .5) / zoom,
          center.y - (p.y - size.y * .5) / zoom};
}
void View::ZoomAt(Point screen, Point size, double factor) {
  auto before = ToWorld(screen, size);
  zoom = std::clamp(zoom * factor, .0001, 10000.0);
  auto after = ToWorld(screen, size);
  center.x += before.x - after.x;
  center.y += before.y - after.y;
}
void View::Pan(Point pixels) {
  center.x -= pixels.x / zoom;
  center.y += pixels.y / zoom;
}
double Distance(Point a, Point b) { return std::hypot(a.x - b.x, a.y - b.y); }
double SegmentDistance(Point p, Point a, Point b) {
  double dx = b.x - a.x, dy = b.y - a.y;
  double denominator = dx * dx + dy * dy;
  if (denominator == 0) return Distance(p, a);
  double t =
      std::clamp(((p.x - a.x) * dx + (p.y - a.y) * dy) / denominator, 0.0, 1.0);
  return Distance(p, {a.x + t * dx, a.y + t * dy});
}

}  // namespace drawer
