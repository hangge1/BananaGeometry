#ifndef DRAWER_SRC_APP_WORKSPACE_PATHS_H_
#define DRAWER_SRC_APP_WORKSPACE_PATHS_H_

#include <filesystem>

namespace drawer {
// Explicit directory wins over the remembered choice, then bundled cases.
// Does not inspect ancestors or require a copy of the host SDK in the
// directory.
std::filesystem::path ResolveCaseDirectory(
    const std::filesystem::path& executable_directory,
    const std::filesystem::path& explicit_directory = {},
    const std::filesystem::path& remembered_directory = {});
}  // namespace drawer

#endif  // DRAWER_SRC_APP_WORKSPACE_PATHS_H_
