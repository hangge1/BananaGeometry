#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "support/session_fixture.h"

namespace drawer {
TEST_CASE_METHOD(testing::TriangleFixture,
                 "Free polygon construction completes as one undoable edit",
                 "[lua][drawing]") {
  const bool right_click = GENERATE(false, true);
  testing::ExecuteChecked(&session, R"(
    local function down(x,y) ctx:draw_event('polygon',0,{type='down',button=1,x=x,y=y}) end
    down(0,0); down(100,0)
    assert(not ctx:finish_drawing())
    down(50,100)
    assert(#ctx.state.objects==0)
  )");
  testing::ExecuteChecked(
      &session,
      right_click
          ? "ctx:draw_event('polygon',0,{type='down',button=2,x=500,y=500})"
          : "ctx:draw_event('polygon',0,{type='key',key='enter'})");
  REQUIRE(session.State().At("objects").Items().size() == 1);
  CHECK(session.State().At("objects").Items()[0].At("points").Items().size() ==
        3);
  CHECK(session.GetHistory().Count() == 1);
  session.Undo();
  CHECK(session.State().At("objects").Items().empty());
  session.Redo();
  CHECK(session.State().At("objects").Items().size() == 1);
}
TEST_CASE_METHOD(testing::TriangleFixture,
                 "Switching construction tools cancels partial geometry",
                 "[lua][drawing]") {
  testing::ExecuteChecked(&session, R"(
    ctx:draw_event('polygon',5,{type='down',button=1,x=0,y=0})
    ctx:draw_event('polygon',5,{type='down',button=1,x=10,y=0})
    ctx:draw_event('bezier',3,{type='down',button=1,x=100,y=100})
    assert(ctx:drawing_count()==1 and ctx.temp.pending[1].x==100)
  )");
  CHECK(session.Draw(1));
  CHECK_FALSE(session.DrawItems().empty());
  CHECK(session.State().At("objects").Items().empty());
  CHECK(session.GetHistory().Count() == 0);
  testing::ExecuteChecked(&session,
                          "ctx:draw_event('bezier',3,{type='key',key='escape'})"
                          ";assert(ctx:drawing_count()==0)");
  CHECK(session.Draw(1));
  CHECK(session.DrawItems().empty());
}
TEST_CASE_METHOD(
    testing::TriangleFixture,
    "Free drawing has a bounded point count and rejects invalid counts",
    "[lua][drawing]") {
  testing::ExecuteChecked(&session, R"(
    for i=1,256 do
      ctx:draw_event('polyline',0,{type='down',button=1,x=i,y=i%3})
    end
    assert(ctx:drawing_count()==0)
  )");
  REQUIRE(session.State().At("objects").Items().size() == 1);
  CHECK(session.State().At("objects").Items()[0].At("points").Items().size() ==
        256);
  CHECK_FALSE(session.Execute("ctx:draw_event('polygon',257,{type='move'})"));
  CHECK_FALSE(session.Execute("ctx:draw_event('bezier',0,{type='move'})"));
}
TEST_CASE("Creation controls cancel previews without losing parameter edits",
          "[lua][drawing][ui]") {
  testing::TestDirectory directory;
  const auto entry = directory.Path() / "main.lua";
  testing::WriteScript(entry, R"(
    return {id='creation',name='Creation',data_version=1,
      on_init=function(c) c:ensure_parameters({vertices=5}) end,
      on_ui=function(c) c:creation_parameter('vertices','vertices','integer',3,256) end,
      on_event=function(c,e) c:draw_event('polygon',c.state.params.vertices,e) end}
  )");
  Session session(testing::SourceRoot() / "sdk/drawer.lua", entry);
  REQUIRE(session.Event(testing::PointerEvent("down", 0, 0)));
  REQUIRE(session.Event(testing::PointerEvent("down", 100, 0)));
  REQUIRE(session.Controls([](const std::string&, const Value::Array&) {
    return std::vector<Value>{3, true, true};
  }));
  testing::ExecuteChecked(&session, "assert(ctx:drawing_count()==0)");
  CHECK(session.State().At("params").At("vertices").Number() == 3);
  CHECK(session.GetHistory().Count() == 1);
  session.Undo();
  CHECK(session.State().At("params").At("vertices").Number() == 5);
}
TEST_CASE("Load defaults become the saved clean baseline", "[lua][document]") {
  testing::TestDirectory directory;
  const auto entry = directory.Path() / "main.lua";
  testing::WriteScript(entry, R"(
    local function defaults(c) c:ensure_parameters({vertices=3}) end
    return {id='defaults',name='Defaults',data_version=1,on_init=defaults,on_load=defaults}
  )");
  Session session(testing::SourceRoot() / "sdk/drawer.lua", entry);
  auto legacy = session.SaveData();
  legacy.state["params"].Fields().clear();
  session.Restore(legacy);
  CHECK(session.SaveData().state.At("params").At("vertices").Number() == 3);
  CHECK_FALSE(session.Dirty());
  testing::ExecuteChecked(
      &session, "ctx:edit('change',function()ctx.state.params.vertices=5 end)");
  session.Undo();
  CHECK(session.State().At("params").At("vertices").Number() == 3);
  CHECK_FALSE(session.Dirty());
}
}  // namespace drawer
