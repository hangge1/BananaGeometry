#include <catch2/catch_test_macros.hpp>
#include "app/working_document.h"
#include "platform/document_io.h"
#include "support/session_fixture.h"
namespace drawer {
namespace {
std::shared_ptr<RuntimeInstance> Runtime() {
  auto r = std::make_shared<RuntimeInstance>();
  r->session = std::make_unique<Session>(
      testing::SourceRoot() / "sdk/drawer.lua",
      testing::SourceRoot() / "cases/triangle/main.lua");
  return r;
}
}  // namespace
TEST_CASE(
    "Workspace codec imports legacy snapshots and round trips multiple "
    "instances",
    "[workspace][document]") {
  auto r = Runtime();
  testing::CreateTriangle(r->session.get());
  const auto legacy = r->session->SaveData();
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
  d.instances = {{1, "one", r->session->SaveData()}};
  SECTION("Duplicate IDs") { d.instances.push_back(d.instances[0]); }
  SECTION("Empty name") { d.name = ""; }
  SECTION("Embedded null") { d.instances[0].name = std::string("a\0b", 3); }
  SECTION("Invalid ID") { d.instances[0].id = 0; }
  SECTION("Invalid version") { d.instances[0].data.case_version = 0; }
  CHECK_THROWS(EncodeWorkspace(d));
}
TEST_CASE(
    "Document management undo preserves geometry edits and deleted session "
    "history",
    "[workspace][history]") {
  WorkingDocument d("document");
  auto r = Runtime();
  auto* session = r->session.get();
  const int id = d.Add("instance", r);
  d.MarkSaved();
  d.RenameInstance(id, "renamed");
  testing::CreateTriangle(session);
  CHECK(d.Undo());
  CHECK(d.Find(id)->name == "instance");
  CHECK(session->State().At("objects").Items().size() == 1);
  const auto history = session->GetHistory().Count();
  d.Remove(id);
  CHECK(d.Instances().empty());
  CHECK(d.Undo());
  CHECK(d.Find(id)->runtime->session.get() == session);
  CHECK(session->GetHistory().Count() == history);
  session->Undo();
  CHECK_FALSE(d.Dirty());
  d.Redo();
  CHECK(d.Instances().empty());
  CHECK(d.Dirty());
}
TEST_CASE(
    "Management save points ignore views and IDs are never reused after "
    "branching",
    "[workspace][history]") {
  WorkingDocument d("document");
  const int first = d.Add("one", Runtime());
  d.MarkSaved();
  d.Find(first)->runtime->session->view.zoom = 3;
  CHECK_FALSE(d.Dirty());
  d.Rename("other");
  CHECK(d.Dirty());
  d.Undo();
  CHECK_FALSE(d.Dirty());
  const int second = d.Add("two", Runtime());
  d.Undo();
  const int third = d.Add("three", Runtime());
  CHECK(third > second);
  CHECK_FALSE(d.CanRedo());
  CHECK(d.Instances().size() == 2);
  CHECK(d.Find(first));
}
TEST_CASE("Bounded management history releases deleted runtimes",
          "[workspace][history]") {
  WorkingDocument d("document");
  auto r = Runtime();
  std::weak_ptr<RuntimeInstance> weak = r;
  int id = d.Add("temporary", r);
  r.reset();
  d.Remove(id);
  CHECK_FALSE(weak.expired());
  for (int i = 0; i < 110; ++i) d.Rename("document " + std::to_string(i));
  CHECK(weak.expired());
  int undo = 0;
  while (d.Undo()) ++undo;
  CHECK(undo <= 100);
}
TEST_CASE(
    "Unavailable instances survive saving and management undo without Lua",
    "[workspace][document]") {
  auto r = Runtime();
  auto missing = std::make_shared<RuntimeInstance>();
  missing->unavailable = r->session->SaveData();
  missing->unavailable.case_id = "missing.plugin";
  missing->error = "not installed";
  WorkingDocument d("document");
  int id = d.Add("missing", missing);
  d.Add("available", r);
  d.MarkSaved();
  d.Remove(id);
  d.Undo();
  auto data = d.Snapshot();
  CHECK(data.instances[0].data.case_id == "missing.plugin");
  CHECK_FALSE(d.Dirty());
  CHECK(DecodeWorkspace(EncodeWorkspace(data)).instances.size() == 2);
}
}  // namespace drawer
