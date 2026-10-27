#include "render/canvas_renderer.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "core/bezier.h"

namespace drawer {
namespace {
ImU32 Color(const Value& shape, float alpha = 1) {
  const auto& c = shape.At("color");
  ImVec4 result(.35f, .7f, 1, alpha);
  if (c.IsArray() && c.Items().size() >= 3) {
    result = {static_cast<float>(c.Items()[0].NumberOr(.35)),
              static_cast<float>(c.Items()[1].NumberOr(.7)),
              static_cast<float>(c.Items()[2].NumberOr(1)),
              alpha * static_cast<float>(
                          c.Items().size() > 3 ? c.Items()[3].NumberOr(1) : 1)};
  }
  return ImGui::ColorConvertFloat4ToU32(result);
}
}  // namespace
void RenderCanvas(const View& view, const std::vector<Value>& items,
                  ImVec2 origin, ImVec2 size) {
  auto* draw = ImGui::GetWindowDrawList();
  draw->PushClipRect(origin, {origin.x + size.x, origin.y + size.y}, true);
  draw->AddRectFilled(origin, {origin.x + size.x, origin.y + size.y},
                      IM_COL32(21, 27, 38, 255));
  auto screen = [&](Point point) {
    auto p = view.ToScreen(point, {size.x, size.y});
    return ImVec2(origin.x + static_cast<float>(p.x),
                  origin.y + static_cast<float>(p.y));
  };
  const double base = std::pow(10.0, std::floor(std::log10(80 / view.zoom)));
  const double multiplier = 80 / (view.zoom * base);
  const double step = base * (multiplier > 5 ? 10 : multiplier > 2 ? 5 : 2);
  auto lower = view.ToWorld({0, size.y}, {size.x, size.y});
  auto upper = view.ToWorld({size.x, 0}, {size.x, size.y});
  for (double x = std::ceil(lower.x / step) * step; x <= upper.x; x += step) {
    auto p = screen({x, 0});
    draw->AddLine({p.x, origin.y}, {p.x, origin.y + size.y},
                  IM_COL32(35, 43, 57, 255));
  }
  for (double y = std::ceil(lower.y / step) * step; y <= upper.y; y += step) {
    auto p = screen({0, y});
    draw->AddLine({origin.x, p.y}, {origin.x + size.x, p.y},
                  IM_COL32(35, 43, 57, 255));
  }
  auto zero = screen({0, 0});
  draw->AddLine({zero.x, origin.y}, {zero.x, origin.y + size.y},
                IM_COL32(65, 80, 100, 255), 1.5f);
  draw->AddLine({origin.x, zero.y}, {origin.x + size.x, zero.y},
                IM_COL32(65, 80, 100, 255), 1.5f);
  for (const auto& item : items) {
    const auto& source = item.At("points");
    if (!source.IsArray() || source.Items().empty()) continue;
    std::vector<ImVec2> points;
    std::vector<Point> world;
    for (const auto& p : source.Items()) {
      Point point{p.At("x").NumberOr(0), p.At("y").NumberOr(0)};
      world.push_back(point);
      points.push_back(screen(point));
    }
    auto kind = item.At("kind").StringOr("");
    const float width =
        static_cast<float>(std::clamp(item.At("width").NumberOr(2), .5, 32.0));
    const bool fill = item.At("fill").IsBool() && item.At("fill").Bool();
    const ImU32 color = Color(item), fill_color = Color(item, .20f);
    if (kind == "point")
      draw->AddCircleFilled(points[0],
                            static_cast<float>(std::clamp(
                                item.At("radius").NumberOr(5), 1.0, 64.0)),
                            color);
    else if (kind == "text")
      draw->AddText(points[0], color, item.At("text").StringOr("").c_str());
    else if (kind == "bezier" && world.size() >= 2 &&
             world.size() <= kMaxBezierDegree + 1) {
      points.clear();
      for (const auto& point : FlattenBezier(world, .75 / view.zoom))
        points.push_back(screen(point));
      draw->AddPolyline(points.data(), static_cast<int>(points.size()), color,
                        0, width);
    } else if (kind == "rect" && points.size() >= 2) {
      ImVec2 a(std::min(points[0].x, points[1].x),
               std::min(points[0].y, points[1].y));
      ImVec2 b(std::max(points[0].x, points[1].x),
               std::max(points[0].y, points[1].y));
      if (fill) draw->AddRectFilled(a, b, fill_color);
      draw->AddRect(a, b, color, 0, 0, width);
    } else if ((kind == "circle" || kind == "ellipse") && points.size() >= 2) {
      double rx = kind == "circle" ? Distance(world[0], world[1]) * view.zoom
                                   : std::abs(points[1].x - points[0].x);
      double ry = kind == "circle" ? rx : std::abs(points[1].y - points[0].y);
      if (rx > 1e7 || ry > 1e7) continue;
      std::vector<ImVec2> ring;
      constexpr int kSegments = 96;
      for (int i = 0; i < kSegments; ++i) {
        const double angle = i * 6.283185307179586 / kSegments;
        ring.emplace_back(
            points[0].x + static_cast<float>(rx * std::cos(angle)),
            points[0].y + static_cast<float>(ry * std::sin(angle)));
      }
      if (fill) draw->AddConvexPolyFilled(ring.data(), kSegments, fill_color);
      draw->AddPolyline(ring.data(), kSegments, color, ImDrawFlags_Closed,
                        width);
    } else if (kind == "line" || kind == "polyline" || kind == "triangle" ||
               kind == "polygon") {
      bool closed = kind == "triangle" || kind == "polygon";
      if (fill && closed && points.size() >= 3)
        draw->AddConcavePolyFilled(points.data(),
                                   static_cast<int>(points.size()), fill_color);
      draw->AddPolyline(points.data(), static_cast<int>(points.size()), color,
                        closed ? ImDrawFlags_Closed : 0, width);
    }
  }
  draw->PopClipRect();
}
}  // namespace drawer
