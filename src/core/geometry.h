#ifndef DRAWER_SRC_CORE_GEOMETRY_H_
#define DRAWER_SRC_CORE_GEOMETRY_H_

namespace drawer {
struct Point {
  double x = 0;
  double y = 0;
};
// Maps mathematical world coordinates (Y up) to canvas-local pixels (Y down).
// The canvas origin is its upper-left corner; zoom is pixels per world unit.
struct View {
  Point center;
  double zoom = 1;
  Point ToScreen(Point world, Point size) const;
  Point ToWorld(Point screen, Point size) const;
  void ZoomAt(Point screen, Point size, double factor);
  void Pan(Point pixels);
};
double Distance(Point a, Point b);
double SegmentDistance(Point point, Point a, Point b);

}  // namespace drawer
#endif  // DRAWER_SRC_CORE_GEOMETRY_H_
