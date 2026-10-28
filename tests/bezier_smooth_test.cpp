#include <cmath>
#include <limits>
#include <string>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "core/bezier.h"
#include "support/session_fixture.h"

namespace drawer {
TEST_CASE("Smooth Bezier joins have equal evaluated endpoint derivatives",
          "[geometry][bezier][smooth]") {
  const int degree = GENERATE(4, 5, 32);
  std::vector<Point> controls(degree + 1, {11, 180});
  controls.front() = controls.back() = {11, 22};
  controls[1] = {35, 42};
  controls[degree - 1] = {-13, 2};
  CHECK_NOTHROW(ValidateBezierJoin(controls, true, true));
  constexpr double kStep = 1e-7;
  const auto start = BezierPoint(controls, 0);
  const auto after = BezierPoint(controls, kStep);
  const auto before = BezierPoint(controls, 1 - kStep);
  const auto end = BezierPoint(controls, 1);
  CHECK((after.x - start.x) / kStep ==
        Catch::Approx((end.x - before.x) / kStep).margin(.02));
  CHECK((after.y - start.y) / kStep ==
        Catch::Approx((end.y - before.y) / kStep).margin(.02));
  const auto flattened = FlattenBezier(controls, .5);
  CHECK(flattened.front().x == flattened.back().x);
  CHECK(flattened.front().y == flattened.back().y);
}

TEST_CASE(
    "Smooth join validation bounds rounding without admitting invalid tangents",
    "[geometry][bezier][smooth]") {
  std::vector<Point> controls{{0, 0}, {120, 0}, {0, 180}, {-120, 0}, {0, 0}};
  SECTION("Open smooth curve") {
    CHECK_THROWS(ValidateBezierJoin(controls, false, true));
  }
  SECTION("Cubic join") {
    controls.erase(controls.begin() + 2);
    CHECK_THROWS(ValidateBezierJoin(controls, true, true));
  }
  SECTION("Broken symmetry") {
    controls[3].y = 1;
    CHECK_THROWS(ValidateBezierJoin(controls, true, true));
  }
  SECTION("Zero tangent") {
    controls[1] = controls[3] = controls[0];
    CHECK_THROWS(ValidateBezierJoin(controls, true, true));
  }
  SECTION("One rounding step at large coordinates") {
    for (auto& point : controls) point.x += 1e11;
    controls[3].x =
        std::nextafter(controls[3].x, std::numeric_limits<double>::infinity());
    CHECK_NOTHROW(ValidateBezierJoin(controls, true, true));
    controls[3].x += 1;
    CHECK_THROWS(ValidateBezierJoin(controls, true, true));
  }
}

}  // namespace drawer
