#ifndef DRAWER_SRC_APP_DESKTOP_SERVICES_H_
#define DRAWER_SRC_APP_DESKTOP_SERVICES_H_

#include <filesystem>
#include <functional>

namespace drawer {
// UI-thread operations. Empty picker/editor use native implementations;
// an empty remember callback disables persistence for isolated hosts and tests.
struct DesktopServices {
  std::function<std::filesystem::path(const std::filesystem::path&)>
      choose_directory;
  std::function<void(const std::filesystem::path&)> edit_script;
  // Empty path records "use the current executable's bundled cases".
  std::function<void(const std::filesystem::path&)> remember_directory;
};
}  // namespace drawer

#endif  // DRAWER_SRC_APP_DESKTOP_SERVICES_H_
