#include <filesystem>

#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>

#include "app/application.h"
#include "app/workspace_paths.h"
#include "support/paths.h"

namespace drawer {
namespace {
void MakeCheckout(const std::filesystem::path& root, bool worktree) {
  std::filesystem::create_directories(root / "sdk");
  std::filesystem::create_directories(root / "cases");
  testing::WriteScript(root / "sdk/drawer.lua", "return {}");
  testing::WriteScript(root / "CMakeLists.txt", "project(Drawer)");
  if (worktree)
    testing::WriteScript(root / ".git", "gitdir: unused-test-directory");
  else
    std::filesystem::create_directory(root / ".git");
}
TEST_CASE("Executables inside a checkout use its source scripts",
          "[app][workspace]") {
  const bool worktree = GENERATE(false, true);
  testing::TestDirectory directory;
  MakeCheckout(directory.Path(), worktree);
  const auto nested = directory.Path() / "nested checkout";
  MakeCheckout(nested, worktree);
  const auto executable = nested / "build/bin/Release";
  std::filesystem::create_directories(executable);
  CHECK(ResolveWorkspaceRoot(executable) == nested);
  // A coincidental scripts folder is not a repository boundary.
  std::filesystem::create_directories(executable / "sdk");
  std::filesystem::create_directory(executable / "cases");
  testing::WriteScript(executable / "sdk/drawer.lua", "return {}");
  CHECK(ResolveWorkspaceRoot(executable) == nested);
  CHECK(ResolveWorkspaceRoot(directory.Path() / "out/Drawer") ==
        directory.Path());
}
TEST_CASE("Portable paths and explicit workspaces are independent of the CWD",
          "[app][workspace]") {
  testing::TestDirectory directory;
  // No files are created outside the test's temporary root. A synthetic path
  // outside any checkout is sufficient to test executable-relative selection.
  const auto portable =
      directory.Path().root_path() / directory.Path().filename() / "portable";
  CHECK(ResolveWorkspaceRoot(portable) == portable);
  CHECK(ResolveWorkspaceRoot(testing::SourceRoot() / "out/Drawer", portable) ==
        portable);
  const auto relative = std::filesystem::relative(directory.Path());
  CHECK(ResolveWorkspaceRoot(portable, relative) == directory.Path());
}
TEST_CASE("An invalid explicit workspace is reported instead of falling back",
          "[app][workspace]") {
  testing::TestDirectory directory;
  const auto missing = directory.Path() / "missing workspace";
  const auto root = ResolveWorkspaceRoot(testing::SourceRoot(), missing);
  CHECK(root == missing);
  CHECK_THROWS_AS(Application(root), std::runtime_error);
}
}  // namespace
}  // namespace drawer
