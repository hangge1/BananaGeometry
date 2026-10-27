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
  Application application{PrepareApplication(directory.Path())};
  ApplicationFixture() {
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1440, 900};
    io.DeltaTime = 1.f / 60;
    io.ConfigInputTrickleEventQueue = false;
    io.Fonts->AddFontDefault();
    REQUIRE(io.Fonts->Build());
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
                 "F5 inside a checkout reads source instead of stale copies",
                 "[app][reload][workspace]") {
  testing::WriteScript(directory.Path() / ".git", "gitdir: test-worktree");
  testing::WriteScript(directory.Path() / "CMakeLists.txt", "project(Drawer)");
  const auto executable_directory = directory.Path() / "out/Drawer";
  PrepareApplication(executable_directory);
  Application running(ResolveWorkspaceRoot(executable_directory));
  Frame(&running);
  Rename();
  Reload(&running);
  REQUIRE(running.Active());
  CHECK(running.Active()->Name() == "Renamed case");
  CHECK(running.Active()->Entry() ==
        directory.Path() / "cases/triangle/main.lua");
  Session copied(executable_directory / "sdk/drawer.lua",
                 executable_directory / "cases/triangle/main.lua");
  CHECK(copied.Name() == "Original name");
}
}  // namespace
}  // namespace drawer
