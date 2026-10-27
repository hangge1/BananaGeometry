#include <limits>
#include <catch2/catch_test_macros.hpp>
#include "support/session_fixture.h"
#include "platform/document_io.h"

namespace drawer {
using testing::SourceRoot;
TEST_CASE_METHOD(testing::TriangleFixture,
                 "Documents preserve editable data and view on Unicode paths",
                 "[document][lua]") {
  testing::CreateTriangle(&session);
  session.view = {{-123.45, 678.9}, 2.5};
  testing::TestDirectory directory;
  const auto path = directory.Path() / L"中文文档.drawer.json";
  WriteDocument(path, session.SaveData());
  Session restored(SourceRoot() / "sdk/drawer.lua",
                   SourceRoot() / "cases/triangle/main.lua");
  restored.Restore(ReadDocument(path));
  CHECK(restored.State() == session.State());
  CHECK(restored.view.center.x == -123.45);
  CHECK(restored.view.zoom == 2.5);
  CHECK_FALSE(restored.Dirty());
  CHECK_FALSE(restored.GetHistory().CanUndo());
}
TEST_CASE_METHOD(testing::TriangleFixture,
                 "Incompatible document versions fail before mutation",
                 "[document]") {
  testing::CreateTriangle(&session);
  const auto before = session.State();
  auto data = session.SaveData();
  data.case_version = 99;
  CHECK_THROWS_AS(session.Restore(data), std::runtime_error);
  CHECK(session.State() == before);
}
TEST_CASE_METHOD(testing::TriangleFixture,
                 "Documents reject invalid views and state structures",
                 "[document]") {
  testing::CreateTriangle(&session);
  auto encoded = EncodeDocument(session.SaveData());
  SECTION("Unsupported format") { encoded["format_version"] = 2; }
  SECTION("Invalid zoom") { encoded["view"]["zoom"] = 0; }
  SECTION("Null is not representable in Lua state") {
    encoded["state"]["params"]["bad"] = Value();
  }
  SECTION("Duplicate object ID") {
    auto& objects = encoded["state"]["objects"].Items();
    const auto duplicate = objects.front();
    objects.push_back(duplicate);
  }
  CHECK_THROWS_AS(DecodeDocument(encoded), std::runtime_error);
}
TEST_CASE_METHOD(testing::TriangleFixture,
                 "Invalid writes leave the previous destination intact",
                 "[document]") {
  testing::CreateTriangle(&session);
  testing::TestDirectory directory;
  const auto path = directory.Path() / "saved.drawer.json";
  WriteDocument(path, session.SaveData());
  const auto before = ReadDocument(path).state;
  auto invalid = session.SaveData();
  invalid.state["params"]["number"] = std::numeric_limits<double>::quiet_NaN();
  CHECK_THROWS_AS(WriteDocument(path, invalid), std::runtime_error);
  CHECK(ReadDocument(path).state == before);
}
TEST_CASE("Custom objects remain editable after loading", "[document][lua]") {
  Session gallery(SourceRoot() / "sdk/drawer.lua",
                  SourceRoot() / "cases/gallery/main.lua");
  Session copy(SourceRoot() / "sdk/drawer.lua",
               SourceRoot() / "cases/gallery/main.lua");
  copy.Restore(
      DecodeDocument(ParseJson(ToJson(EncodeDocument(gallery.SaveData())))));
  CHECK(copy.State() == gallery.State());
  CHECK(copy.Draw(1));
  testing::ExecuteChecked(
      &copy,
      "ctx:edit('move',function() "
      "local o=ctx.state.objects[#ctx.state.objects]; "
      "ctx.types.cross.move_handle(ctx,o,1,{x=20,y=30}) end)");
  CHECK(copy.State() != gallery.State());
  copy.Undo();
  CHECK(copy.State() == gallery.State());
}
}  // namespace drawer
