#ifndef DRAWER_SRC_CORE_BEZIER_H_
#define DRAWER_SRC_CORE_BEZIER_H_

#include <vector>

#include "core/geometry.h"

namespace drawer {
inline constexpr int kMaxBezierDegree = 32;
// Validates closure and optional C1 mirror constraints, allowing double
// rounding. Smooth joins require degree >= 4 and a nonzero tangent; throws on
// invalid data.
void ValidateBezierJoin(const std::vector<Point>& controls, bool closed,
                        bool smooth);
// De Casteljau evaluation for 2..33 finite control points and t in [0, 1].
Point BezierPoint(const std::vector<Point>& controls, double parameter);
// Shared render/pick approximation in world units. Subdivision is capped at
// 1024 segments to bound work for extreme coordinates or zoom levels.
std::vector<Point> FlattenBezier(const std::vector<Point>& controls,
                                 double tolerance);
}  // namespace drawer

#endif  // DRAWER_SRC_CORE_BEZIER_H_
