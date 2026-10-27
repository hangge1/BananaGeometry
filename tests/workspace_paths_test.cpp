#include <filesystem>

#include <catch2/catch_test_macros.hpp>

#include "app/application.h"
#include "app/workspace_paths.h"
#include "support/paths.h"

namespace drawer {
TEST_CASE("Default case directories always belong to the current executable",
          "[app][workspace]") {
  testing::TestDirectory directory;
  const auto runtime = directory.Path() / "out/Drawer";
  testing::WriteScript(directory.Path() / ".git", "gitdir: source checkout");
  testing::WriteScript(directory.Path() / "CMakeLists.txt", "project(Drawer)");
  CHECK(ResolveCaseDirectory(runtime) == runtime / "cases");
  CHECK(ResolveCaseDirectory(runtime, {}, directory.Path() / "remembered") ==
        directory.Path() / "remembered");
}
TEST_CASE(
    "Explicit case directories override remembered paths without probing the "
    "filesystem",
    "[app][workspace]") {
  testing::TestDirectory directory;
  const auto selected = directory.Path() / L"中文 用例";
  CHECK(ResolveCaseDirectory(directory.Path(), selected,
                             directory.Path() / "old") == selected);
  CHECK(ResolveCaseDirectory(directory.Path(),
                             std::filesystem::relative(selected)) == selected);
}
TEST_CASE("An unavailable remembered directory can be recovered through the UI",
          "[app][workspace]") {
  testing::TestDirectory directory;
  std::filesystem::create_directory(directory.Path() / "sdk");
  std::filesystem::copy(testing::SourceRoot() / "sdk", directory.Path() / "sdk",
                        std::filesystem::copy_options::recursive);
  const auto missing = directory.Path() / "missing";
  Application application(directory.Path(), missing);
  CHECK(application.CaseDirectory() == missing);
  CHECK_FALSE(application.Error().empty());
  CHECK_FALSE(application.Active());
  const auto valid = directory.Path() / "valid empty cases";
  std::filesystem::create_directory(valid);
  CHECK(application.SetCaseDirectory(valid));
  CHECK(application.Error().empty());
}
}  // namespace drawer
