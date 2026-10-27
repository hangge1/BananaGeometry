#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <catch2/matchers/catch_matchers_string.hpp>
#include "support/session_fixture.h"

namespace drawer {
using Catch::Matchers::ContainsSubstring;
using testing::ExecuteChecked;
using testing::PointerEvent;
using testing::SourceRoot;

TEST_CASE_METHOD(testing::TriangleFixture,
                 "Continuous drawing creates independent objects and history",
                 "[lua][interaction]") {
  testing::CreateTriangle(&session);
  testing::CreateTriangle(&session);
  CHECK(session.State().At("objects").Items().size() == 2);
  CHECK(session.GetHistory().Count() == 2);
  session.Undo();
  CHECK(session.State().At("objects").Items().size() == 1);
}
TEST_CASE_METHOD(testing::TriangleFixture,
                 "Instances do not share editable state", "[lua]") {
  Session second(SourceRoot() / "sdk/drawer.lua",
                 SourceRoot() / "cases/triangle/main.lua");
  testing::CreateTriangle(&session);
  CHECK(second.State().At("objects").Items().empty());
}
TEST_CASE_METHOD(testing::TriangleFixture,
                 "A complete drag is one undo step and preserves the mode",
                 "[lua][interaction]") {
  testing::CreateTriangle(&session);
  const auto original = session.State();
  ExecuteChecked(&session, "ctx:set_mode('edit')");
  REQUIRE(session.Event(PointerEvent("down", 0, 0)));
  REQUIRE(session.Editing());
  for (int step = 1; step <= 30; ++step)
    REQUIRE(session.Event(PointerEvent("move", -step, -step)));
  REQUIRE(session.Event(PointerEvent("up", -30, -30)));
  CHECK_FALSE(session.Editing());
  CHECK(session.GetHistory().Count() == 2);
  const auto moved = session.State();
  CHECK(moved != original);
  session.Undo();
  CHECK(session.State() == original);
  session.Redo();
  CHECK(session.State() == moved);
  ExecuteChecked(&session, "assert(ctx.mode=='edit')");
}
TEST_CASE_METHOD(testing::TriangleFixture,
                 "Cancel restores drag data and clears stale handles",
                 "[lua][interaction]") {
  const bool escape = GENERATE(false, true);
  testing::CreateTriangle(&session);
  const auto before = session.State();
  ExecuteChecked(&session, "ctx:set_mode('edit')");
  REQUIRE(session.Event(PointerEvent("down", 0, 0)));
  REQUIRE(session.Event(PointerEvent("move", -50, -50)));
  if (escape)
    REQUIRE(session.Event(PointerEvent("key", 0, 0, "escape")));
  else
    session.Cancel();
  CHECK_FALSE(session.Editing());
  CHECK(session.State() == before);
  CHECK(session.GetHistory().Count() == 1);
  CHECK(session.Event(PointerEvent("move", 10, 10)));
  CHECK(session.Event(PointerEvent("up", 10, 10)));
  CHECK(session.State() == before);
}
TEST_CASE_METHOD(testing::TriangleFixture, "Clear is undoable", "[lua]") {
  testing::CreateTriangle(&session);
  const auto before = session.State();
  REQUIRE(session.Clear());
  CHECK(session.State().At("objects").Items().empty());
  session.Undo();
  CHECK(session.State() == before);
}
TEST_CASE_METHOD(testing::TriangleFixture, "Invalid edits roll back all state",
                 "[lua]") {
  const auto source = GENERATE(
      std::string("ctx:edit('error',function() ctx.state.params.bad=1; "
                  "error('rollback marker') end)"),
      std::string("ctx:edit('cycle',function() ctx.state.loop=ctx.state end)"),
      std::string(
          "ctx:edit('sparse',function() ctx.state.bad={[1]=1,[3]=3} end)"));
  const auto before = session.State();
  CAPTURE(source);
  CHECK_FALSE(session.Execute(source));
  CHECK_FALSE(session.Error().empty());
  CHECK_FALSE(session.Editing());
  CHECK(session.State() == before);
}
TEST_CASE_METHOD(testing::TriangleFixture,
                 "Instruction budget catches infinite loops", "[lua]") {
  CHECK_FALSE(session.Execute("while true do end"));
  CHECK_THAT(session.Error(), ContainsSubstring("instruction budget"));
}
TEST_CASE_METHOD(testing::TriangleFixture,
                 "Lua failures retain their source name", "[lua]") {
  CHECK_FALSE(session.Execute("error('source marker')", "@source_spec.lua"));
  CHECK_THAT(session.Error(), ContainsSubstring("source_spec.lua"));
}
TEST_CASE_METHOD(testing::TriangleFixture,
                 "Parameter widgets commit and undo edits", "[lua][ui]") {
  REQUIRE(
      session.Controls([](const std::string& operation,
                          const Value::Array& arguments) -> std::vector<Value> {
        if (operation == "slider") return {4, true, true};
        return {arguments.size() > 1 ? arguments[1] : Value(false), false,
                false};
      }));
  CHECK(session.State().At("params").At("width").Number() == 4);
  CHECK(session.GetHistory().Count() == 1);
  session.Undo();
  CHECK(session.State().At("params").At("width").Number() == 2);
}
TEST_CASE("Every example initializes and renders headlessly",
          "[lua][examples]") {
  for (const auto& path : DiscoverCases(SourceRoot() / "cases")) {
    CAPTURE(path.u8string());
    Session session(SourceRoot() / "sdk/drawer.lua", path);
    CHECK(session.Controls({}));
    CHECK(session.Draw(1));
  }
}
TEST_CASE("Failed reload candidates leave the old VM unchanged",
          "[lua][reload]") {
  testing::TestDirectory directory;
  const auto entry = directory.Path() / "main.lua";
  testing::WriteScript(
      entry,
      "return {id='reload',name='Reload',data_version=1,on_init=function(c) "
      "c.state.params.value=1 end}");
  Session old(SourceRoot() / "sdk/drawer.lua", entry);
  testing::WriteScript(entry, "this is invalid lua!");
  CHECK_THROWS_AS(Session(SourceRoot() / "sdk/drawer.lua", entry),
                  std::runtime_error);
  CHECK(old.State().At("params").At("value").Number() == 1);
  testing::WriteScript(
      entry,
      "return {id='reload',name='Reload',data_version=1,on_init=function(c) "
      "c.state.params.value=2 end}");
  Session replacement(SourceRoot() / "sdk/drawer.lua", entry);
  CHECK(replacement.State().At("params").At("value").Number() == 2);
  CHECK(old.State().At("params").At("value").Number() == 1);
}
TEST_CASE("Case-local modules work under Unicode directories",
          "[lua][modules]") {
  testing::TestDirectory directory;
  const auto folder = directory.Path() / L"中文用例";
  std::filesystem::create_directories(folder);
  testing::WriteScript(folder / "main.lua",
                       "local h=require('helper'); return "
                       "{id='unicode',name='Unicode',data_version=1,on_init="
                       "function(c) c.state.params.answer=h.answer end}");
  testing::WriteScript(folder / "helper.lua", "return {answer=42}");
  Session session(SourceRoot() / "sdk/drawer.lua", folder / "main.lua");
  CHECK(session.State().At("params").At("answer").Number() == 42);
  CHECK_FALSE(session.Execute("require('..outside')"));
}
TEST_CASE_METHOD(testing::TriangleFixture, "Lua geometry executable specs",
                 "[lua][geometry]") {
  testing::RunLuaSpec(&session, "geometry_spec.lua");
}
TEST_CASE_METHOD(testing::TriangleFixture, "Lua transaction executable specs",
                 "[lua][history]") {
  testing::RunLuaSpec(&session, "transaction_spec.lua");
}
TEST_CASE_METHOD(testing::TriangleFixture,
                 "A thousand objects serialize and draw", "[lua][scale]") {
  ExecuteChecked(
      &session,
      "ctx:edit('1000 objects',function() for i=1,1000 do "
      "ctx:add('triangle',{{x=i,y=0},{x=i+3,y=0},{x=i,y=3}}) end end)");
  REQUIRE(session.Draw(1));
  CHECK(session.DrawItems().size() == 1000);
  CHECK(DecodeDocument(ParseJson(ToJson(EncodeDocument(session.SaveData()))))
            .state == session.State());
}
}  // namespace drawer
