#include "benchmark/benchmark.h"
#include "core/document.h"
#include "support.h"

namespace drawer::measuring {
namespace {
void DocumentEncode(benchmark::State& state) {
  const auto document = TriangleDocument(static_cast<int>(state.range(0)));
  const auto bytes = ToJson(EncodeDocument(document)).size();
  for ([[maybe_unused]] auto iteration : state) {
    auto json = ToJson(EncodeDocument(document));
    benchmark::DoNotOptimize(json);
  }
  state.SetBytesProcessed(state.iterations() * static_cast<int64_t>(bytes));
}
void DocumentDecode(benchmark::State& state) {
  const auto document = TriangleDocument(static_cast<int>(state.range(0)));
  const auto json = ToJson(EncodeDocument(document));
  for ([[maybe_unused]] auto iteration : state) {
    auto decoded = DecodeDocument(ParseJson(json));
    benchmark::DoNotOptimize(decoded);
  }
  state.SetBytesProcessed(state.iterations() *
                          static_cast<int64_t>(json.size()));
}
BENCHMARK(DocumentEncode)
    ->UseRealTime()
    ->Arg(100)
    ->Arg(1000)
    ->Unit(benchmark::kMillisecond);
BENCHMARK(DocumentDecode)
    ->UseRealTime()
    ->Arg(100)
    ->Arg(1000)
    ->Unit(benchmark::kMillisecond);
}  // namespace
}  // namespace drawer::measuring
