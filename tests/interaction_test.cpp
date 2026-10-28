#include <limits>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "support/gallery_fixture.h"

namespace drawer {
namespace {
using testing::ExecuteChecked;
using testing::PointerEvent;

using testing::GalleryFixture;

bool IsHighlight(const Value& item) {
  return item.At("color") == Value::Array{1, .82, .25, 1};
}

TEST_CASE_METHOD(GalleryFixture,
                 "Selection highlights the shape and exposes its handles",
                 "[lua][interaction]") {
  const auto before = session.SaveData();
  REQUIRE(session.Draw(1));
  const auto baseline = session.DrawItems().size();
  REQUIRE(session.Event(PointerEvent("down", 160, 100)));
  ExecuteChecked(&session, "assert(ctx.temp.selected==3)");
  REQUIRE(session.Event(PointerEvent("up", 160, 100)));
  CHECK_FALSE(session.Editing());
  REQUIRE(session.Draw(1));
  int outlines = 0;
  int handles = 0;
  for (const auto& item : session.DrawItems()) {
    if (!IsHighlight(item)) continue;
    if (item.At("kind").StringOr("") == "rect") {
      ++outlines;
      CHECK_FALSE(item.At("fill").Bool());
      CHECK(item.At("width").Number() == 1.5);
      CHECK(item.At("points").Items()[0].At("x").Number() < 100);
      CHECK(item.At("points").Items()[1].At("x").Number() > 220);
    }
    if (item.At("kind").StringOr("") == "point") {
      CHECK(item.At("outline").Bool());
      ++handles;
    }
  }
  CHECK(outlines == 1);
  CHECK(handles == 2);
  CHECK(session.State() == before.state);
  CHECK(session.GetHistory().Count() == 0);
  CHECK_FALSE(session.Dirty());

  REQUIRE(session.Event(PointerEvent("down", 500, 500)));
  ExecuteChecked(&session, "assert(ctx.temp.selected==nil)");
  REQUIRE(session.Draw(1));
  CHECK(session.DrawItems().size() == baseline);
}

TEST_CASE_METHOD(GalleryFixture,
                 "Custom drawing helpers share selection feedback",
                 "[lua][interaction][custom]") {
  const auto before = session.State();
  REQUIRE(session.Event(PointerEvent("down", 0, 0)));
  REQUIRE(session.Event(PointerEvent("up", 0, 0)));
  REQUIRE(session.Draw(1));
  int highlighted_lines = 0;
  int frames = 0;
  for (const auto& item : session.DrawItems()) {
    if (IsHighlight(item) && item.At("kind").StringOr("") == "line")
      ++highlighted_lines;
    if (IsHighlight(item) && item.At("kind").StringOr("") == "rect") {
      ++frames;
      CHECK(item.At("points").Items()[0].At("x").Number() < -25);
      CHECK(item.At("points").Items()[1].At("x").Number() > 25);
    }
  }
  CHECK(highlighted_lines == 0);
  CHECK(frames == 1);
  CHECK(session.State() == before);
  CHECK(session.GetHistory().Count() == 0);
  ExecuteChecked(&session, "ctx:set_mode('draw')");
  REQUIRE(session.Draw(1));
  for (const auto& item : session.DrawItems()) CHECK_FALSE(IsHighlight(item));
}

TEST_CASE_METHOD(GalleryFixture,
                 "Text body dragging preserves its grab offset at every zoom",
                 "[lua][interaction][text]") {
  const double zoom = GENERATE(.5, 1., 2., 4.);
  session.view.zoom = zoom;
  session.SetTextMeasurer(
      [](const std::string&, double) { return Point{120, 20}; });
  const auto before = session.State();
  const double x = 100 + 100 / zoom;
  const double y = -65 - 12 / zoom;
  REQUIRE(session.Event(PointerEvent("down", x, y)));
  REQUIRE(session.Editing());
  ExecuteChecked(&session, "assert(ctx.temp.selected==6)");
  // Moving at the click position must not snap the anchor to the pointer.
  REQUIRE(session.Event(PointerEvent("move", x, y)));
  REQUIRE(session.Draw(zoom));
  bool preview_found = false;
  bool outline_found = false;
  for (const auto& item : session.DrawItems()) {
    if (item.At("kind").StringOr("") == "text" &&
        item.At("text").StringOr("") == "二维图形 SDK") {
      CHECK(item.At("color") == Value::Array{.8, .85, 1, 1});
      CHECK(item.At("points").Items()[0].At("x").Number() == 100);
      CHECK(item.At("points").Items()[0].At("y").Number() == -65);
      preview_found = true;
    }
    if (IsHighlight(item) && item.At("kind").StringOr("") == "rect")
      outline_found = true;
  }
  CHECK(preview_found);
  CHECK(outline_found);
  REQUIRE(session.Event(PointerEvent("move", x + 30 / zoom, y - 16 / zoom)));
  REQUIRE(session.Event(PointerEvent("up", x + 30 / zoom, y - 16 / zoom)));
  const auto& anchor =
      session.State().At("objects").Items()[5].At("points").Items()[0];
  CHECK(anchor.At("x").Number() == Catch::Approx(100 + 30 / zoom));
  CHECK(anchor.At("y").Number() == Catch::Approx(-65 - 16 / zoom));
  CHECK(session.GetHistory().Count() == 1);
  const auto moved = session.State();
  session.Undo();
  CHECK(session.State() == before);
  session.Redo();
  CHECK(session.State() == moved);
}

TEST_CASE_METHOD(GalleryFixture,
                 "Text clicks and canceled drags do not create edits",
                 "[lua][interaction][text]") {
  const bool escape = GENERATE(false, true);
  session.SetTextMeasurer(
      [](const std::string&, double) { return Point{120, 20}; });
  const auto before = session.State();
  REQUIRE(session.Event(PointerEvent("down", 200, -75)));
  REQUIRE(session.Event(PointerEvent("up", 200, -75)));
  CHECK(session.GetHistory().Count() == 0);
  CHECK_FALSE(session.Dirty());
  REQUIRE(session.Event(PointerEvent("down", 200, -75)));
  REQUIRE(session.Event(PointerEvent("move", 250, -100)));
  if (escape)
    REQUIRE(session.Event(PointerEvent("key", 250, -100, "escape")));
  else
    session.Cancel();
  REQUIRE(session.Event(PointerEvent("up", 250, -100)));
  CHECK_FALSE(session.Editing());
  CHECK(session.State() == before);
  CHECK(session.GetHistory().Count() == 0);
  CHECK_FALSE(session.Dirty());
}

TEST_CASE_METHOD(GalleryFixture,
                 "Text bounds use measured pixels and mathematical coordinates",
                 "[lua][interaction][text]") {
  session.SetTextMeasurer([](const std::string& text, double) {
    return text.empty() ? Point{} : Point{120, 40};
  });
  ExecuteChecked(&session, R"(
    ctx.zoom=2
    local label={kind='text',points={{x=100,y=-65}},text='中文\nSDK##label'}
    local b=ctx:text_bounds(label)
    assert(b.left==100 and b.right==160 and b.top==-65 and b.bottom==-85)
    assert(ctx:hit(label,{x=159,y=-84},0))
    assert(not ctx:hit(label,{x=161,y=-84},0))
    assert(not ctx:hit(label,{x=159,y=-86},0))
    label.text=''
    assert(ctx:text_bounds(label)==nil)
    assert(not ctx:hit(label,{x=159,y=-84},0))
  )");
}

TEST_CASE_METHOD(GalleryFixture,
                 "Foreground text is picked before an unselected rear handle",
                 "[lua][interaction][text]") {
  session.SetTextMeasurer(
      [](const std::string&, double) { return Point{120, 20}; });
  ExecuteChecked(&session, R"(
    ctx:edit('overlap',function()
      ctx.state.objects=ctx.array()
      ctx:add('point',{{x=50,y=-10}})
      ctx:add('text',{{x=0,y=0}},{text='foreground'})
    end)
  )");
  REQUIRE(session.Event(PointerEvent("down", 50, -10)));
  ExecuteChecked(&session,
                 "assert(ctx:object(ctx.temp.selected).kind=='text')");
  session.Cancel();
}

TEST_CASE_METHOD(GalleryFixture, "Invalid text measurements are rejected",
                 "[lua][text]") {
  const double width = GENERATE(-1., std::numeric_limits<double>::infinity());
  session.SetTextMeasurer(
      [width](const std::string&, double) { return Point{width, 20}; });
  CHECK_FALSE(session.Execute("ctx:text_bounds(ctx.state.objects[6])"));
  CHECK(session.Error().find("finite and nonnegative") != std::string::npos);
  CHECK_FALSE(session.Dirty());
}
}  // namespace
}  // namespace drawer
