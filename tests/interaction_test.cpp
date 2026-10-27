#include <catch2/catch_test_macros.hpp>

#include "support/session_fixture.h"

namespace drawer {
namespace {
using testing::ExecuteChecked;
using testing::PointerEvent;

struct GalleryFixture {
  Session session{testing::SourceRoot() / "sdk/drawer.lua",
                  testing::SourceRoot() / "cases/gallery/main.lua"};
};

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
  CHECK_FALSE(session.Editing());
  REQUIRE(session.Draw(1));
  int outlines = 0;
  int handles = 0;
  for (const auto& item : session.DrawItems()) {
    if (!IsHighlight(item)) continue;
    if (item.At("kind").StringOr("") == "rect") {
      ++outlines;
      CHECK_FALSE(item.At("fill").Bool());
      CHECK(item.At("width").Number() > 2);
    }
    if (item.At("kind").StringOr("") == "point") ++handles;
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
  for (const auto& item : session.DrawItems()) {
    if (IsHighlight(item) && item.At("kind").StringOr("") == "line")
      ++highlighted_lines;
  }
  CHECK(highlighted_lines == 2);
  CHECK(session.State() == before);
  CHECK(session.GetHistory().Count() == 0);
  ExecuteChecked(&session, "ctx:set_mode('draw')");
  REQUIRE(session.Draw(1));
  for (const auto& item : session.DrawItems()) CHECK_FALSE(IsHighlight(item));
}
}  // namespace
}  // namespace drawer
