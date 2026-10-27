#include "app/workspace_paths.h"

namespace drawer {
std::filesystem::path ResolveWorkspaceRoot(
    const std::filesystem::path& executable_directory,
    const std::filesystem::path& explicit_workspace) {
  if (!explicit_workspace.empty())
    return std::filesystem::absolute(explicit_workspace).lexically_normal();
  const auto executable_root =
      std::filesystem::absolute(executable_directory).lexically_normal();
  for (auto directory = executable_root;; directory = directory.parent_path()) {
    // A .git file also supports worktrees; arbitrary parent folders with cases
    // are not enough to redirect a portable installation's script source.
    if (std::filesystem::exists(directory / ".git") &&
        std::filesystem::is_regular_file(directory / "CMakeLists.txt") &&
        std::filesystem::is_regular_file(directory / "sdk/drawer.lua") &&
        std::filesystem::is_directory(directory / "cases"))
      return directory;
    if (directory == directory.parent_path()) break;
  }
  return executable_root;
}
}  // namespace drawer
