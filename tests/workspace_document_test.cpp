#include <catch2/catch_test_macros.hpp>
#include <memory>
#include "platform/document_io.h"
#include "support/session_fixture.h"
namespace drawer {
namespace {
std::unique_ptr<Session> Runtime() {
  return std::make_unique<Session>(
      testing::SourceRoot() / "sdk/drawer.lua",
      testing::SourceRoot() / "cases/triangle/main.lua");
}
}  // namespace
TEST_CASE(
    "Workspace codec imports legacy snapshots and round trips multiple "
    "instances",
    "[workspace][document]") {
  auto r = Runtime();
  testing::CreateTriangle(r.get());
  const auto legacy = r->SaveData();
  auto imported = DecodeWorkspace(EncodeDocument(legacy));
  REQUIRE(imported.imported_legacy);
  REQUIRE(imported.instances.size() == 1);
  CHECK(imported.instances[0].data.state == legacy.state);
  imported.name = "中文文档";
  imported.instances.push_back({2, "另一实例", legacy});
  imported.instances[1].data.view.zoom = 3;
  const auto restored = DecodeWorkspace(
      ParseJson(ToJson(EncodeWorkspace(imported), JsonFormat::kIndented)));
  CHECK_FALSE(restored.imported_legacy);
  CHECK(restored.name == imported.name);
  CHECK(restored.instances.size() == 2);
  CHECK(restored.instances[1].data.view.zoom == 3);
  testing::TestDirectory directory;
  const auto path = directory.Path() / "workspace.drawer.json";
  WriteWorkspace(path, restored);
  CHECK(ReadWorkspace(path).instances[0].data.state == legacy.state);
}
TEST_CASE("Workspace rejects malformed names identities and instance data",
          "[workspace][document]") {
  auto r = Runtime();
  WorkspaceDocument d;
  d.instances = {{1, "one", r->SaveData()}};
  SECTION("Duplicate IDs") { d.instances.push_back(d.instances[0]); }
  SECTION("Empty name") { d.name = ""; }
  SECTION("Embedded null") { d.instances[0].name = std::string("a\0b", 3); }
  SECTION("Invalid ID") { d.instances[0].id = 0; }
  SECTION("Invalid version") { d.instances[0].data.case_version = 0; }
  CHECK_THROWS(EncodeWorkspace(d));
}
}  // namespace drawer
