#include <catch2/catch_test_macros.hpp>
#include "support/session_fixture.h"
namespace drawer {
TEST_CASE_METHOD(
    testing::TriangleFixture,
    "Tree selection changes mode without modifying persistent data",
    "[inspector][lua]") {
  testing::CreateTriangle(&session);
  const auto before = session.State();
  const auto history = session.GetHistory().Count();
  REQUIRE(session.SelectObject(1));
  CHECK(session.SelectedObject() == 1);
  CHECK(session.GetModeFeedback().id == "edit");
  CHECK(session.State() == before);
  CHECK(session.GetHistory().Count() == history);
  REQUIRE(session.DeleteSelectedObject());
  CHECK(session.State().At("objects").Items().empty());
  session.Undo();
  CHECK(session.State() == before);
  CHECK(session.SelectedObject() == 0);
  CHECK_FALSE(session.SelectObject(999));
  CHECK(session.State() == before);
}
TEST_CASE("Inspector edits smooth control points through the linked handle API",
          "[inspector][lua]") {
  Session s(testing::SourceRoot() / "sdk/drawer.lua",
            testing::SourceRoot() / "cases/bezier/main.lua");
  testing::ExecuteChecked(
      &s,
      R"(ctx:edit('curve',function()ctx:add('bezier',{{x=0,y=0},{x=100,y=0},{x=0,y=180},{x=-100,y=0},{x=0,y=0}},{closed=true,smooth=true})end))");
  REQUIRE(s.SelectObject(1));
  const auto before = s.State();
  REQUIRE(s.ObjectControls([](const std::string& op, const Value::Array& args) {
    if (op == "collapsing_header")
      return std::vector<Value>{true, false, false};
    if (args[0].StringOr("") == "x##point2")
      return std::vector<Value>{150, true, true};
    return std::vector<Value>{args.size() > 1 ? args[1] : Value(), false,
                              false};
  }));
  CHECK(s.State()
            .At("objects")
            .Items()[0]
            .At("points")
            .Items()[3]
            .At("x")
            .Number() == -150);
  s.Undo();
  CHECK(s.State() == before);
}
TEST_CASE_METHOD(
    testing::TriangleFixture,
    "Inspector names are data and one continuous widget edit makes one undo",
    "[inspector][lua]") {
  testing::CreateTriangle(&session);
  session.SelectObject(1);
  const auto before = session.State();
  const auto count = session.GetHistory().Count();
  for (int frame = 0; frame < 3; ++frame) {
    REQUIRE(session.ObjectControls(
        [frame](const std::string&, const Value::Array& args) {
          if (args[0].StringOr("") == "名称")
            return std::vector<Value>{std::string(frame == 2 ? "完成" : "编辑"),
                                      true, frame == 2};
          return std::vector<Value>{args.size() > 1 ? args[1] : Value(), false,
                                    false};
        }));
  }
  CHECK(session.GetHistory().Count() == count + 1);
  CHECK(session.State().At("objects").Items()[0].At("display_name").String() ==
        "完成");
  session.Undo();
  CHECK(session.State() == before);
}
TEST_CASE(
    "Undeclared custom editing capability does not route tree selection into "
    "drawing",
    "[inspector][lua]") {
  testing::TestDirectory dir;
  auto entry = dir.Path() / "main.lua";
  testing::WriteScript(
      entry,
      R"(return {id='custom',name='custom',data_version=1,on_init=function(c)c:add('line',{{x=0,y=0},{x=20,y=0}})end,on_event=function(c,e)c:edit('event',function()c.state.params.changed=true end)end})");
  Session s(testing::SourceRoot() / "sdk/drawer.lua", entry);
  const auto before = s.State();
  REQUIRE(s.SelectObject(1));
  REQUIRE(s.Event(testing::PointerEvent("down", 0, 0)));
  CHECK(s.State() == before);
  REQUIRE(s.ObjectControls([](const std::string& op, const Value::Array&) {
    CHECK(op == "text");
    return std::vector<Value>{};
  }));
  CHECK_FALSE(s.DeleteSelectedObject());
  CHECK(s.State() == before);
}
TEST_CASE("Object property extensions are protected and failures roll back",
          "[inspector][lua]") {
  testing::TriangleFixture f;
  auto& s = f.session;
  testing::CreateTriangle(&s);
  s.SelectObject(1);
  const auto before = s.State();
  testing::ExecuteChecked(&s,
                          R"(ctx:register_type('polygon',{
inspect=function(c,o)if c.ui.button('custom') then c:edit('custom',function()o.width=9 end) end end,
bounds=function()return {{x=-500,y=-600},{x=700,y=800}}end}))");
  const auto bounds = s.ObjectBounds(1);
  REQUIRE(bounds.IsArray());
  CHECK(bounds.Items()[0].At("x").Number() == -500);
  REQUIRE(s.ObjectControls([](const std::string& op, const Value::Array& args) {
    if (op == "button") return std::vector<Value>{true, false, false};
    return std::vector<Value>{args.size() > 1 ? args[1] : Value(), false,
                              false};
  }));
  CHECK(s.State().At("objects").Items()[0].At("width").Number() == 9);
  s.Undo();
  REQUIRE(s.SelectObject(1));
  testing::ExecuteChecked(
      &s,
      R"(ctx:register_type('polygon',{inspect=function(c,o)c:edit('bad',function()o.width=10;error('inspector failure')end)end}))");
  CHECK_FALSE(s.ObjectControls({}));
  CHECK(s.State() == before);
  CHECK(s.Error().find("inspector failure") != std::string::npos);
}
}  // namespace drawer
