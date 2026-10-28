#include <cmath>
#include <limits>
#include <string>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "core/bezier.h"
#include "support/session_fixture.h"

namespace drawer {
namespace {
constexpr auto kNewSmooth = "平滑接点##new_bezier";
constexpr auto kSelectedSmooth = "平滑接点##selected_bezier";

std::vector<Value> Unchanged(const std::string& operation,
                             const Value::Array& args) {
  return {operation == "button" || operation == "mode_button" ? Value(false)
          : args.size() > 1                                   ? args[1]
                                                              : Value(),
          false, false};
}

void Control(Session* session, const std::string& label, const Value& value) {
  bool found = false;
  const bool success = session->Controls(
      [&](const std::string& operation, const Value::Array& args) {
        if (!args.empty() && args[0].StringOr("") == label) {
          found = true;
          return std::vector<Value>{value, true, true};
        }
        return Unchanged(operation, args);
      });
  INFO(session->Error());
  REQUIRE(success);
  REQUIRE(found);
}

struct SmoothFixture {
  Session session{testing::SourceRoot() / "sdk/drawer.lua",
                  testing::SourceRoot() / "cases/bezier/main.lua"};
  void Prepare() {
    Control(&session, "曲线形式", 2);
    Control(&session, kNewSmooth, true);
    REQUIRE(session.Event(testing::PointerEvent("down", 0, 0)));
    REQUIRE(session.Event(testing::PointerEvent("down", 120, 0)));
    REQUIRE(session.Event(testing::PointerEvent("down", 0, 180)));
    testing::ExecuteChecked(
        &session,
        "ctx:set_mode('edit');ctx.temp.selected=ctx.state.objects[1].id");
  }
};
}  // namespace

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

TEST_CASE(
    "Bezier and Gallery create smooth curves using only independent controls",
    "[lua][bezier][smooth][cases]") {
  const auto directory =
      GENERATE(std::string("bezier"), std::string("gallery"));
  const int degree = GENERATE(4, 6, 32);
  Session session(testing::SourceRoot() / "sdk/drawer.lua",
                  testing::SourceRoot() / "cases" / directory / "main.lua");
  if (directory == "gallery")
    testing::ExecuteChecked(
        &session,
        "ctx:edit('tool',function()ctx.state.params.kind='bezier'end)");
  Control(&session, "曲线形式", 2);
  Control(&session, kNewSmooth, true);
  Control(&session, "阶数", degree);
  const auto steps = session.GetHistory().Count();
  for (int i = 0; i < degree - 2; ++i) {
    REQUIRE(
        session.Event(testing::PointerEvent("down", i * 20, i % 2 ? 80 : 0)));
    CHECK(session.State().At("objects").Items().empty());
  }
  CHECK(session.GetModeFeedback().title.find(
            std::to_string(degree) + " 阶平滑闭合") != std::string::npos);
  CHECK(session.GetModeFeedback().hint.find(std::to_string(degree - 2) + "/" +
                                            std::to_string(degree - 1)) !=
        std::string::npos);
  REQUIRE(session.Draw(1));
  REQUIRE(session.Event(testing::PointerEvent("down", 50, 200)));
  REQUIRE(session.State().At("objects").Items().size() == 1);
  const auto& object = session.State().At("objects").Items()[0];
  CHECK(object.At("closed").Bool());
  CHECK(object.At("smooth").Bool());
  CHECK(object.At("points").Items().size() == degree + 1);
  CHECK(session.GetHistory().Count() == steps + 1);
  testing::ExecuteChecked(&session, R"(
    local o=ctx.state.objects[1]
    local p=o.points
    assert(p[1].x==p[#p].x and p[1].y==p[#p].y)
    assert(p[2].x+p[#p-1].x==2*p[1].x)
    assert(p[2].y+p[#p-1].y==2*p[1].y)
    assert(ctx:hit(o,ctx.math.bezier(p,.5),1))
  )");
  Session copy(testing::SourceRoot() / "sdk/drawer.lua",
               testing::SourceRoot() / "cases" / directory / "main.lua");
  copy.Restore(
      DecodeDocument(ParseJson(ToJson(EncodeDocument(session.SaveData())))));
  CHECK(copy.State() == session.State());
  CHECK_FALSE(copy.Dirty());
  session.Undo();
  CHECK(session.State().At("objects").Items().empty());
  session.Redo();
  CHECK(session.State() == copy.State());
}

TEST_CASE_METHOD(
    SmoothFixture,
    "New smooth settings raise degree in one edit and cancel previews",
    "[lua][bezier][smooth][ui]") {
  Control(&session, "曲线形式", 2);
  REQUIRE(session.Event(testing::PointerEvent("down", 0, 0)));
  const auto before = session.State();
  const auto steps = session.GetHistory().Count();
  Control(&session, kNewSmooth, true);
  CHECK(session.State().At("params").At("degree").Number() == 4);
  CHECK(session.GetHistory().Count() == steps + 1);
  CHECK(session.GetModeFeedback().notice.find("未完成") != std::string::npos);
  testing::ExecuteChecked(&session, "assert(ctx:drawing_count()==0)");
  session.Undo();
  CHECK(session.State() == before);
  session.Redo();
  Control(&session, "曲线形式", 1);
  CHECK_FALSE(session.State().At("params").At("smooth").Bool());
}

TEST_CASE_METHOD(SmoothFixture,
                 "Selected smoothing is independent of new curve preferences",
                 "[lua][bezier][smooth][ui]") {
  Prepare();
  const auto smooth = session.State();
  Control(&session, kSelectedSmooth, false);
  const auto plain = session.State();
  CHECK(plain.At("objects").Items()[0].At("points") ==
        smooth.At("objects").Items()[0].At("points"));
  CHECK_FALSE(plain.At("objects").Items()[0].At("smooth").Bool());
  CHECK(plain.At("params").At("smooth").Bool());
  Control(&session, kSelectedSmooth, true);
  CHECK(session.State() == smooth);
  Control(&session, "断开接点", true);
  CHECK_FALSE(session.State().At("objects").Items()[0].At("closed").Bool());
  CHECK_FALSE(session.State().At("objects").Items()[0].At("smooth").Bool());
  CHECK(session.State().At("objects").Items()[0].At("points") ==
        smooth.At("objects").Items()[0].At("points"));
  session.Undo();
  CHECK(session.State() == smooth);
}

TEST_CASE_METHOD(
    SmoothFixture,
    "Smooth seam and tangent drags use one reversible source snapshot",
    "[lua][bezier][smooth][interaction]") {
  const int index = GENERATE(1, 2, 3, 4);
  const bool cancel = GENERATE(false, true);
  Prepare();
  const auto before = session.State();
  const auto steps = session.GetHistory().Count();
  const auto& original = before.At("objects").Items()[0].At("points").Items();
  const auto x = original[index - 1].At("x").Number();
  const auto y = original[index - 1].At("y").Number();
  REQUIRE(session.Event(testing::PointerEvent("down", x, y)));
  REQUIRE(session.Event(testing::PointerEvent("move", x + 10, y + 10)));
  REQUIRE(session.Event(testing::PointerEvent("move", x + 30, y + 20)));
  if (cancel) {
    REQUIRE(session.Event(testing::PointerEvent("key", 0, 0, "escape")));
    CHECK(session.State() == before);
    CHECK(session.GetHistory().Count() == steps);
    return;
  }
  REQUIRE(session.Event(testing::PointerEvent("up", x + 30, y + 20)));
  const auto moved = session.State();
  const auto& points = moved.At("objects").Items()[0].At("points").Items();
  CHECK(points[index - 1].At("x").Number() == x + 30);
  CHECK(points[index - 1].At("y").Number() == y + 20);
  CHECK(points.front() == points.back());
  CHECK(points[1].At("x").Number() + points[3].At("x").Number() ==
        2 * points[0].At("x").Number());
  CHECK(points[1].At("y").Number() + points[3].At("y").Number() ==
        2 * points[0].At("y").Number());
  if (index == 1) {
    CHECK(points[1].At("x").Number() == 150);
    CHECK(points[3].At("x").Number() == -90);
    CHECK(points[2] == original[2]);
  } else {
    CHECK(points[0] == original[0]);
  }
  CHECK(session.GetHistory().Count() == steps + 1);
  session.Undo();
  CHECK(session.State() == before);
  session.Redo();
  CHECK(session.State() == moved);
}

TEST_CASE_METHOD(
    SmoothFixture,
    "Zero tangent input keeps construction and the last valid drag",
    "[lua][bezier][smooth][interaction]") {
  Control(&session, "曲线形式", 2);
  Control(&session, kNewSmooth, true);
  REQUIRE(session.Event(testing::PointerEvent("down", 0, 0)));
  REQUIRE(session.Event(testing::PointerEvent("down", 0, 0)));
  testing::ExecuteChecked(&session, "assert(ctx:drawing_count()==1)");
  CHECK(session.GetModeFeedback().notice.find("第二点") != std::string::npos);
  REQUIRE(session.Event(testing::PointerEvent("down", 120, 0)));
  REQUIRE(session.Event(testing::PointerEvent("down", 0, 180)));
  testing::ExecuteChecked(
      &session,
      "ctx:set_mode('edit');ctx.temp.selected=ctx.state.objects[1].id");
  REQUIRE(session.Event(testing::PointerEvent("down", 120, 0)));
  REQUIRE(session.Event(testing::PointerEvent("move", 150, 20)));
  REQUIRE(session.Event(testing::PointerEvent("move", 0, 0)));
  REQUIRE(session.Event(testing::PointerEvent("up", 0, 0)));
  CHECK(session.State()
            .At("objects")
            .Items()[0]
            .At("points")
            .Items()[1]
            .At("x")
            .Number() == 150);
  CHECK(session.State()
            .At("objects")
            .Items()[0]
            .At("points")
            .Items()[1]
            .At("y")
            .Number() == 20);
}

TEST_CASE_METHOD(SmoothFixture,
                 "Existing smooth conversion preserves a usable tangent and "
                 "rejects cubic joins",
                 "[lua][bezier][smooth][sdk]") {
  const bool use_incoming = GENERATE(false, true);
  testing::ExecuteChecked(&session, use_incoming ? R"(
    ctx:edit('plain',function()ctx:add('bezier',{{x=0,y=0},{x=0,y=0},{x=0,y=180},{x=-80,y=-20},{x=0,y=0}},{closed=true})end)
  )"
                                                 : R"(
    ctx:edit('plain',function()ctx:add('bezier',{{x=0,y=0},{x=80,y=20},{x=0,y=180},{x=-10,y=10},{x=0,y=0}},{closed=true})end)
  )");
  testing::ExecuteChecked(&session, R"(
    local o=ctx.state.objects[1]
    ctx:edit('smooth',function()ctx:set_bezier_smooth(o,true)end)
    assert(o.points[2].x==80 and o.points[2].y==20)
    assert(o.points[4].x==-80 and o.points[4].y==-20)
  )");
  const auto before = session.State();
  CHECK_FALSE(session.Execute(R"(
    ctx:edit('invalid',function()
      local o=ctx:add('bezier',{{x=0,y=0},{x=100,y=0},{x=-100,y=0},{x=0,y=0}},{closed=true})
      ctx:set_bezier_smooth(o,true)
    end)
  )"));
  CHECK(session.State() == before);
}

TEST_CASE_METHOD(
    SmoothFixture,
    "Smooth translation and loaded editing preserve C1 and canvas isolation",
    "[lua][bezier][smooth][document]") {
  Prepare();
  const auto before = session.State();
  testing::ExecuteChecked(&session, R"(
    local o=ctx.state.objects[1]
    local original=ctx.copy(o)
    ctx:edit('translate',function()
      ctx:translate_object(o,{x=10,y=20},original)
      ctx:translate_object(o,{x=30,y=40},original)
    end)
  )");
  const auto& original = before.At("objects").Items()[0].At("points").Items();
  const auto& moved =
      session.State().At("objects").Items()[0].At("points").Items();
  for (size_t i = 0; i < original.size(); ++i) {
    CHECK(moved[i].At("x").Number() == original[i].At("x").Number() + 30);
    CHECK(moved[i].At("y").Number() == original[i].At("y").Number() + 40);
  }
  Session copy(testing::SourceRoot() / "sdk/drawer.lua",
               testing::SourceRoot() / "cases/bezier/main.lua");
  copy.Restore(
      DecodeDocument(ParseJson(ToJson(EncodeDocument(session.SaveData())))));
  testing::ExecuteChecked(&copy, R"(
    local o=ctx.state.objects[1]
    ctx:edit('reshape',function()ctx:move_handle(o,2,{x=170,y=60})end)
    assert(o.points[4].x==-110 and o.points[4].y==20)
  )");
  CHECK(copy.State() != session.State());
  copy.Undo();
  CHECK(copy.State() == session.State());
}

TEST_CASE_METHOD(
    SmoothFixture,
    "Malformed smooth documents fail without replacing active geometry",
    "[lua][bezier][smooth][document]") {
  Prepare();
  const auto before = session.State();
  auto data = session.SaveData();
  auto& object = data.state["objects"].Items()[0];
  SECTION("Non-boolean smooth flag") { object["smooth"] = "yes"; }
  SECTION("Open smooth geometry") { object["closed"] = false; }
  SECTION("Broken symmetry") { object["points"].Items()[3]["y"] = 10; }
  SECTION("Zero tangent") {
    object["points"].Items()[1] = object["points"].Items()[0];
    object["points"].Items()[3] = object["points"].Items()[0];
  }
  CHECK_THROWS(EncodeDocument(data));
  CHECK_THROWS(session.Restore(data));
  CHECK(session.State() == before);
}

TEST_CASE_METHOD(
    SmoothFixture,
    "Legacy ordinary closure stays unconstrained and has no smooth preference",
    "[lua][bezier][smooth][document]") {
  testing::ExecuteChecked(&session, R"(
    ctx:edit('legacy',function()
      ctx:add('bezier',{{x=0,y=0},{x=80,y=0},{x=0,y=180},{x=-10,y=0},{x=0,y=0}},{closed=true})
    end)
  )");
  auto legacy = session.SaveData();
  legacy.state["params"].Fields().erase("smooth");
  session.Restore(legacy);
  CHECK_FALSE(session.State().At("params").At("smooth").Bool());
  CHECK(session.State().At("objects").Items()[0].At("smooth").IsNull());
  CHECK_FALSE(session.Dirty());
  testing::ExecuteChecked(&session, R"(
    local o=ctx.state.objects[1]
    ctx:edit('independent',function()ctx:move_handle(o,2,{x=90,y=30})end)
    assert(o.points[4].x==-10 and o.points[4].y==0)
  )");
}
}  // namespace drawer
