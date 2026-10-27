#ifndef DRAWER_BENCHMARKS_SUPPORT_H_
#define DRAWER_BENCHMARKS_SUPPORT_H_

#include <filesystem>

#include "benchmark_config.h"
#include "core/document.h"

namespace drawer::measuring {
inline std::filesystem::path SourceRoot() {
  return std::filesystem::path(DRAWER_BENCHMARK_SOURCE_DIR);
}

// Deterministic input, built before timing; no dependency on test fixtures.
inline Document TriangleDocument(int count) {
  Value::Array objects;
  objects.reserve(count);
  for (int i = 0; i < count; ++i) {
    const double x = (i % 40) * 10;
    const double y = (i / 40) * 10;
    objects.emplace_back(Value::Object{
        {"id", i + 1},
        {"kind", "triangle"},
        {"points", Value::Array{Value::Object{{"x", x}, {"y", y}},
                                Value::Object{{"x", x + 3}, {"y", y}},
                                Value::Object{{"x", x}, {"y", y + 3}}}}});
  }
  return {"drawer.triangle",
          1,
          Value::Object{{"objects", std::move(objects)},
                        {"params", Value::Object{}},
                        {"next_id", count + 1}},
          {}};
}
}  // namespace drawer::measuring

#endif  // DRAWER_BENCHMARKS_SUPPORT_H_
