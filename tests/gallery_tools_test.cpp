#include <algorithm>
#include <tuple>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "support/session_fixture.h"

namespace drawer {
namespace {
struct GalleryToolFixture {
  Session session{testing::SourceRoot() / "sdk/drawer.lua",
                  testing::SourceRoot() / "cases/gallery/main.lua"};
  void Control(const std::string& label, Value value, bool finished = true) {
    REQUIRE(
        session.Controls([&](const std::string& operation,
                             const Value::Array& args) -> std::vector<Value> {
          if (!args.empty() && args[0].StringOr("") == label)
            return {value, true, finished};
          return {operation == "button" ? Value(false)
                  : args.size() > 1     ? args[1]
                                        : Value(),
                  false, false};
        }));
  }
  void Tool(const std::string& name) {
    bool selected = false;
    REQUIRE(
        session.Controls([&](const std::string& operation,
                             const Value::Array& args) -> std::vector<Value> {
          if (operation == "combo" && args[0].StringOr("") == "图元") {
            const auto& choices = args[2].Items();
            const auto choice = std::find_if(
                choices.begin(), choices.end(),
                [&](const Value& item) { return item.StringOr("") == name; });
            if (choice != choices.end()) {
              selected = true;
              return {static_cast<int>(choice - choices.begin() + 1), true,
                      true};
            }
          }
          return {operation == "button" ? Value(false)
                  : args.size() > 1     ? args[1]
                                        : Value(),
                  false, false};
        }));
    REQUIRE(selected);
  }
};
}  // namespace

TEST_CASE_METHOD(GalleryToolFixture,
                 "Gallery draws every selectable primitive interactively",
                 "[lua][cases][gallery]") {
  const auto [kind, label, count] =
      GENERATE(Catch::Generators::table<std::string, std::string, int>(
          {{"point", "点", 1},
           {"line", "线段", 2},
           {"polyline", "折线", 5},
           {"triangle", "三角形", 3},
           {"rect", "矩形", 2},
           {"circle", "圆", 2},
           {"ellipse", "椭圆", 2},
           {"polygon", "多边形", 5},
           {"bezier", "贝塞尔曲线", 4},
           {"text", "文本", 1},
           {"cross", "自定义十字", 1}}));
  CHECK(session.State().At("objects").Items().empty());
  Tool(label);
  const auto before = session.GetHistory().Count();
  for (int i = 0; i < count; ++i)
    REQUIRE(session.Event(
        testing::PointerEvent("down", double(i * 20), i % 2 ? 40 : 0)));
  REQUIRE(session.State().At("objects").Items().size() == 1);
  CHECK(session.State().At("objects").Items()[0].At("kind").String() == kind);
  CHECK(session.State().At("objects").Items()[0].At("points").Items().size() ==
        count);
  CHECK(session.GetHistory().Count() == before + 1);
  REQUIRE(session.Draw(1));
  Session copy(testing::SourceRoot() / "sdk/drawer.lua",
               testing::SourceRoot() / "cases/gallery/main.lua");
  copy.Restore(
      DecodeDocument(ParseJson(ToJson(EncodeDocument(session.SaveData())))));
  CHECK(copy.State() == session.State());
  CHECK(copy.Draw(1));
  session.Undo();
  CHECK(session.State().At("objects").Items().empty());
}
TEST_CASE_METHOD(
    GalleryToolFixture,
    "Gallery tool switches cancel partial objects and enter drawing mode",
    "[lua][cases][gallery]") {
  Tool("多边形");
  REQUIRE(session.Event(testing::PointerEvent("down", 0, 0)));
  REQUIRE(session.Event(testing::PointerEvent("down", 100, 0)));
  testing::ExecuteChecked(&session, "ctx:set_mode('edit')");
  Tool("圆");
  testing::ExecuteChecked(
      &session, "assert(ctx.mode=='draw' and ctx:drawing_count()==0)");
  REQUIRE(session.Event(testing::PointerEvent("down", 10, 10)));
  REQUIRE(session.Event(testing::PointerEvent("down", 30, 10)));
  REQUIRE(session.State().At("objects").Items().size() == 1);
  CHECK(session.State().At("objects").Items()[0].At("kind").String() ==
        "circle");
  Tool("折线");
  Control("点数", 2);
  Tool("多边形");
  CHECK(session.State().At("params").At("vertices").Number() == 3);
}
TEST_CASE_METHOD(GalleryToolFixture,
                 "Gallery closes text edits before switching tools",
                 "[lua][cases][gallery][ui]") {
  Tool("文本");
  Control("文字", "changed", false);
  REQUIRE(session.Editing());
  Tool("线段");
  CHECK_FALSE(session.Editing());
  CHECK(session.State().At("params").At("text").String() == "changed");
  CHECK(session.State().At("params").At("kind").String() == "line");
  session.Undo();
  CHECK(session.State().At("params").At("kind").String() == "text");
  CHECK(session.State().At("params").At("text").String() == "changed");
}
TEST_CASE_METHOD(GalleryToolFixture,
                 "Gallery reference scene is an explicit undoable action",
                 "[lua][cases][gallery]") {
  Control("插入演示图元", true);
  CHECK(session.State().At("objects").Items().size() == 9);
  CHECK(session.GetHistory().Count() == 1);
  session.Undo();
  CHECK(session.State().At("objects").Items().empty());
}

TEST_CASE_METHOD(GalleryToolFixture,
                 "Gallery honors degree and free construction controls",
                 "[lua][cases][gallery]") {
  Tool("贝塞尔曲线");
  Control("阶数", 5);
  for (int i = 0; i < 6; ++i)
    REQUIRE(
        session.Event(testing::PointerEvent("down", i * 20, i % 2 ? 50 : 0)));
  REQUIRE(session.State().At("objects").Items().size() == 1);
  CHECK(session.State().At("objects").Items()[0].At("points").Items().size() ==
        6);
  Tool("折线");
  Control("自由点数", true);
  REQUIRE(session.Event(testing::PointerEvent("down", 0, 0)));
  REQUIRE(session.Event(testing::PointerEvent("down", 20, 20)));
  REQUIRE(session.Event(
      Value::Object{{"type", "down"}, {"button", 2}, {"x", 500}, {"y", 500}}));
  CHECK(session.State().At("objects").Items().size() == 2);
}
TEST_CASE_METHOD(GalleryToolFixture,
                 "Gallery does not create invisible empty text",
                 "[lua][cases][gallery]") {
  Tool("文本");
  Control("文字", "");
  REQUIRE(session.Event(testing::PointerEvent("down", 0, 0)));
  CHECK(session.State().At("objects").Items().empty());
}
TEST_CASE_METHOD(
    GalleryToolFixture,
    "Gallery text size affects new text independently of existing labels",
    "[lua][text][gallery]") {
  Tool("文本");
  bool saw_size = false, saw_width = false;
  REQUIRE(
      session.Controls([&](const std::string& op, const Value::Array& args) {
        const auto label = args.empty() ? std::string{} : args[0].StringOr("");
        saw_size |= label == "新文字字号(px)";
        saw_width |= label == "新图元线宽";
        return std::vector<Value>{op == "button"    ? Value(false)
                                  : args.size() > 1 ? args[1]
                                                    : Value(),
                                  false, false};
      }));
  CHECK(saw_size);
  CHECK_FALSE(saw_width);
  Control("新文字字号(px)", 36);
  REQUIRE(session.Event(testing::PointerEvent("down", 0, 0)));
  REQUIRE(session.State().At("objects").Items().size() == 1);
  CHECK(session.State().At("objects").Items()[0].At("font_size").Number() ==
        36);
  CHECK(session.State().At("objects").Items()[0].At("width").IsNull());
  Control("新文字字号(px)", 60);
  REQUIRE(session.Event(testing::PointerEvent("down", 100, 0)));
  REQUIRE(session.State().At("objects").Items().size() == 2);
  CHECK(session.State().At("objects").Items()[0].At("font_size").Number() ==
        36);
  CHECK(session.State().At("objects").Items()[1].At("font_size").Number() ==
        60);
}
}  // namespace drawer
