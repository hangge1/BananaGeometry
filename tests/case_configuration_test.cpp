#include <cmath>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "support/session_fixture.h"

namespace drawer {
namespace {
void SetControl(Session* session, const std::string& label, Value value) {
  REQUIRE(session->Controls(
      [&](const std::string& operation,
          const Value::Array& arguments) -> std::vector<Value> {
        if (!arguments.empty() && arguments[0].StringOr("") == label)
          return {value, true, true};
        return {operation == "button"  ? Value(false)
                : arguments.size() > 1 ? arguments[1]
                                       : Value(),
                false, false};
      }));
}
}  // namespace

TEST_CASE_METHOD(testing::TriangleFixture,
                 "Polygon case accepts configurable vertex counts",
                 "[lua][cases][polygon]") {
  const int count = GENERATE(3, 5, 16, 256);
  SetControl(&session, "顶点数", count);
  const auto steps = session.GetHistory().Count();
  for (int i = 0; i < count; ++i) {
    const double angle = i * 6.283185307179586 / count;
    REQUIRE(session.Event(testing::PointerEvent("down", 100 * std::cos(angle),
                                                100 * std::sin(angle))));
  }
  REQUIRE(session.State().At("objects").Items().size() == 1);
  const auto& object = session.State().At("objects").Items()[0];
  CHECK(object.At("kind").String() == "polygon");
  CHECK(object.At("points").Items().size() == count);
  CHECK(session.GetHistory().Count() == steps + 1);
  CHECK(session.Draw(1));
  session.Undo();
  CHECK(session.State().At("objects").Items().empty());
}

TEST_CASE_METHOD(testing::TriangleFixture,
                 "Polygon free mode uses the case finish action",
                 "[lua][cases][polygon]") {
  SetControl(&session, "自由顶点数", true);
  testing::CreateTriangle(&session);
  CHECK(session.State().At("objects").Items().empty());
  REQUIRE(
      session.Controls([](const std::string& operation,
                          const Value::Array& arguments) -> std::vector<Value> {
        const bool finish =
            operation == "button" && arguments[0].StringOr("") == "完成多边形";
        return {finish                 ? Value(true)
                : arguments.size() > 1 ? arguments[1]
                                       : Value(false),
                finish, finish};
      }));
  CHECK(session.State().At("objects").Items().size() == 1);
}

TEST_CASE("Bezier case creates and restores user-selected degrees",
          "[lua][cases][bezier]") {
  const int degree = GENERATE(1, 2, 5, 32);
  Session session(testing::SourceRoot() / "sdk/drawer.lua",
                  testing::SourceRoot() / "cases/bezier/main.lua");
  SetControl(&session, "阶数", degree);
  for (int i = 0; i <= degree; ++i)
    REQUIRE(
        session.Event(testing::PointerEvent("down", i * 10, i % 2 ? 100 : 0)));
  REQUIRE(session.State().At("objects").Items().size() == 1);
  CHECK(session.State().At("objects").Items()[0].At("points").Items().size() ==
        degree + 1);
  testing::ExecuteChecked(&session, "ctx:set_mode('edit')");
  REQUIRE(session.Event(testing::PointerEvent("down", 0, 0)));
  REQUIRE(session.Event(testing::PointerEvent("move", -10, 10)));
  REQUIRE(session.Event(testing::PointerEvent("up", -10, 10)));
  CHECK(session.Draw(1));
  Session copy(testing::SourceRoot() / "sdk/drawer.lua",
               testing::SourceRoot() / "cases/bezier/main.lua");
  copy.Restore(
      DecodeDocument(ParseJson(ToJson(EncodeDocument(session.SaveData())))));
  CHECK(copy.State() == session.State());
}

TEST_CASE("Legacy polygon and cubic settings receive compatible defaults",
          "[lua][cases][document]") {
  const auto directory =
      GENERATE(std::string("triangle"), std::string("bezier"));
  Session session(testing::SourceRoot() / "sdk/drawer.lua",
                  testing::SourceRoot() / "cases" / directory / "main.lua");
  auto legacy = session.SaveData();
  legacy.state["params"] = Value::Object{{"width", 2}};
  session.Restore(legacy);
  CHECK(session.State().At("params").At("width").Number() == 2);
  CHECK(session.State()
            .At("params")
            .At(directory == "triangle" ? "vertices" : "degree")
            .Number() == 3);
  CHECK_FALSE(session.Dirty());
  REQUIRE(session.Controls({}));
}
}  // namespace drawer
