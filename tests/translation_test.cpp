#include <array>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "support/gallery_fixture.h"

namespace drawer {
namespace {
using testing::ExecuteChecked;
using testing::GalleryFixture;
using testing::PointerEvent;
Value Drag(const char* type, Point point, bool shift = false, bool alt = false,
           int button = 1) {
  auto event = PointerEvent(type, point.x, point.y);
  event["shift"] = shift;
  event["alt"] = alt;
  event["button"] = button;
  return event;
}
Value Translated(Value state, size_t index, Point delta) {
  for (auto& point : state["objects"].Items()[index]["points"].Items()) {
    point["x"] = point.At("x").Number() + delta.x;
    point["y"] = point.At("y").Number() + delta.y;
  }
  return state;
}
TEST_CASE_METHOD(
    GalleryFixture,
    "Primitive bodies translate without reshaping or changing style",
    "[lua][translation]") {
  const size_t index = GENERATE(size_t(0), 1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u);
  const std::array<Point, 9> positions{{{-220, 125},
                                        {-30, 100},
                                        {160, 100},
                                        {-200, -80},
                                        {-40, -80},
                                        {175, -75},
                                        {120, -110},
                                        {-10, 200},
                                        {15, 0}}};
  session.SetTextMeasurer([](const std::string&) { return Point{120, 20}; });
  const auto before = session.State();
  const auto origin = positions[index];
  REQUIRE(session.Event(Drag("down", origin, false, index == 6)));
  REQUIRE(session.GetPointerHint() == Session::PointerHint::kMove);
  REQUIRE(session.Event(Drag("move", {origin.x + 20, origin.y - 15})));
  REQUIRE(session.Event(Drag("up", {origin.x + 40, origin.y - 30})));
  CHECK(session.State() == Translated(before, index, {40, -30}));
  CHECK(session.GetHistory().Count() == 1);
  const auto moved = session.State();
  session.Undo();
  CHECK(session.State() == before);
  session.Redo();
  CHECK(session.State() == moved);
}
TEST_CASE_METHOD(
    GalleryFixture,
    "Alt overrides grips and Shift constrains from the original geometry",
    "[lua][translation]") {
  const auto before = session.State();
  REQUIRE(session.Event(Drag("down", {-220, 100}, false, true)));
  REQUIRE(session.Event(Drag("move", {-200, 112}, true)));
  REQUIRE(session.Event(Drag("move", {-200, 112}, false)));
  REQUIRE(session.Event(Drag("up", {-215, 130}, true)));
  CHECK(session.State() == Translated(before, 0, {0, 30}));
  CHECK(session.GetHistory().Count() == 1);
}
TEST_CASE_METHOD(GalleryFixture,
                 "Body clicks and subthreshold jitter leave the document clean",
                 "[lua][translation]") {
  const double zoom = GENERATE(.5, 1., 4.);
  session.view.zoom = zoom;
  const auto before = session.State();
  REQUIRE(session.Event(Drag("down", {160, 100})));
  REQUIRE(session.Event(Drag("move", {160 + 2 / zoom, 100})));
  REQUIRE(session.Event(Drag("up", {160 + 2 / zoom, 100})));
  CHECK(session.State() == before);
  CHECK_FALSE(session.Dirty());
  CHECK(session.GetHistory().Count() == 0);
}
TEST_CASE_METHOD(
    GalleryFixture,
    "Unfilled interiors are grab areas and translation cancels atomically",
    "[lua][translation]") {
  ExecuteChecked(
      &session,
      "ctx:edit('outline',function()ctx.state.objects[3].fill=false end)");
  const auto before = session.State();
  REQUIRE(session.Event(Drag("down", {160, 100})));
  REQUIRE(session.Event(Drag("move", {200, 130})));
  REQUIRE(session.Event(Drag("up", {200, 130}, false, false, 2)));
  REQUIRE(session.Editing());
  REQUIRE(session.Event(PointerEvent("key", 200, 130, "escape")));
  CHECK(session.State() == before);
  CHECK(session.GetHistory().Count() == 1);
  CHECK(session.GetPointerHint() == Session::PointerHint::kNone);
}
TEST_CASE_METHOD(GalleryFixture,
                 "Arrow nudges move the selected object in screen pixel steps",
                 "[lua][translation]") {
  session.view.zoom = 2;
  const auto before = session.State();
  REQUIRE(session.Event(Drag("down", {160, 100})));
  REQUIRE(session.Event(Drag("up", {160, 100})));
  REQUIRE(session.Event(PointerEvent("key", 160, 100, "rightarrow")));
  auto event = PointerEvent("key", 160, 100, "uparrow");
  event["shift"] = true;
  REQUIRE(session.Event(event));
  CHECK(session.State() == Translated(before, 2, {.5, 5}));
  CHECK(session.GetHistory().Count() == 2);
  session.Undo();
  session.Undo();
  CHECK(session.State() == before);
}
TEST_CASE_METHOD(
    GalleryFixture,
    "Custom translation uses an immutable source and rolls back failures",
    "[lua][translation][custom]") {
  ExecuteChecked(&session, R"(
    ctx:register_type('origin_shape',{
      hit=function(_,o,p) return math.abs(p.x-o.origin.x)<20 and math.abs(p.y-o.origin.y)<20 end,
      translate=function(_,o,d,s)
        o.origin={x=s.origin.x+d.x,y=s.origin.y+d.y}
        if d.x>100 then error('translation rejected') end
      end})
    ctx:edit('custom',function()
      ctx.state.objects=ctx.array()
      ctx:add('origin_shape',ctx.array(),{origin={x=0,y=0},label='preserve'})
    end)
  )");
  REQUIRE(session.Event(Drag("down", {5, 5})));
  REQUIRE(session.Event(Drag("move", {15, 15})));
  REQUIRE(session.Event(Drag("up", {25, 40})));
  CHECK(session.State().At("objects").Items()[0].At("origin") ==
        Value::Object{{"x", 20}, {"y", 35}});
  const auto before = session.State();
  REQUIRE(session.Event(Drag("down", {25, 40})));
  CHECK_FALSE(session.Event(Drag("move", {200, 40})));
  CHECK(session.State() == before);
  CHECK_FALSE(session.Editing());
  CHECK(session.GetHistory().Count() == 2);
}
TEST_CASE_METHOD(GalleryFixture,
                 "Pointer feedback distinguishes bodies grips and empty space",
                 "[lua][translation]") {
  REQUIRE(session.Event(Drag("move", {160, 100})));
  CHECK(session.GetPointerHint() == Session::PointerHint::kMove);
  REQUIRE(session.Event(Drag("move", {100, 60})));
  CHECK(session.GetPointerHint() == Session::PointerHint::kHandle);
  REQUIRE(session.Event(Drag("move", {500, 500})));
  CHECK(session.GetPointerHint() == Session::PointerHint::kNone);
  CHECK_FALSE(session.Dirty());
}
}  // namespace
}  // namespace drawer
