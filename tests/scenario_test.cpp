#include <fstream>

#include <catch2/catch_test_macros.hpp>

#include "support/session_fixture.h"

namespace drawer {
TEST_CASE(
    "Repository visual scenarios execute through the reusable fixture API",
    "[scenarios][lua]") {
  const auto root = testing::SourceRoot();
  std::ifstream input(root / "tests/scenarios/manifest.json", std::ios::binary);
  REQUIRE(input.good());
  const std::string source((std::istreambuf_iterator<char>(input)), {});
  const auto manifest = ParseJson(source);
  for (const auto& scene : manifest.At("scenes").Items()) {
    CAPTURE(scene.At("id").String());
    bool found = false;
    for (const auto& entry : DiscoverCases(root / "cases")) {
      Session session(root / "sdk/drawer.lua", entry);
      if (session.Id() != scene.At("case_id").String()) continue;
      found = true;
      const bool loaded = session.ExecuteFile(
          root / "tests/scenarios" /
          std::filesystem::u8path(scene.At("fixture").String()));
      INFO(session.Error());
      REQUIRE(loaded);
      CHECK(session.Draw(1));
      break;
    }
    REQUIRE(found);
  }
}
TEST_CASE_METHOD(testing::TriangleFixture,
                 "Fixture files retain diagnostics and reject excessive input",
                 "[scenarios][lua]") {
  testing::TestDirectory directory;
  const auto file = directory.Path() / "fixture-error.lua";
  const auto before = session.State();
  testing::WriteScript(file,
                       "ctx:edit('rollback',function()ctx.state.params.bad=1;"
                       "error('fixture sentinel')end)");
  CHECK_FALSE(session.ExecuteFile(file));
  CHECK(session.Error().find("fixture-error.lua") != std::string::npos);
  CHECK(session.State() == before);
  testing::WriteScript(file, std::string(1024 * 1024 + 1, ' '));
  CHECK_FALSE(session.ExecuteFile(file));
  CHECK(session.Error().find("exceeds 1 MiB") != std::string::npos);
  CHECK(session.State() == before);
}
}  // namespace drawer
