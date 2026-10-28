#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <limits>
#include <memory>

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "app/canvas_view.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "render/text_metrics.h"
#include "support/session_fixture.h"
#include "support/gallery_fixture.h"

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
  double measured_font_size = 0;

  CanvasFixture() {
    testing::PrepareGallery(&session);
    auto& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = {1000, 760};
    io.DeltaTime = 1.f / 60;
    io.ConfigInputTrickleEventQueue = false;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
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

  void Frame(Point pointer = {-1000, -1000}, bool pressed = false,
             bool shift = false, bool input_enabled = true,
             bool text_input = false, bool interaction_enabled = true) {
    auto& io = ImGui::GetIO();
    io.AddMousePosEvent(static_cast<float>(pointer.x),
                        static_cast<float>(pointer.y));
    io.AddMouseButtonEvent(0, pressed);
    io.AddKeyEvent(ImGuiMod_Shift, shift);
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
    text_size = MeasureCanvasText("二维图形 SDK##id\nNext", measured_font_size);
    io.WantTextInput = text_input;
    canvas.Draw(&session, input_enabled, interaction_enabled);
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
TEST_CASE_METHOD(CanvasFixture,
                 "Canvas captures whole object drags beyond its edges",
                 "[app][translation]") {
  const bool outside = GENERATE(false, true);
  const double zoom = GENERATE(.5, 2.);
  session.view.zoom = zoom;
  const auto before = session.State();
  Frame();
  const auto pointer = Screen({160, 100});
  Frame(pointer);
  CHECK(ImGui::GetMouseCursor() == ImGuiMouseCursor_ResizeAll);
  Frame(pointer, true);
  const Point moved{outside ? 1200 : pointer.x + 40, pointer.y + 24};
  Frame(moved, true);
  Frame(moved);
  REQUIRE_FALSE(session.Editing());
  auto expected = before;
  for (auto& point : expected["objects"].Items()[2]["points"].Items()) {
    point["x"] = point.At("x").Number() + (moved.x - pointer.x) / zoom;
    point["y"] = point.At("y").Number() - 24 / zoom;
  }
  CHECK(session.State() == expected);
  CHECK(session.view.center.x == 0);
  CHECK(session.view.center.y == 0);
  CHECK(session.GetHistory().Count() == 1);
}
TEST_CASE_METHOD(
    CanvasFixture,
    "Explicit text sizes match rendered multiline bounds and picking",
    "[app][render][text]") {
  const int font_size = GENERATE(12, 32);
  const double zoom = GENERATE(.5, 2.);
  ImGui::GetIO().FontGlobalScale = GENERATE(1.f, 1.75f);
  session.view.zoom = zoom;
  measured_font_size = font_size;
  testing::ExecuteChecked(
      &session,
      "ctx:edit('font',function()local o=ctx.state.objects[6];"
      "o.text='二维图形 SDK##id\\nNext';o.font_size=" +
          std::to_string(font_size) + " end)");
  const auto before = session.State();
  const auto history = session.GetHistory().Count();
  Frame();
  CHECK(text_size.y == Catch::Approx(font_size * 2));
  // Inspect actual glyph geometry, so measuring a larger box alone cannot pass.
  const auto* draw = ImGui::FindWindowByName("Canvas test")->DrawList;
  const auto color = ImGui::ColorConvertFloat4ToU32({.8f, .85f, 1, 1});
  float top = std::numeric_limits<float>::max(), bottom = -top;
  for (const auto& vertex : draw->VtxBuffer)
    if (vertex.col == color) {
      top = std::min(top, vertex.pos.y);
      bottom = std::max(bottom, vertex.pos.y);
    }
  CHECK(bottom - top > font_size * 1.25f);
  CHECK(bottom - top <= font_size * 2.1f);
  const auto bounds = session.ObjectBounds(6);
  CHECK(bounds.Items()[1].At("x").Number() ==
        Catch::Approx(100 + text_size.x / zoom));
  CHECK(bounds.Items()[1].At("y").Number() ==
        Catch::Approx(-65 - text_size.y / zoom));
  const auto pointer =
      Screen({100 + (text_size.x - 3) / zoom, -65 - (text_size.y - 3) / zoom});
  Frame(pointer);
  Frame(pointer, true);
  REQUIRE(session.SelectedObject() == 6);
  const Point moved{pointer.x + 24, pointer.y + 18};
  Frame(moved, true);
  Frame(moved);
  const auto& anchor =
      session.State().At("objects").Items()[5].At("points").Items()[0];
  CHECK(anchor.At("x").Number() == Catch::Approx(100 + 24 / zoom));
  CHECK(anchor.At("y").Number() == Catch::Approx(-65 - 18 / zoom));
  CHECK(session.GetHistory().Count() == history + 1);
  session.Undo();
  CHECK(session.State() == before);
}
TEST_CASE_METHOD(CanvasFixture,
                 "Canvas keyboard nudges respect selection and navigation",
                 "[app][translation]") {
  Frame();
  const auto pointer = Screen({160, 100});
  Frame(pointer);
  Frame(pointer, true);
  Frame(pointer);
  const double before = session.State()
                            .At("objects")
                            .Items()[2]
                            .At("points")
                            .Items()[0]
                            .At("x")
                            .Number();
  ImGui::GetIO().AddKeyEvent(ImGuiKey_RightArrow, true);
  Frame(pointer);
  CHECK(session.State()
            .At("objects")
            .Items()[2]
            .At("points")
            .Items()[0]
            .At("x")
            .Number() == before + 1);
  ImGui::GetIO().AddKeyEvent(ImGuiKey_RightArrow, false);
  Frame(pointer);
}
TEST_CASE_METHOD(
    CanvasFixture,
    "Drawing crosshairs follow the pointer only inside an enabled canvas",
    "[app][mode][cursor]") {
  testing::ExecuteChecked(&session, "ctx:set_mode('draw')");
  Frame();
  const auto pointer = Screen({0, 0});
  Frame(pointer);
  CHECK(canvas.CrosshairVisible());
  CHECK(ImGui::GetMouseCursor() == ImGuiMouseCursor_None);
  CHECK(ImGui::GetForegroundDrawList()->VtxBuffer.Size > 0);
  Frame(pointer, false, false, false);
  CHECK_FALSE(canvas.CrosshairVisible());
  Frame(pointer, false, false, true, true);
  CHECK_FALSE(canvas.CrosshairVisible());
  Frame();
  CHECK_FALSE(canvas.CrosshairVisible());
  CHECK(ImGui::GetMouseCursor() == ImGuiMouseCursor_Arrow);
  testing::ExecuteChecked(&session,
                          "ctx:register_mode('inspect',{label='Inspect',cursor="
                          "'hand'});ctx:set_mode('inspect')");
  Frame(pointer);
  CHECK_FALSE(canvas.CrosshairVisible());
  CHECK(ImGui::GetMouseCursor() == ImGuiMouseCursor_Hand);
}
TEST_CASE_METHOD(
    CanvasFixture,
    "Temporary pan cursors restore the drawing mode and honor input focus",
    "[app][mode][cursor]") {
  testing::ExecuteChecked(&session, "ctx:set_mode('draw')");
  Frame();
  const auto pointer = Screen({0, 0});
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Space, true);
  Frame(pointer);
  CHECK_FALSE(canvas.CrosshairVisible());
  CHECK(ImGui::GetMouseCursor() == ImGuiMouseCursor_ResizeAll);
  CHECK(session.GetModeFeedback().id == "draw");
  const auto center = session.view.center;
  Frame({pointer.x + 30, pointer.y + 20}, true, false, false);
  CHECK(session.view.center.x == center.x);
  CHECK(session.view.center.y == center.y);
  ImGui::GetIO().AddKeyEvent(ImGuiKey_Space, false);
  Frame(pointer);
  CHECK(canvas.CrosshairVisible());
}
TEST_CASE_METHOD(CanvasFixture, "Mode overlays do not consume drawing clicks",
                 "[app][mode]") {
  testing::ExecuteChecked(&session, "ctx:set_mode('draw')");
  const auto before = session.State().At("objects").Items().size();
  Frame();
  const Point first{origin.x + 25, origin.y + 25};
  const Point second{origin.x + 100, origin.y + 25};
  Frame(first);
  Frame(first, true);
  Frame(first);
  Frame(second);
  Frame(second, true);
  Frame(second);
  CHECK(session.State().At("objects").Items().size() == before + 1);
}
TEST_CASE_METHOD(CanvasFixture,
                 "Document preview suppresses case input and drawing cursors "
                 "but permits zoom",
                 "[app][preview][cursor]") {
  testing::ExecuteChecked(&session, "ctx:set_mode('draw')");
  const auto before = session.State();
  const auto history = session.GetHistory().Count();
  Frame({}, false, false, true, false, false);
  const auto pointer = Screen({0, 0});
  Frame(pointer, true, false, true, false, false);
  Frame(pointer, false, false, true, false, false);
  CHECK_FALSE(canvas.CrosshairVisible());
  CHECK(ImGui::GetMouseCursor() == ImGuiMouseCursor_Arrow);
  ImGui::GetIO().AddMouseWheelEvent(0, 1);
  Frame(pointer, false, false, true, false, false);
  CHECK(session.view.zoom > 1);
  CHECK(session.State() == before);
  CHECK(session.GetHistory().Count() == history);
  Frame(pointer);
  CHECK(canvas.CrosshairVisible());
}
TEST_CASE_METHOD(CanvasFixture,
                 "Selected point centers remain visible through hollow handles",
                 "[app][render][selection]") {
  REQUIRE(session.SelectObject(7));
  Frame();
  const auto center = Screen({120, -110});
  auto* window = ImGui::FindWindowByName("Canvas test");
  REQUIRE(window);
  const auto* draw = window->DrawList;
  auto covers_center = [&](ImU32 color) {
    for (int i = 0; i + 2 < draw->IdxBuffer.Size; i += 3) {
      const auto& a = draw->VtxBuffer[draw->IdxBuffer[i]];
      const auto& b = draw->VtxBuffer[draw->IdxBuffer[i + 1]];
      const auto& c = draw->VtxBuffer[draw->IdxBuffer[i + 2]];
      if (a.col == color && b.col == color && c.col == color &&
          ImTriangleContainsPoint(a.pos, b.pos, c.pos,
                                  {float(center.x), float(center.y)}))
        return true;
    }
    return false;
  };
  CHECK(covers_center(ImGui::ColorConvertFloat4ToU32({1, .65f, .3f, 1})));
  CHECK_FALSE(
      covers_center(ImGui::ColorConvertFloat4ToU32({1, .82f, .25f, 1})));
}
}  // namespace
}  // namespace drawer
