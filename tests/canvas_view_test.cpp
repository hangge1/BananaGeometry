#include <windows.h>

#include <filesystem>
#include <memory>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "app/canvas_view.h"
#include "imgui.h"
#include "render/text_metrics.h"
#include "support/session_fixture.h"

namespace drawer {
namespace {
struct CanvasFixture {
  std::unique_ptr<ImGuiContext, decltype(&ImGui::DestroyContext)> context{
      ImGui::CreateContext(), ImGui::DestroyContext};
  Session session{testing::SourceRoot() / "sdk/drawer.lua",
                  testing::SourceRoot() / "cases/gallery/main.lua"};
  CanvasView canvas;
  Point origin;
  Point size;
  Point text_size;

  CanvasFixture() {
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1000, 760};
    io.DeltaTime = 1.f / 60;
    io.ConfigInputTrickleEventQueue = false;
    // Match the application's CJK font when available, with only test glyphs.
    wchar_t system_root[MAX_PATH]{};
    if (GetWindowsDirectoryW(system_root, MAX_PATH)) {
      const auto font = std::filesystem::path(system_root) / L"Fonts/msyh.ttc";
      if (std::filesystem::exists(font)) {
        static const ImWchar kRanges[] = {0x20,   0x7e,   0x4e8c, 0x4e8c,
                                          0x56fe, 0x56fe, 0x5f62, 0x5f62,
                                          0x7ef4, 0x7ef4, 0};
        io.Fonts->AddFontFromFileTTF(font.u8string().c_str(), 17, nullptr,
                                     kRanges);
      }
    }
    if (io.Fonts->Fonts.empty()) io.Fonts->AddFontDefault();
    REQUIRE(io.Fonts->Build());
  }

  void Frame(Point pointer = {-1000, -1000}, bool pressed = false) {
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(static_cast<float>(pointer.x),
                        static_cast<float>(pointer.y));
    io.AddMouseButtonEvent(0, pressed);
    ImGui::NewFrame();
    ImGui::SetNextWindowPos({0, 0});
    ImGui::SetNextWindowSize(io.DisplaySize);
    ImGui::Begin("Canvas test", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);
    const auto cursor = ImGui::GetCursorScreenPos();
    const auto available = ImGui::GetContentRegionAvail();
    origin = {cursor.x, cursor.y};
    size = {available.x, available.y};
    text_size = MeasureCanvasText("二维图形 SDK##id\nNext");
    canvas.Draw(&session, true);
    ImGui::End();
    ImGui::Render();
  }

  Point Screen(Point world) const {
    const auto local = session.view.ToScreen(world, size);
    return {local.x + origin.x, local.y + origin.y};
  }
};

TEST_CASE_METHOD(
    CanvasFixture,
    "Canvas text uses its displayed font bounds for mouse dragging",
    "[app][render][text]") {
  const double zoom = GENERATE(.5, 2.);
  const float font_scale = GENERATE(1.f, 1.75f);
  ImGui::GetIO().FontGlobalScale = font_scale;
  session.view.zoom = zoom;
  testing::ExecuteChecked(&session, R"(
    ctx:edit('multiline label',function()
      ctx.state.objects[6].text='二维图形 SDK##id\nNext'
    end)
  )");
  const auto before = session.State();
  const auto history_count = session.GetHistory().Count();
  Frame();
  REQUIRE(text_size.x > 70 * font_scale);
  REQUIRE(text_size.y > 20 * font_scale);
  const auto pointer =
      Screen({100 + (text_size.x - 5) / zoom, -65 - (text_size.y - 5) / zoom});
  Frame(pointer);
  Frame(pointer, true);
  REQUIRE(session.Editing());
  testing::ExecuteChecked(&session, "assert(ctx.temp.selected==6)");
  const Point moved{pointer.x + 32, pointer.y + 24};
  Frame(moved, true);
  Frame(moved);
  REQUIRE_FALSE(session.Editing());
  const auto& anchor =
      session.State().At("objects").Items()[5].At("points").Items()[0];
  CHECK(anchor.At("x").Number() == Catch::Approx(100 + 32 / zoom));
  CHECK(anchor.At("y").Number() == Catch::Approx(-65 - 24 / zoom));
  CHECK(session.GetHistory().Count() == history_count + 1);
  CHECK(ImGui::GetDrawData()->TotalVtxCount > 0);
  session.Undo();
  CHECK(session.State() == before);
}
}  // namespace
}  // namespace drawer
