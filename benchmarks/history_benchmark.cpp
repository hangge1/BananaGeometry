#include "benchmark/benchmark.h"
#include "core/history.h"
#include "support.h"

namespace drawer::measuring {
namespace {
void HistoryCommit(benchmark::State& state) {
  const auto document = TriangleDocument(static_cast<int>(state.range(0)));
  for ([[maybe_unused]] auto iteration : state) {
    // Reset and prepare input outside timing. Push includes taking ownership,
    // detecting a real change, serializing its size and storing the snapshot.
    state.PauseTiming();
    {
      History history;
      history.Reset(document.state);
      auto changed = document.state;
      changed["params"]["revision"] = 1;
      state.ResumeTiming();
      history.Push(std::move(changed), "parameter change");
      auto count = history.Count();
      benchmark::DoNotOptimize(count);
      state.PauseTiming();
    }
    state.ResumeTiming();
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(HistoryCommit)
    ->UseRealTime()
    ->Arg(100)
    ->Arg(1000)
    ->Unit(benchmark::kMillisecond);
}  // namespace
}  // namespace drawer::measuring
