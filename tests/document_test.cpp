#include <fstream>
#include <iterator>
#include <limits>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "support/session_fixture.h"
#include "support/gallery_fixture.h"
#include "platform/document_io.h"

namespace drawer {
using testing::SourceRoot;
namespace {
std::string ReadText(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  REQUIRE(input.good());
  return std::string(std::istreambuf_iterator<char>(input), {});
}
}  // namespace
TEST_CASE_METHOD(testing::TriangleFixture,
                 "Documents preserve editable data and view on Unicode paths",
                 "[document][lua]") {
  testing::CreateTriangle(&session);
  session.view = {{-123.45, 678.9}, 2.5};
  testing::TestDirectory directory;
  const auto path = directory.Path() / L"中文文档.drawer.json";
  WriteDocument(path, session.SaveData());
  const auto text = ReadText(path);
  CHECK(text.substr(0, 2) == "{\n");
  CHECK(text.find("\n  \"case_id\": \"") != std::string::npos);
  CHECK(text.substr(text.size() - 2) == "}\n");
  CHECK(text.find('\r') == std::string::npos);
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
                 "Compact documents load and save again as readable UTF-8 JSON",
                 "[document][json]") {
  testing::CreateTriangle(&session);
  testing::ExecuteChecked(&session, R"(
    ctx:edit('label',function()ctx.state.params.label='中文 [a,b]: {"x":1}'end)
  )");
  testing::TestDirectory directory;
  const auto path = directory.Path() / "legacy.drawer.json";
  const auto document = session.SaveData();
  const auto compact = ToJson(EncodeDocument(document));
  REQUIRE(compact.find('\n') == std::string::npos);
  testing::WriteScript(path, compact);
  const auto loaded = ReadDocument(path);
  WriteDocument(path, loaded);
  const auto text = ReadText(path);
  CHECK(text.find('\n') != std::string::npos);
  CHECK(text.find("中文") != std::string::npos);
  CHECK(text.substr(0, 3) != "\xef\xbb\xbf");
  CHECK(ParseJson(text) == EncodeDocument(document));
  Session restored(SourceRoot() / "sdk/drawer.lua",
                   SourceRoot() / "cases/triangle/main.lua");
  restored.Restore(ReadDocument(path));
  CHECK(restored.State() == session.State());
  CHECK_FALSE(restored.Dirty());
}
TEST_CASE_METHOD(
    testing::TriangleFixture,
    "Indented output size limits preserve the existing destination",
    "[document][json]") {
  testing::CreateTriangle(&session);
  testing::TestDirectory directory;
  const auto path = directory.Path() / "saved.drawer.json";
  WriteDocument(path, session.SaveData());
  const auto before = ReadText(path);
  auto data = session.SaveData();
  data.state["params"]["padding"] = "";
  constexpr size_t kLimit = 16 * 1024 * 1024;
  const auto overhead = ToJson(EncodeDocument(data)).size();
  data.state["params"]["padding"] = std::string(kLimit - overhead, 'x');
  REQUIRE(ToJson(EncodeDocument(data)).size() == kLimit);
  CHECK_THROWS_AS(WriteDocument(path, data), std::runtime_error);
  CHECK(ReadText(path) == before);
  CHECK(ReadDocument(path).state == session.State());
  CHECK(std::distance(std::filesystem::directory_iterator(directory.Path()),
                      std::filesystem::directory_iterator()) == 1);
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
  testing::PrepareGallery(&gallery);
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
