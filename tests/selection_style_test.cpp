#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "support/gallery_fixture.h"

namespace drawer {
namespace {
const Value kSelectionColor = Value::Array{1, .82, .25, 1};

TEST_CASE_METHOD(
    testing::GalleryFixture,
    "Selected color previews render once and retain alpha and undo",
    "[selection][inspector][lua]") {
  const int id = GENERATE(1, 2, 3, 4, 5, 6, 7, 8, 9);
  const double alpha = GENERATE(1., .35);
  const Value color = Value::Array{.9, .2, .3, alpha};
  session.SetTextMeasurer([](const std::string&) { return Point{120, 20}; });
  REQUIRE(session.SelectObject(id));
  const auto before = session.State();
  REQUIRE(session.ObjectControls(
      [&](const std::string& op, const Value::Array& args) {
        if (op == "color") return std::vector<Value>{color, true, false};
        return std::vector<Value>{args.size() > 1 ? args[1] : Value(), false,
                                  false};
      }));
  REQUIRE(session.Editing());
  CHECK(session.State() == before);
  REQUIRE(session.Draw(1));
  int bodies = 0;
  for (const auto& item : session.DrawItems()) {
    if (item.At("color") == color) ++bodies;
    if (item.At("color") == kSelectionColor) {
      const auto kind = item.At("kind").String();
      CHECK((kind == "rect" || kind == "point"));
      if (kind == "rect") CHECK_FALSE(item.At("fill").Bool());
      if (kind == "point") CHECK(item.At("outline").Bool());
    }
  }
  // The custom cross draws two lines; every other sample has one body.
  CHECK(bodies == (id == 9 ? 2 : 1));
  REQUIRE(session.FinishUiEdit());
  CHECK(session.SelectedObject() == id);
  CHECK(session.State().At("objects").Items()[id - 1].At("color") == color);
  CHECK(session.GetHistory().Count() == 1);
  session.Undo();
  CHECK(session.State() == before);
}

TEST_CASE_METHOD(
    testing::GalleryFixture,
    "Selection frames stay outside actual painted bounds at every zoom",
    "[selection][lua]") {
  const int id = GENERATE(1, 2, 3, 6, 7, 9);
  const double zoom = GENERATE(.5, 1., 4.);
  session.SetTextMeasurer([](const std::string&) { return Point{120, 20}; });
  REQUIRE(session.SelectObject(id));
  REQUIRE(session.Draw(zoom));
  Point lower, upper;
  double gap = 7 / zoom;
  if (id == 1) {
    lower = {-270, 50};
    upper = {-170, 150};
  }
  if (id == 2) {
    lower = {-130, 65};
    upper = {10, 135};
  }
  if (id == 3) {
    lower = {100, 60};
    upper = {220, 140};
  }
  if (id == 6) {
    lower = {100, -65 - 20 / zoom};
    upper = {100 + 120 / zoom, -65};
    gap = 6 / zoom;
  }
  if (id == 7) {
    lower = upper = {120, -110};
    gap = 12 / zoom;
  }
  if (id == 9) {
    lower = {-25, -25};
    upper = {25, 25};
  }
  bool found = false;
  for (const auto& item : session.DrawItems()) {
    if (item.At("color") != kSelectionColor || item.At("kind") != Value("rect"))
      continue;
    found = true;
    const auto& points = item.At("points").Items();
    CHECK(points[0].At("x").Number() == Catch::Approx(lower.x - gap));
    CHECK(points[0].At("y").Number() == Catch::Approx(lower.y - gap));
    CHECK(points[1].At("x").Number() == Catch::Approx(upper.x + gap));
    CHECK(points[1].At("y").Number() == Catch::Approx(upper.y + gap));
  }
  REQUIRE(found);
}

TEST_CASE("Bezier guides are drawn behind the original colored curve",
          "[selection][bezier][lua]") {
  const bool curved = GENERATE(false, true);
  Session session(testing::SourceRoot() / "sdk/drawer.lua",
                  testing::SourceRoot() / "cases/bezier/main.lua");
  const std::string middle = curved ? "{x=0,y=200}," : "";
  testing::ExecuteChecked(
      &session, "ctx:edit('curve',function()ctx:add('bezier',{{x=-100,y=0}," +
                    middle +
                    "{x=100,y=0}},{color={0.9,0.2,0.3,1},width=1})end)");
  REQUIRE(session.SelectObject(1));
  REQUIRE(session.Draw(1));
  int guide = -1, curve = -1;
  for (size_t i = 0; i < session.DrawItems().size(); ++i) {
    const auto& item = session.DrawItems()[i];
    if (item.At("kind") == Value("line")) guide = static_cast<int>(i);
    if (item.At("kind") == Value("bezier")) {
      curve = static_cast<int>(i);
      CHECK(item.At("color") == Value::Array{.9, .2, .3, 1});
    }
    if (item.At("kind") == Value("rect") &&
        item.At("color") == kSelectionColor) {
      CHECK(item.At("points").Items()[1].At("y").Number() ==
            Catch::Approx((curved ? 100 : 0) + 6.5));
    }
  }
  REQUIRE(guide >= 0);
  CHECK(curve > guide);
  const Value sample_color = Value::Array{.3, .9, .8, 1};
  for (const auto& item : session.DrawItems())
    CHECK_FALSE((item.At("kind") == Value("point") &&
                 item.At("color") == sample_color));
  REQUIRE(session.Controls([](const std::string& op, const Value::Array& args) {
    if (op == "checkbox" && args[0].StringOr("") == "显示采样点")
      return std::vector<Value>{true, true, true};
    if (op == "button" || op == "mode_button")
      return std::vector<Value>{false, false, false};
    return std::vector<Value>{args.size() > 1 ? args[1] : Value(), false,
                              false};
  }));
  REQUIRE(session.Draw(1));
  int samples = 0;
  for (const auto& item : session.DrawItems())
    if (item.At("kind") == Value("point") && item.At("color") == sample_color)
      ++samples;
  CHECK(samples == 25);
}
}  // namespace
}  // namespace drawer
