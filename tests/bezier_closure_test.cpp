#include <algorithm>
#include <string>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "support/session_fixture.h"

namespace drawer {
namespace {
std::vector<Value> UnchangedControl(const std::string& operation,
                                    const Value::Array& arguments) {
  return {operation == "button" || operation == "mode_button"
              ? Value(false)
              : (arguments.size() > 1 ? arguments[1] : Value()),
          false, false};
}

void Control(Session* session, const std::string& label, const Value& value) {
  bool found = false;
  const bool success = session->Controls(
      [&](const std::string& operation, const Value::Array& arguments) {
        if (!arguments.empty() && arguments[0].StringOr("") == label) {
          found = true;
          return std::vector<Value>{value, true, true};
        }
        return UnchangedControl(operation, arguments);
      });
  INFO(session->Error());
  REQUIRE(success);
  REQUIRE(found);
}

void ClosedTriangle(Session* session) {
  Control(session, "曲线形式", 2);
  REQUIRE(session->Event(testing::PointerEvent("down", 0, 0)));
  REQUIRE(session->Event(testing::PointerEvent("down", 180, 220)));
  REQUIRE(session->Event(testing::PointerEvent("down", -180, 220)));
  testing::ExecuteChecked(
      session,
      "ctx:set_mode('edit');ctx.temp.selected=ctx.state.objects[1].id");
}

struct BezierFixture {
  Session session{testing::SourceRoot() / "sdk/drawer.lua",
                  testing::SourceRoot() / "cases/bezier/main.lua"};
};
}  // namespace

TEST_CASE("Closed Bezier tools collect independent controls and round trip",
          "[lua][bezier][closure][cases]") {
  const auto directory =
      GENERATE(std::string("bezier"), std::string("gallery"));
  const int degree = GENERATE(3, 5, 32);
  Session session(testing::SourceRoot() / "sdk/drawer.lua",
                  testing::SourceRoot() / "cases" / directory / "main.lua");
  if (directory == "gallery") {
    REQUIRE(session.Controls(
        [](const std::string& operation, const Value::Array& arguments) {
          if (operation == "combo" && arguments[0].StringOr("") == "图元") {
            const auto& choices = arguments[2].Items();
            const auto found =
                std::find(choices.begin(), choices.end(), Value("贝塞尔曲线"));
            REQUIRE(found != choices.end());
            return std::vector<Value>{
                static_cast<int>(found - choices.begin() + 1), true, true};
          }
          return UnchangedControl(operation, arguments);
        }));
  }
  Control(&session, "曲线形式", 2);
  Control(&session, "阶数", degree);
  const auto steps = session.GetHistory().Count();
  for (int i = 0; i < degree - 1; ++i) {
    REQUIRE(
        session.Event(testing::PointerEvent("down", i * 10, i % 2 ? 100 : 0)));
    CHECK(session.State().At("objects").Items().empty());
  }
  CHECK(session.GetModeFeedback().title.find("闭合") != std::string::npos);
  CHECK(session.GetModeFeedback().hint.find(std::to_string(degree - 1) + "/" +
                                            std::to_string(degree)) !=
        std::string::npos);
  REQUIRE(session.Draw(1));
  REQUIRE(session.Event(testing::PointerEvent("down", -100, 100)));
  REQUIRE(session.State().At("objects").Items().size() == 1);
  const auto& object = session.State().At("objects").Items()[0];
  CHECK(object.At("closed").Bool());
  CHECK(object.At("points").Items().size() == degree + 1);
  CHECK(object.At("points").Items().front() ==
        object.At("points").Items().back());
  CHECK(session.GetHistory().Count() == steps + 1);
  Session copy(testing::SourceRoot() / "sdk/drawer.lua",
               testing::SourceRoot() / "cases" / directory / "main.lua");
  copy.Restore(
      DecodeDocument(ParseJson(ToJson(EncodeDocument(session.SaveData())))));
  CHECK(copy.State() == session.State());
  CHECK_FALSE(copy.Dirty());
  CHECK(copy.Draw(1));
  session.Undo();
  CHECK(session.State().At("objects").Items().empty());
  session.Redo();
  CHECK(session.State() == copy.State());
}

TEST_CASE_METHOD(
    BezierFixture,
    "Closing and disconnecting preserve degree and each undo boundary",
    "[lua][bezier][closure][ui]") {
  for (const auto point :
       {Point{0, 0}, Point{180, 220}, Point{-180, 220}, Point{50, -50}})
    REQUIRE(session.Event(testing::PointerEvent("down", point.x, point.y)));
  testing::ExecuteChecked(
      &session,
      "ctx:set_mode('edit');ctx.temp.selected=ctx.state.objects[1].id");
  const auto open = session.State();
  const auto before = session.GetHistory().Count();
  Control(&session, "闭合曲线", true);
  CHECK(session.GetHistory().Count() == before + 1);
  const auto closed = session.State();
  const auto& points = closed.At("objects").Items()[0].At("points").Items();
  REQUIRE(points.size() == 4);
  CHECK(points.front() == points.back());
  CHECK(points[1] == open.At("objects").Items()[0].At("points").Items()[1]);
  CHECK_FALSE(session.State().At("params").At("closed").Bool());
  session.Undo();
  CHECK(session.State() == open);
  session.Redo();
  CHECK(session.State() == closed);
  testing::ExecuteChecked(&session,
                          "ctx.temp.selected=ctx.state.objects[1].id");
  Control(&session, "断开接点", true);
  const auto disconnected = session.State();
  CHECK_FALSE(disconnected.At("objects").Items()[0].At("closed").Bool());
  CHECK(disconnected.At("objects").Items()[0].At("points") ==
        closed.At("objects").Items()[0].At("points"));
  const auto steps = session.GetHistory().Count();
  REQUIRE(session.Event(testing::PointerEvent("down", 0, 0)));
  REQUIRE(session.Event(testing::PointerEvent("move", 40, -20)));
  REQUIRE(session.Event(testing::PointerEvent("up", 40, -20)));
  const auto& opened =
      session.State().At("objects").Items()[0].At("points").Items();
  CHECK(opened.front() == points.front());
  CHECK(opened.back().At("x").Number() == 40);
  CHECK(opened.back().At("y").Number() == -20);
  CHECK(session.GetHistory().Count() == steps + 1);
  session.Undo();
  CHECK(session.State() == disconnected);
  session.Undo();
  CHECK(session.State() == closed);
}

TEST_CASE_METHOD(
    BezierFixture,
    "Dragging the closed seam links endpoints and cancels atomically",
    "[lua][bezier][closure][interaction]") {
  const bool cancel = GENERATE(false, true);
  ClosedTriangle(&session);
  const auto before = session.State();
  const auto steps = session.GetHistory().Count();
  REQUIRE(session.Event(testing::PointerEvent("down", 2, 1)));
  REQUIRE(session.Event(testing::PointerEvent("move", 45, 21)));
  testing::ExecuteChecked(&session, R"(
    local o=ctx.state.objects[1]
    assert(o.points[1].x==43 and o.points[1].y==20)
    assert(o.points[4].x==43 and o.points[4].y==20)
    assert(#ctx:handles(o)==3)
  )");
  if (cancel) {
    REQUIRE(session.Event(testing::PointerEvent("key", 0, 0, "escape")));
    CHECK(session.State() == before);
    CHECK(session.GetHistory().Count() == steps);
  } else {
    REQUIRE(session.Event(testing::PointerEvent("up", 45, 21)));
    const auto moved = session.State();
    CHECK(session.GetHistory().Count() == steps + 1);
    session.Undo();
    CHECK(session.State() == before);
    session.Redo();
    CHECK(session.State() == moved);
  }
}

TEST_CASE_METHOD(
    BezierFixture,
    "Closed curves translate without drift and retain independent handles",
    "[lua][bezier][closure][interaction]") {
  ClosedTriangle(&session);
  const auto before = session.State();
  auto down = testing::PointerEvent("down", 0, 0);
  down["alt"] = true;
  REQUIRE(session.Event(down));
  REQUIRE(session.Event(testing::PointerEvent("move", 20, -5)));
  REQUIRE(session.Event(testing::PointerEvent("move", 40, -10)));
  REQUIRE(session.Event(testing::PointerEvent("up", 40, -10)));
  const auto moved = session.State();
  const auto& points = moved.At("objects").Items()[0].At("points").Items();
  const auto& original = before.At("objects").Items()[0].At("points").Items();
  for (size_t i = 0; i < points.size(); ++i) {
    CHECK(points[i].At("x").Number() == original[i].At("x").Number() + 40);
    CHECK(points[i].At("y").Number() == original[i].At("y").Number() - 10);
  }
  REQUIRE(session.Event(testing::PointerEvent("down", 220, 210)));
  REQUIRE(session.Event(testing::PointerEvent("up", 240, 240)));
  const auto& reshaped =
      session.State().At("objects").Items()[0].At("points").Items();
  CHECK(reshaped.front() == points.front());
  CHECK(reshaped.back() == points.back());
  CHECK(reshaped[1] != points[1]);
  session.Undo();
  CHECK(session.State() == moved);
}

TEST_CASE_METHOD(
    BezierFixture,
    "Closure controls raise only new low degrees and cancel pending drawing",
    "[lua][bezier][closure][ui]") {
  Control(&session, "阶数", 1);
  REQUIRE(session.Event(testing::PointerEvent("down", 0, 0)));
  const auto steps = session.GetHistory().Count();
  Control(&session, "曲线形式", 2);
  CHECK(session.State().At("params").At("degree").Number() == 3);
  CHECK(session.GetHistory().Count() == steps + 1);
  CHECK(session.GetModeFeedback().notice.find("未完成") != std::string::npos);
  testing::ExecuteChecked(&session, "assert(ctx:drawing_count()==0)");
  session.Undo();
  CHECK(session.State().At("params").At("degree").Number() == 1);
  CHECK_FALSE(session.State().At("params").At("closed").Bool());
  REQUIRE(session.Event(testing::PointerEvent("down", 0, 0)));
  REQUIRE(session.Event(testing::PointerEvent("down", 100, 0)));
  testing::ExecuteChecked(
      &session,
      "ctx:set_mode('edit');ctx.temp.selected=ctx.state.objects[1].id");
  bool offered_closure = false;
  REQUIRE(session.Controls(
      [&](const std::string& operation, const Value::Array& args) {
        if (!args.empty() && args[0].StringOr("") == "闭合曲线")
          offered_closure = true;
        return UnchangedControl(operation, args);
      }));
  CHECK_FALSE(offered_closure);
  const auto before = session.State();
  CHECK_FALSE(
      session.Execute("ctx:edit('invalid',function()ctx:set_bezier_closed(ctx."
                      "state.objects[1],true)end)"));
  CHECK(session.State() == before);
}

TEST_CASE_METHOD(
    BezierFixture,
    "Creation closure changes leave existing curves and other canvases intact",
    "[lua][bezier][closure][isolation]") {
  ClosedTriangle(&session);
  const auto closed = session.State().At("objects").Items()[0];
  testing::ExecuteChecked(&session, "ctx:set_mode('draw')");
  Control(&session, "曲线形式", 1);
  for (int i = 0; i < 4; ++i)
    REQUIRE(session.Event(
        testing::PointerEvent("down", 300 + i * 20, i % 2 ? 50 : 0)));
  REQUIRE(session.State().At("objects").Items().size() == 2);
  CHECK(session.State().At("objects").Items()[0] == closed);
  CHECK_FALSE(session.State().At("objects").Items()[1].At("closed").Bool());
  Session other(testing::SourceRoot() / "sdk/drawer.lua",
                testing::SourceRoot() / "cases/bezier/main.lua");
  CHECK_FALSE(other.State().At("params").At("closed").Bool());
  CHECK(other.State().At("objects").Items().empty());
  CHECK_FALSE(other.Dirty());
}

TEST_CASE_METHOD(
    BezierFixture,
    "Closed Bezier picking follows the curve and displays one seam handle",
    "[lua][bezier][closure][selection]") {
  ClosedTriangle(&session);
  testing::ExecuteChecked(&session, R"(
    local o=ctx.state.objects[1]
    assert(ctx:hit(o,ctx.math.bezier(o.points,0.5),1))
    assert(not ctx:hit(o,{x=100,y=0},1))
    assert(ctx:handle_label(o,1)=='起点/终点')
  )");
  REQUIRE(session.Draw(1));
  int seam_labels = 0;
  int handles = 0;
  for (const auto& shape : session.DrawItems()) {
    if (shape.At("text").StringOr("") == "起点/终点") ++seam_labels;
    if (shape.At("kind").StringOr("") == "point" &&
        shape.At("outline").IsBool() && shape.At("outline").Bool() &&
        shape.At("width").NumberOr(0) == 1.5)
      ++handles;
  }
  CHECK(seam_labels == 1);
  CHECK(handles == 3);
}

TEST_CASE_METHOD(
    BezierFixture,
    "Documents reject inconsistent closed curves before replacing live state",
    "[lua][bezier][closure][document]") {
  ClosedTriangle(&session);
  const auto before = session.State();
  auto document = session.SaveData();
  auto& object = document.state["objects"].Items()[0];
  SECTION("Non-boolean closure") { object["closed"] = "yes"; }
  SECTION("Different endpoints") { object["points"].Items().back()["x"] = 10; }
  SECTION("Low degree") { object["points"].Items().resize(3); }
  CHECK_THROWS_AS(EncodeDocument(document), std::runtime_error);
  CHECK_THROWS_AS(session.Restore(document), std::runtime_error);
  CHECK(session.State() == before);
}

TEST_CASE_METHOD(
    BezierFixture,
    "Legacy coincident endpoints remain open until explicitly closed",
    "[lua][bezier][closure][document]") {
  testing::ExecuteChecked(&session, R"(
    ctx:edit('legacy',function()
      ctx:add('bezier',{{x=0,y=0},{x=100,y=100},{x=-100,y=100},{x=0,y=0}})
    end)
  )");
  auto legacy = session.SaveData();
  legacy.state["params"].Fields().erase("closed");
  session.Restore(DecodeDocument(ParseJson(ToJson(EncodeDocument(legacy)))));
  CHECK(session.State().At("objects").Items()[0].At("closed").IsNull());
  CHECK_FALSE(session.State().At("params").At("closed").Bool());
  CHECK_FALSE(session.Dirty());
  testing::ExecuteChecked(&session, R"(
    local o=ctx.state.objects[1]
    assert(#ctx:handles(o)==4)
    ctx:edit('close',function()ctx:set_bezier_closed(o,true)end)
    assert(#ctx:handles(o)==3)
  )");
}
}  // namespace drawer
