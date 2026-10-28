#include <filesystem>
#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "app/application.h"
#include "app/workspace_paths.h"
#include "imgui.h"
#include "support/session_fixture.h"

namespace drawer {
namespace {
std::filesystem::path PrepareApplication(const std::filesystem::path& root) {
  std::filesystem::create_directories(root);
  std::filesystem::copy(testing::SourceRoot() / "sdk", root / "sdk",
                        std::filesystem::copy_options::recursive);
  const auto folder = root / "cases/triangle";
  std::filesystem::create_directories(folder);
  testing::WriteScript(folder / "labels.lua",
                       "return {name='Original name',caption='Original text'}");
  testing::WriteScript(folder / "main.lua", R"(
    local labels=require('labels')
    return {id='drawer.triangle',name=labels.name,data_version=1,
      on_init=function(c) c.state.params.caption=labels.caption end,
      on_ui=function(c) c.ui.text(labels.caption) end,
      on_draw=function(c) c:text({x=0,y=0},labels.caption) end}
  )");
  return root;
}
struct ApplicationFixture {
  testing::TestDirectory directory;
  std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context{
      ImGui::CreateContext(), ImGui::DestroyContext};
  std::filesystem::path selected_directory;
  std::vector<std::filesystem::path> edited_paths;
  std::vector<std::filesystem::path> remembered_paths;
  bool editor_fails = false;
  bool persistence_fails = false;
  Application application{PrepareApplication(directory.Path()),
                          {},
                          {[this](const auto&) { return selected_directory; },
                           [this](const auto& path) {
                             if (editor_fails)
                               throw std::runtime_error("editor unavailable");
                             edited_paths.push_back(path);
                           },
                           [this](const auto& path) {
                             if (persistence_fails)
                               throw std::runtime_error("settings read-only");
                             remembered_paths.push_back(path);
                           }}};
  ApplicationFixture() {
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1440, 900};
    io.DeltaTime = 1.f / 60;
    io.ConfigInputTrickleEventQueue = false;
    io.Fonts->AddFontDefault();
    REQUIRE(io.Fonts->Build());
    REQUIRE(application.SelectInstance(
        application.ActiveDocument()->active_instance));
    Frame();
  }
  void Frame(Application* target = nullptr) {
    ImGui::NewFrame();
    (target ? target : &application)->Frame();
    ImGui::Render();
  }
  void Reload(Application* target = nullptr) {
    ImGui::GetIO().AddKeyEvent(ImGuiKey_F5, true);
    Frame(target);
    ImGui::GetIO().AddKeyEvent(ImGuiKey_F5, false);
    Frame(target);
  }
  void Rename() {
    testing::WriteScript(directory.Path() / "cases/triangle/labels.lua",
                         "return {name='Renamed case',caption='Updated text'}");
  }
};
TEST_CASE_METHOD(ApplicationFixture,
                 "F5 reload updates script text and the cached case name",
                 "[app][reload]") {
  Rename();
  Reload();
  REQUIRE(application.Active());
  CHECK(application.Active()->Name() == "Renamed case");
  CHECK(application.Active()->State().At("params").At("caption").String() ==
        "Updated text");
  std::string caption;
  REQUIRE(application.Active()->Controls(
      [&](const std::string& operation, const Value::Array& arguments) {
        if (operation == "text") caption = arguments.at(0).String();
        return std::vector<Value>{};
      }));
  CHECK(caption == "Updated text");
  REQUIRE(application.Active()->Draw(1));
  REQUIRE(application.Active()->DrawItems().size() == 1);
  CHECK(application.Active()->DrawItems()[0].At("text").String() ==
        "Updated text");
  REQUIRE(application.Cases().size() == 1);
  CHECK(application.Cases()[0].name == "Renamed case");
  CHECK_FALSE(application.Active()->Dirty());
}
TEST_CASE_METHOD(ApplicationFixture,
                 "Refreshing the case list preserves running document data",
                 "[app][reload]") {
  auto* original = application.Active();
  REQUIRE(original);
  testing::ExecuteChecked(original,
                          "ctx:edit('data',function()"
                          "ctx.state.params.user_data=42 end)");
  const auto state = original->State();
  Rename();
  application.RefreshCases();
  CHECK(application.Active() == original);
  CHECK(original->State() == state);
  CHECK(original->Dirty());
  CHECK(original->Name() == "Original name");
  REQUIRE(application.Cases().size() == 1);
  CHECK(application.Cases()[0].name == "Renamed case");
}
TEST_CASE_METHOD(ApplicationFixture,
                 "A repaired F5 reload clears the previous failure diagnostic",
                 "[app][reload]") {
  auto* original = application.Active();
  testing::WriteScript(directory.Path() / "cases/triangle/labels.lua",
                       "this is invalid Lua!");
  Reload();
  CHECK(application.Active() == original);
  CHECK(application.Cases()[0].name == "Original name");
  CHECK(application.Error().find("labels.lua") != std::string::npos);
  application.RefreshCases();
  CHECK(application.Cases().empty());
  Rename();
  Reload();
  CHECK(application.Active()->Name() == "Renamed case");
  REQUIRE(application.Cases().size() == 1);
  CHECK(application.Cases()[0].name == "Renamed case");
  CHECK(application.Error().empty());
}
TEST_CASE_METHOD(ApplicationFixture,
                 "An explicit case directory loads without its own SDK",
                 "[app][reload][workspace]") {
  const auto runtime = directory.Path() / "out/Drawer";
  PrepareApplication(runtime);
  Application running(runtime, directory.Path() / "cases");
  REQUIRE(running.SelectInstance(running.ActiveDocument()->active_instance));
  Rename();
  Frame(&running);
  Reload(&running);
  REQUIRE(running.Active());
  CHECK(running.Active()->Name() == "Renamed case");
  CHECK(running.Active()->Entry() ==
        directory.Path() / "cases/triangle/main.lua");
  CHECK(running.Active()->Sdk() == runtime / "sdk/drawer.lua");
  Session copied(runtime / "sdk/drawer.lua",
                 runtime / "cases/triangle/main.lua");
  CHECK(copied.Name() == "Original name");
}
TEST_CASE_METHOD(ApplicationFixture,
                 "Choosing a case directory preserves dirty tabs and remembers "
                 "only successful choices",
                 "[app][workspace]") {
  auto* old = application.Active();
  testing::ExecuteChecked(
      old, "ctx:edit('data',function()ctx.state.params.value=42 end)");
  const auto before = old->State();
  CHECK_FALSE(application.OpenCaseDirectory());
  CHECK(remembered_paths.empty());
  selected_directory = directory.Path() / "missing";
  CHECK_FALSE(application.OpenCaseDirectory());
  CHECK(application.CaseDirectory() == directory.Path() / "cases");
  CHECK(remembered_paths.empty());
  selected_directory = directory.Path() / L"空 用例目录";
  std::filesystem::create_directory(selected_directory);
  CHECK(application.OpenCaseDirectory());
  CHECK(application.Cases().empty());
  CHECK(application.Active() == old);
  CHECK(old->State() == before);
  CHECK(old->Dirty());
  REQUIRE(remembered_paths.size() == 1);
  CHECK(remembered_paths.back() == selected_directory);
  CHECK(application.SetCaseDirectory({}));
  CHECK(remembered_paths.back().empty());
}
TEST_CASE_METHOD(ApplicationFixture,
                 "Old tab reloads and editor requests retain their source "
                 "after a directory switch",
                 "[app][reload][editor][workspace]") {
  const auto external = directory.Path() / "external";
  std::filesystem::create_directories(external);
  std::filesystem::copy(directory.Path() / "cases", external / "cases",
                        std::filesystem::copy_options::recursive);
  testing::WriteScript(external / "cases/triangle/labels.lua",
                       "return {name='External case',caption='External text'}");
  REQUIRE(application.SetCaseDirectory(external / "cases"));
  Rename();
  Reload();
  CHECK(application.Active()->Name() == "Renamed case");
  REQUIRE(application.Cases().size() == 1);
  CHECK(application.Cases()[0].name == "External case");
  REQUIRE(application.EditCurrentScript());
  CHECK(edited_paths.back() == directory.Path() / "cases/triangle/main.lua");
  application.NewCase(application.Cases()[0].entry);
  REQUIRE(application.Active());
  CHECK(application.Active()->Name() == "External case");
  CHECK(application.Active()->Sdk() == directory.Path() / "sdk/drawer.lua");
  REQUIRE(application.EditCurrentScript());
  CHECK(edited_paths.back() == external / "cases/triangle/main.lua");
}
TEST_CASE_METHOD(ApplicationFixture,
                 "Editor and preference failures leave drawing data intact",
                 "[app][editor][workspace]") {
  const auto before = application.Active()->State();
  editor_fails = true;
  CHECK_FALSE(application.EditCurrentScript());
  CHECK(application.Error().find("editor unavailable") != std::string::npos);
  CHECK(application.Active()->State() == before);
  CHECK(edited_paths.empty());
  persistence_fails = true;
  const auto external = directory.Path() / "external cases";
  std::filesystem::create_directory(external);
  CHECK(application.SetCaseDirectory(external));
  CHECK(application.CaseDirectory() == external);
  CHECK(application.Error().find("settings read-only") != std::string::npos);
  CHECK(application.Active()->State() == before);
  std::filesystem::remove(application.Active()->Entry());
  CHECK_FALSE(application.EditCurrentScript());
  CHECK(edited_paths.empty());
}
}  // namespace
}  // namespace drawer
