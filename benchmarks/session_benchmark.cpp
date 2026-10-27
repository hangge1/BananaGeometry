#include "benchmark/benchmark.h"
#include "runtime/session.h"
#include "support.h"

namespace drawer::measuring {
namespace {
void LuaDrawExtraction(benchmark::State& state) {
  Session session(SourceRoot() / "sdk/drawer.lua",
                  SourceRoot() / "cases/triangle/main.lua");
  session.Restore(TriangleDocument(static_cast<int>(state.range(0))));
  for ([[maybe_unused]] auto iteration : state) {
    if (!session.Draw(1)) {
      state.SkipWithError(session.Error());
      break;
    }
    auto count = session.DrawItems().size();
    benchmark::DoNotOptimize(count);
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
void LuaHitTestMiss(benchmark::State& state) {
  Session session(SourceRoot() / "sdk/drawer.lua",
                  SourceRoot() / "cases/triangle/main.lua");
  session.Restore(TriangleDocument(static_cast<int>(state.range(0))));
  if (!session.Execute("ctx:set_mode('edit')")) {
    state.SkipWithError(session.Error());
    return;
  }
  const Value event =
      Value::Object{{"type", "down"}, {"button", 1}, {"x", -100}, {"y", -100}};
  for ([[maybe_unused]] auto iteration : state) {
    if (!session.Event(event)) {
      state.SkipWithError(session.Error());
      break;
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(LuaDrawExtraction)
    ->UseRealTime()
    ->Arg(100)
    ->Arg(1000)
    ->Unit(benchmark::kMillisecond);
BENCHMARK(LuaHitTestMiss)
    ->UseRealTime()
    ->Arg(100)
    ->Arg(1000)
    ->Unit(benchmark::kMillisecond);
}  // namespace
}  // namespace drawer::measuring
