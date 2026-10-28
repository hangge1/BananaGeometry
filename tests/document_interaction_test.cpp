#include <memory>

#include <catch2/catch_test_macros.hpp>

#include "app/application.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "platform/document_io.h"
#include "support/session_fixture.h"

namespace drawer {
namespace {
struct DocumentInteractionFixture {
  std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context{
      ImGui::CreateContext(), ImGui::DestroyContext};
  Application app{testing::SourceRoot()};

  DocumentInteractionFixture() {
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1440, 900};
    io.DeltaTime = 1.f / 60;
    io.ConfigInputTrickleEventQueue = false;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.Fonts->AddFontDefault();
    REQUIRE(io.Fonts->Build());
    Frame();
  }
  void Frame() {
    ImGui::NewFrame();
    app.Frame();
    ImGui::Render();
  }
  ImGuiWindow* Child(const char* id) {
    auto* root = ImGui::FindWindowByName("Drawer");
    REQUIRE(root);
    for (auto* window : context->Windows)
      if (window->ParentWindow == root && window->ChildId == root->GetID(id))
        return window;
    FAIL("Application child window not found");
  }
  void Click(ImVec2 point) {
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(point.x, point.y);
    io.AddMouseButtonEvent(0, true);
    Frame();
    io.AddMouseButtonEvent(0, false);
    Frame();
  }
  // Rows use unframed tree nodes below the manager title and separator.
  void ClickTreeRow(int row) {
    auto* manager = Child("manager");
    const auto& style = ImGui::GetStyle();
    const auto origin = manager->DC.CursorStartPos;
    const float line = ImGui::GetTextLineHeightWithSpacing();
    Click({origin.x + 100, origin.y + line + 1 + style.ItemSpacing.y +
                               row * line + ImGui::GetFontSize() * .5f});
  }
  void DrawTriangle() {
    const auto center = Child("view")->InnerRect.GetCenter();
    Click({center.x - 90, center.y + 70});
    Click({center.x + 90, center.y + 70});
    Click({center.x, center.y - 70});
  }
  void Key(ImGuiKey key, bool ctrl = false) {
    auto& io = ImGui::GetIO();
    io.AddKeyEvent(ImGuiMod_Ctrl, ctrl);
    io.AddKeyEvent(key, true);
    Frame();
    io.AddKeyEvent(key, false);
    io.AddKeyEvent(ImGuiMod_Ctrl, false);
    Frame();
  }
};

TEST_CASE_METHOD(
    DocumentInteractionFixture,
    "Loading and selecting document nodes require explicit instance activation",
    "[app][workspace][preview][ui]") {
  CHECK_FALSE(app.Active());
  REQUIRE(app.Preview());
  Session source(testing::SourceRoot() / "sdk/drawer.lua",
                 testing::SourceRoot() / "cases/triangle/main.lua");
  testing::CreateTriangle(&source);
  WorkspaceDocument document;
  document.name = "Loaded document";
  document.instances = {{1, "Polygon instance", source.SaveData()}};
  testing::TestDirectory directory;
  auto path = directory.Path() / "preview.drawer.json";
  WriteWorkspace(path, document);
  REQUIRE(app.OpenDocument(path));
  Frame();
  Frame();
  CHECK_FALSE(app.Active());
  auto* session = app.Preview();
  REQUIRE(session);
  const auto loaded = session->State();
  DrawTriangle();
  Key(ImGuiKey_F5);
  CHECK(app.Preview() == session);
  CHECK(session->State() == loaded);
  CHECK_FALSE(app.ActiveDocument()->Dirty());
  CHECK(ImGui::GetMouseCursor() == ImGuiMouseCursor_Arrow);

  // The startup document and its empty instance occupy the first two rows.
  ClickTreeRow(3);
  REQUIRE(app.Active() == session);
  DrawTriangle();
  REQUIRE(session->State().At("objects").Items().size() == 2);
  ClickTreeRow(4);
  REQUIRE(app.Active() == session);
  CHECK(session->SelectedObject() == 1);
  CHECK(session->GetModeFeedback().id == "edit");
  const auto edited = session->State();
  const auto history = session->GetHistory().Count();
  testing::ExecuteChecked(
      session, "ctx:begin_edit('draft');ctx.state.objects[1].width=25");
  REQUIRE(session->Editing());
  ClickTreeRow(2);
  CHECK_FALSE(app.Active());
  CHECK(app.Preview() == session);
  CHECK_FALSE(session->Editing());
  CHECK(session->SelectedObject() == 0);
  DrawTriangle();
  Key(ImGuiKey_Z, true);
  Key(ImGuiKey_Y, true);
  Key(ImGuiKey_Delete);
  Key(ImGuiKey_F5);
  CHECK(app.Preview() == session);
  CHECK(session->State() == edited);
  CHECK(session->GetHistory().Count() == history);
  CHECK_FALSE(app.SelectInstance(999));
  CHECK_FALSE(app.Active());
  ClickTreeRow(3);
  CHECK(app.Active() == session);
  CHECK(session->GetModeFeedback().id == "edit");
}
}  // namespace
}  // namespace drawer
