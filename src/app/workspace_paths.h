#ifndef DRAWER_SRC_APP_WORKSPACE_PATHS_H_
#define DRAWER_SRC_APP_WORKSPACE_PATHS_H_

#include <filesystem>

namespace drawer {
// Prefer an explicit workspace, then the nearest enclosing source checkout,
// otherwise the executable's directory. Resolution never depends on the CWD
// unless an explicit relative workspace is supplied. Application validates
// assets.
std::filesystem::path ResolveWorkspaceRoot(
    const std::filesystem::path& executable_directory,
    const std::filesystem::path& explicit_workspace = {});
}  // namespace drawer

#endif  // DRAWER_SRC_APP_WORKSPACE_PATHS_H_
