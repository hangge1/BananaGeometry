#ifndef DRAWER_SRC_PLATFORM_PREFERENCES_H_
#define DRAWER_SRC_PLATFORM_PREFERENCES_H_

#include <filesystem>

namespace drawer {
struct Preferences {
  // Empty means bundled cases, resolved relative to the current executable.
  std::filesystem::path case_directory;
};
// Missing files return defaults; malformed files and I/O failures throw.
Preferences ReadPreferences(const std::filesystem::path& path);
// Replaces atomically. A failed write leaves the prior file unchanged.
void WritePreferences(const std::filesystem::path& path,
                      const Preferences& preferences);
}  // namespace drawer

#endif  // DRAWER_SRC_PLATFORM_PREFERENCES_H_
