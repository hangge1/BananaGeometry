#include <catch2/catch_test_macros.hpp>
#include "app/application.h"
#include "platform/document_io.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "support/session_fixture.h"
namespace drawer {
TEST_CASE(
    "Saving or switching keeps unfinished property input and cancels drafts",
    "[app][workspace][inspector]") {
  Application app(testing::SourceRoot());
  CHECK_FALSE(app.ActiveDocument()->Dirty());
  REQUIRE(app.SelectInstance(app.ActiveDocument()->active_instance));
  auto* first = app.Active();
  testing::CreateTriangle(first);
  REQUIRE(first->SelectObject(1));
  auto rename = [&] {
    REQUIRE(
        first->ObjectControls([](const std::string&, const Value::Array& args) {
          if (args[0].StringOr("") == "名称")
            return std::vector<Value>{"未失焦输入", true, false};
          return std::vector<Value>{args.size() > 1 ? args[1] : Value(), false,
                                    false};
        }));
    REQUIRE(first->Editing());
  };
  rename();
  testing::TestDirectory dir;
  auto path = dir.Path() / "input.drawer.json";
  REQUIRE(app.SaveDocument(path));
  CHECK(ReadWorkspace(path)
            .instances[0]
            .data.state.At("objects")
            .Items()[0]
            .At("display_name")
            .String() == "未失焦输入");
  CHECK(first->SelectedObject() == 1);
  rename();
  app.NewDocument();
  CHECK_FALSE(first->Editing());
  const auto before = first->State();
  testing::ExecuteChecked(
      first, "ctx:begin_edit('preview');ctx.state.objects[1].width=30");
  REQUIRE(first->FinishUiEdit());
  CHECK(first->State() == before);
}
TEST_CASE(
    "Application saves all instances and imports legacy without overwriting it",
    "[app][workspace]") {
  Application app(testing::SourceRoot());
  REQUIRE(app.SelectInstance(app.ActiveDocument()->active_instance));
  auto* first = app.Active();
  REQUIRE(first);
  testing::CreateTriangle(first);
  app.NewCase(testing::SourceRoot() / "cases/bezier/main.lua");
  auto* second = app.Active();
  REQUIRE(second);
  CHECK(second != first);
  CHECK(app.ActiveDocument()->Instances().size() == 2);
  testing::TestDirectory dir;
  auto path = dir.Path() / "all.drawer.json";
  REQUIRE(app.SaveDocument(path));
  CHECK_FALSE(app.ActiveDocument()->Dirty());
  REQUIRE(app.OpenDocument(path));
  CHECK(app.ActiveDocument()->Instances().size() == 2);
  CHECK(app.ActiveDocument()->Instances()[0].runtime->session->State() ==
        first->State());
  const auto old = dir.Path() / "legacy.drawer.json";
  WriteDocument(old, first->SaveData());
  REQUIRE(app.OpenDocument(old));
  CHECK(app.ActiveDocument()->legacy);
  CHECK(app.ActiveDocument()->Instances().size() == 1);
  REQUIRE(app.SaveDocument(dir.Path() / "converted.drawer.json"));
  CHECK_FALSE(app.ActiveDocument()->legacy);
  CHECK(ReadDocument(old).state == first->State());
}
TEST_CASE(
    "Missing plugin instances stay intact and can be retried from an explicit "
    "library",
    "[app][workspace]") {
  Application app(testing::SourceRoot());
  WorkspaceDocument data;
  auto original = app.Preview()->SaveData();
  data.instances = {{1, "available", original}, {2, "missing", original}};
  data.instances[1].data.case_id = "missing.plugin";
  testing::TestDirectory dir;
  auto file = dir.Path() / "missing.drawer.json";
  WriteWorkspace(file, data);
  REQUIRE(app.OpenDocument(file));
  auto* d = app.ActiveDocument();
  CHECK(d->Find(1)->runtime->session);
  CHECK_FALSE(d->Find(2)->runtime->session);
  CHECK_FALSE(d->Find(2)->runtime->error.empty());
  REQUIRE(app.SaveDocument(file));
  CHECK(ReadWorkspace(file).instances[1].data.state == original.state);
  auto cases = dir.Path() / "cases/missing";
  std::filesystem::create_directories(cases);
  testing::WriteScript(
      cases / "main.lua",
      "return "
      "{id='missing.plugin',name='Installed',data_version=1,edit_mode='edit'}");
  REQUIRE(app.SetCaseDirectory(cases.parent_path()));
  REQUIRE(app.SelectInstance(2));
  REQUIRE(app.RetryInstance());
  CHECK(app.Active()->State() == original.state);
  CHECK_FALSE(d->Dirty());
  CHECK(d->Find(1)->runtime->session);
}
TEST_CASE(
    "A failed instance restore preserves raw data without disrupting other "
    "instances",
    "[app][workspace]") {
  Application app(testing::SourceRoot());
  WorkspaceDocument data;
  data.instances = {{1, "good", app.Preview()->SaveData()},
                    {2, "bad version", app.Preview()->SaveData()}};
  data.instances[1].data.case_version = 99;
  testing::TestDirectory dir;
  const auto file = dir.Path() / "versions.drawer.json";
  WriteWorkspace(file, data);
  REQUIRE(app.OpenDocument(file));
  auto* d = app.ActiveDocument();
  CHECK(d->Find(1)->runtime->session);
  CHECK_FALSE(d->Find(2)->runtime->session);
  REQUIRE(app.SelectInstance(2));
  CHECK_FALSE(app.RetryInstance());
  REQUIRE(app.SaveDocument(file));
  CHECK(ReadWorkspace(file).instances[1].data.case_version == 99);
}
TEST_CASE("Menus and Ctrl N create a document rather than a hidden case picker",
          "[app][workspace][ui]") {
  auto* context = ImGui::CreateContext();
  struct Cleanup {
    ImGuiContext* context;
    ~Cleanup() { ImGui::DestroyContext(context); }
  } cleanup{context};
  auto& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.DisplaySize = {1440, 900};
  io.DeltaTime = 1.f / 60;
  io.ConfigInputTrickleEventQueue = false;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  io.Fonts->AddFontDefault();
  REQUIRE(io.Fonts->Build());
  Application app(testing::SourceRoot());
  auto frame = [&] {
    ImGui::NewFrame();
    app.Frame();
    ImGui::Render();
  };
  frame();
  auto* original = app.ActiveDocument();
  auto* window = ImGui::FindWindowByName("Drawer");
  REQUIRE(window);
  CHECK((window->Flags & ImGuiWindowFlags_MenuBar) != 0);
  auto rect = window->MenuBarRect();
  io.AddMousePosEvent(rect.Min.x + 20, rect.Min.y + 8);
  io.AddMouseButtonEvent(0, true);
  frame();
  io.AddMouseButtonEvent(0, false);
  frame();
  CHECK(context->OpenPopupStack.Size > 0);
  io.AddKeyEvent(ImGuiKey_Escape, true);
  frame();
  io.AddKeyEvent(ImGuiKey_Escape, false);
  frame();
  io.AddMousePosEvent(700, 500);
  frame();
  REQUIRE(context->OpenPopupStack.Size == 0);
  io.AddKeyEvent(ImGuiMod_Ctrl, true);
  io.AddKeyEvent(ImGuiKey_N, true);
  frame();
  io.AddKeyEvent(ImGuiKey_N, false);
  io.AddKeyEvent(ImGuiMod_Ctrl, false);
  frame();
  CHECK(app.ActiveDocument() != original);
  CHECK(app.ActiveDocument()->Instances().empty());
  CHECK(app.Active() == nullptr);
}
}  // namespace drawer
