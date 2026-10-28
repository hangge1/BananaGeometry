#include "core/bezier.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace drawer {
namespace {
void ValidateControls(const std::vector<Point>& controls) {
  if (controls.size() < 2 || controls.size() > kMaxBezierDegree + 1)
    throw std::runtime_error("Bezier requires 2..33 control points");
  for (const auto& point : controls)
    if (!std::isfinite(point.x) || !std::isfinite(point.y) ||
        std::abs(point.x) > 1e12 || std::abs(point.y) > 1e12)
      throw std::runtime_error("Invalid Bezier control point");
}
Point Interpolate(Point a, Point b, double t) {
  return {(1 - t) * a.x + t * b.x, (1 - t) * a.y + t * b.y};
}
void Subdivide(const std::vector<Point>& controls, double tolerance, int depth,
               std::vector<Point>* result) {
  bool flat = true;
  for (size_t i = 1; i + 1 < controls.size(); ++i)
    if (SegmentDistance(controls[i], controls.front(), controls.back()) >
        tolerance) {
      flat = false;
      break;
    }
  // Distance to the finite chord also catches collinear reversals/overshoot.
  if (flat || depth == 10) {
    result->push_back(controls.back());
    return;
  }
  auto work = controls;
  std::vector<Point> left{work.front()};
  std::vector<Point> right(controls.size());
  right.back() = work.back();
  for (size_t remaining = work.size() - 1; remaining > 0; --remaining) {
    for (size_t i = 0; i < remaining; ++i)
      work[i] = Interpolate(work[i], work[i + 1], .5);
    left.push_back(work.front());
    right[remaining - 1] = work[remaining - 1];
  }
  Subdivide(left, tolerance, depth + 1, result);
  Subdivide(right, tolerance, depth + 1, result);
}
}  // namespace

void ValidateBezierJoin(const std::vector<Point>& controls, bool closed,
                        bool smooth) {
  ValidateControls(controls);
  const auto start = controls.front();
  const auto end = controls.back();
  if (closed && (controls.size() < 4 || start.x != end.x || start.y != end.y))
    throw std::runtime_error(
        "Closed Bezier requires degree 3..32 and identical endpoints");
  if (!smooth) return;
  if (!closed || controls.size() < 5)
    throw std::runtime_error("Smooth Bezier requires closure and degree 4..32");
  const auto first = controls[1];
  const auto last = controls[controls.size() - 2];
  if (start.x == first.x && start.y == first.y)
    throw std::runtime_error("Smooth Bezier requires a nonzero seam tangent");
  auto mirrored = [](double anchor, double a, double b) {
    const double scale =
        std::max({1., std::abs(anchor), std::abs(a), std::abs(b)});
    const double tolerance =
        1e-9 + 8 * std::numeric_limits<double>::epsilon() * scale;
    return std::abs(b - (2 * anchor - a)) <= tolerance;
  };
  if (!mirrored(start.x, first.x, last.x) ||
      !mirrored(start.y, first.y, last.y))
    throw std::runtime_error("Smooth Bezier requires mirrored seam tangents");
}

Point BezierPoint(const std::vector<Point>& controls, double parameter) {
  ValidateControls(controls);
  if (!std::isfinite(parameter) || parameter < 0 || parameter > 1)
    throw std::runtime_error("Bezier parameter must be in [0, 1]");
  auto work = controls;
  for (size_t remaining = work.size() - 1; remaining > 0; --remaining)
    for (size_t i = 0; i < remaining; ++i)
      work[i] = Interpolate(work[i], work[i + 1], parameter);
  return work.front();
}
std::vector<Point> FlattenBezier(const std::vector<Point>& controls,
                                 double tolerance) {
  ValidateControls(controls);
  if (!std::isfinite(tolerance) || tolerance <= 0)
    throw std::runtime_error("Bezier tolerance must be finite and positive");
  std::vector<Point> result{controls.front()};
  Subdivide(controls, tolerance, 0, &result);
  return result;
}
}  // namespace drawer
