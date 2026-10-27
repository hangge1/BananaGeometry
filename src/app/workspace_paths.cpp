#include "app/workspace_paths.h"

namespace drawer {
std::filesystem::path ResolveCaseDirectory(
    const std::filesystem::path& executable_directory,
    const std::filesystem::path& explicit_directory,
    const std::filesystem::path& remembered_directory) {
  const auto selected = !explicit_directory.empty() ? explicit_directory
                        : !remembered_directory.empty()
                            ? remembered_directory
                            : executable_directory / "cases";
  return std::filesystem::absolute(selected).lexically_normal();
}
}  // namespace drawer
