#include <catch2/catch_test_macros.hpp>

#include "support/session_fixture.h"

namespace drawer {
TEST_CASE_METHOD(
    testing::TriangleFixture,
    "Explicit mode resets preserve the SDK v1 cancellation contract",
    "[lua][mode]") {
  REQUIRE(session.Event(testing::PointerEvent("down", 0, 0)));
  testing::ExecuteChecked(
      &session, "ctx:set_mode('draw');assert(ctx:drawing_count()==0)");
  CHECK(session.GetModeFeedback().id == "draw");
  CHECK_FALSE(session.Dirty());
}
TEST_CASE_METHOD(testing::TriangleFixture,
                 "Selected mode buttons are idempotent and switching cancels "
                 "only the preview",
                 "[lua][mode][ui]") {
  const auto before = session.State();
  REQUIRE(session.Event(testing::PointerEvent("down", 0, 0)));
  REQUIRE(session.Event(testing::PointerEvent("down", 100, 0)));
  REQUIRE(session.Controls(
      [](const std::string& operation, const Value::Array& arguments) {
        const bool clicked =
            operation == "mode_button" && arguments[0].String() == "绘制";
        return std::vector<Value>{clicked, false, false};
      }));
  testing::ExecuteChecked(&session, "assert(ctx:drawing_count()==2)");
  CHECK(session.GetModeFeedback().id == "draw");
  REQUIRE(session.Controls(
      [](const std::string& operation, const Value::Array& arguments) {
        const bool clicked =
            operation == "mode_button" && arguments[0].String() == "调整";
        return std::vector<Value>{clicked, false, false};
      }));
  const auto feedback = session.GetModeFeedback();
  CHECK(feedback.id == "edit");
  CHECK_FALSE(feedback.notice.empty());
  CHECK(feedback.title.find("调整") != std::string::npos);
  CHECK(session.State() == before);
  CHECK_FALSE(session.Dirty());
  CHECK(session.GetHistory().Count() == 0);
  testing::ExecuteChecked(&session, "assert(ctx:drawing_count()==0)");
}
TEST_CASE_METHOD(testing::TriangleFixture,
                 "Mode feedback reports construction progress and custom modes",
                 "[lua][mode]") {
  CHECK(session.GetModeFeedback().cursor == "crosshair");
  REQUIRE(session.Event(testing::PointerEvent("down", 0, 0)));
  CHECK(session.GetModeFeedback().hint.find("1/3") != std::string::npos);
  testing::ExecuteChecked(&session, R"(
    ctx:register_mode('inspect',{label='Inspect geometry',hint='Pick a measurement',cursor='hand'})
    ctx:set_mode('inspect')
  )");
  const auto custom = session.GetModeFeedback();
  CHECK(custom.id == "inspect");
  CHECK(custom.title == "Inspect geometry");
  CHECK(custom.hint == "Pick a measurement");
  CHECK(custom.cursor == "hand");
  testing::ExecuteChecked(&session, "ctx:set_mode('unregistered')");
  CHECK(session.GetModeFeedback().title == "unregistered");
  CHECK(session.GetModeFeedback().cursor == "arrow");
}
TEST_CASE_METHOD(
    testing::TriangleFixture,
    "Mode switching rolls back a drag and notice expiry uses elapsed time",
    "[lua][mode]") {
  testing::ExecuteChecked(&session, R"(
    ctx:edit('shape',function() ctx:add('polygon',{{x=0,y=0},{x=100,y=0},{x=50,y=100}})end)
    ctx:set_mode('edit')
  )");
  session.MarkSaved();
  const auto before = session.State();
  REQUIRE(session.Event(testing::PointerEvent("down", 50, 30)));
  REQUIRE(session.Event(testing::PointerEvent("move", 100, 60)));
  REQUIRE(session.Editing());
  testing::ExecuteChecked(
      &session, "_host.clock=function()return 100 end;ctx:set_mode('draw')");
  CHECK(session.State() == before);
  CHECK_FALSE(session.Dirty());
  CHECK_FALSE(session.GetModeFeedback().notice.empty());
  testing::ExecuteChecked(&session, "_host.clock=function()return 103 end");
  CHECK(session.GetModeFeedback().notice.empty());
  session.Undo();
  CHECK(session.GetModeFeedback().id == "draw");
  session.Redo();
  CHECK(session.GetModeFeedback().id == "draw");
}
TEST_CASE_METHOD(
    testing::TriangleFixture,
    "Mode state is isolated and resets to the case default on restore",
    "[lua][mode]") {
  Session other(testing::SourceRoot() / "sdk/drawer.lua", session.Entry());
  testing::ExecuteChecked(&session, "ctx:set_mode('edit')");
  CHECK(other.GetModeFeedback().id == "draw");
  REQUIRE(session.Event(Value::Object{{"type", "key"}, {"key", "escape"}}));
  CHECK(session.GetModeFeedback().id == "edit");
  session.Restore(session.SaveData());
  CHECK(session.GetModeFeedback().id == "draw");
  CHECK_FALSE(session.Dirty());
}
TEST_CASE_METHOD(
    testing::TriangleFixture,
    "A failed feedback query is diagnosed and leaves the Lua stack usable",
    "[lua][mode]") {
  const auto before = session.State();
  testing::ExecuteChecked(
      &session, "ctx.mode_feedback=function()error('feedback sentinel')end");
  CHECK(session.GetModeFeedback().id == "draw");
  CHECK(session.Error().find("feedback sentinel") != std::string::npos);
  CHECK(session.State() == before);
  testing::ExecuteChecked(&session, "assert(ctx.mode=='draw')");
}
}  // namespace drawer
