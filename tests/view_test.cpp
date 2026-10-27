#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "core/document.h"

namespace drawer {
using Catch::Matchers::WithinAbs;
TEST_CASE("World coordinates round trip through screen coordinates", "[view]") {
  const View view{{125.4, -87.2}, 3.25};
  const Point size{1000, 720}, world{12.34, 56.78};
  CHECK_THAT(Distance(world, view.ToWorld(view.ToScreen(world, size), size)),
             WithinAbs(0, 1e-10));
  CHECK(view.ToScreen({0, 100}, size).y < view.ToScreen({0, 0}, size).y);
}
TEST_CASE("Zoom keeps its mouse anchor fixed", "[view]") {
  View view{{125.4, -87.2}, 3.25};
  const Point size{1000, 720}, mouse{123, 456};
  const auto anchor = view.ToWorld(mouse, size);
  view.ZoomAt(mouse, size, 2);
  CHECK_THAT(Distance(anchor, view.ToWorld(mouse, size)), WithinAbs(0, 1e-10));
}
TEST_CASE("Pan applies the requested pixel displacement", "[view]") {
  View view{{125.4, -87.2}, 3.25};
  const Point size{1000, 720}, world{12.34, 56.78};
  const auto before = view.ToScreen(world, size);
  view.Pan({10, -25});
  const auto after = view.ToScreen(world, size);
  CHECK_THAT(after.x - before.x, WithinAbs(10, 1e-10));
  CHECK_THAT(after.y - before.y, WithinAbs(-25, 1e-10));
}
TEST_CASE("Zoom stays within supported limits", "[view]") {
  View view;
  view.ZoomAt({0, 0}, {1000, 720}, 1e100);
  CHECK(view.zoom == 10000);
  view.ZoomAt({0, 0}, {1000, 720}, 1e-100);
  CHECK(view.zoom == .0001);
}
TEST_CASE("Segment distance handles projections and degenerate segments",
          "[geometry]") {
  CHECK(SegmentDistance({1, 2}, {0, 0}, {2, 0}) == 2);
  CHECK(SegmentDistance({3, 4}, {0, 0}, {0, 0}) == 5);
}
}  // namespace drawer
