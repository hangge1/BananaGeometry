#ifndef DRAWER_SRC_PLATFORM_DESKTOP_ACTIONS_H_
#define DRAWER_SRC_PLATFORM_DESKTOP_ACTIONS_H_

#include <filesystem>

namespace drawer {
// Empty result means canceled; native failures throw. The CWD is unchanged.
std::filesystem::path ChooseCaseDirectory(
    const std::filesystem::path& initial_directory);
// Uses the shell's edit verb, falling back to Notepad if no editor is
// associated. Does not use the open verb on a script, which could execute an
// interpreter.
void EditScriptFile(const std::filesystem::path& path);
// Location only; does not create or read the preferences file.
std::filesystem::path LocalPreferencesPath();
}  // namespace drawer

#endif  // DRAWER_SRC_PLATFORM_DESKTOP_ACTIONS_H_
