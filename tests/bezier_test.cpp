#include <algorithm>
#include <limits>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "core/bezier.h"
#include "support/session_fixture.h"

namespace drawer {
TEST_CASE("Bezier evaluation supports linear quadratic and higher degrees",
          "[geometry][bezier]") {
  CHECK(BezierPoint({{0, 0}, {10, 20}}, .5).y == 10);
  CHECK(BezierPoint({{0, 0}, {10, 20}, {20, 0}}, .5).y == 10);
  const int degree = GENERATE(3, 5, 16, 32);
  std::vector<Point> controls;
  for (int i = 0; i <= degree; ++i)
    controls.push_back({double(i), double(i * 2)});
  CHECK(BezierPoint(controls, 0).x == 0);
  CHECK(BezierPoint(controls, 1).x == degree);
  CHECK(BezierPoint(controls, .5).x == Catch::Approx(degree * .5));
  CHECK(BezierPoint(controls, .5).y == Catch::Approx(degree));
}
TEST_CASE("Bezier subdivision bounds sampled approximation error",
          "[geometry][bezier]") {
  const int degree = GENERATE(2, 3, 7, 32);
  std::vector<Point> controls;
  for (int i = 0; i <= degree; ++i)
    controls.push_back({double(i * 10), (i % 2 ? 1. : -1.) * 100});
  const auto curve = FlattenBezier(controls, .5);
  CHECK(curve.size() <= 1025);
  for (int sample = 0; sample <= 200; ++sample) {
    const auto point = BezierPoint(controls, sample / 200.);
    double distance = std::numeric_limits<double>::max();
    for (size_t i = 1; i < curve.size(); ++i)
      distance =
          std::min(distance, SegmentDistance(point, curve[i - 1], curve[i]));
    CHECK(distance <= .500001);
  }
  const auto reversal =
      FlattenBezier({{0, 0}, {100, 0}, {-100, 0}, {0, 0}}, .5);
  CHECK(reversal.size() > 2);
}
TEST_CASE("Bezier rejects invalid controls and bounds subdivision work",
          "[geometry][bezier]") {
  CHECK_THROWS(BezierPoint({{0, 0}}, .5));
  CHECK_THROWS(BezierPoint(std::vector<Point>(34), .5));
  CHECK_THROWS(BezierPoint({{0, 0}, {1, 1}}, 2));
  CHECK_THROWS(FlattenBezier({{0, 0}, {1, 1}}, 0));
  CHECK_THROWS(
      FlattenBezier({{0, 0}, {1, std::numeric_limits<double>::infinity()}}, 1));
  CHECK(FlattenBezier({{0, 0}, {0, 1e12}, {1e12, 0}}, 1e-12).size() <= 1025);
}
TEST_CASE_METHOD(
    testing::TriangleFixture,
    "Higher degree curves round trip and use shared picking geometry",
    "[lua][bezier][document]") {
  testing::ExecuteChecked(&session, R"(
    ctx:edit('curve',function()
      local points={}
      for i=0,7 do points[#points+1]={x=i*10,y=i*20} end
      local curve=ctx:add('bezier',points)
      assert(ctx:hit(curve,ctx.math.bezier(points,0.5),0.01))
      assert(not ctx:hit(curve,{x=-100,y=100},0.01))
    end)
  )");
  CHECK(DecodeDocument(ParseJson(ToJson(EncodeDocument(session.SaveData()))))
            .state == session.State());
  auto invalid = session.State();
  invalid["objects"].Items()[0]["points"] =
      Value::Array{Value::Object{{"x", 0}, {"y", 0}}};
  CHECK_THROWS(ValidateState(invalid));
}
}  // namespace drawer
