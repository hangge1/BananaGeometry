#include "platform/preferences.h"

#include <windows.h>

#include <atomic>
#include <fstream>
#include <stdexcept>
#include <string>

#include "core/value.h"

namespace drawer {
Preferences ReadPreferences(const std::filesystem::path& path) {
  if (!std::filesystem::exists(path)) return {};
  if (std::filesystem::file_size(path) > 64 * 1024)
    throw std::runtime_error("Preferences exceed 64 KiB");
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("Cannot read preferences");
  const std::string text((std::istreambuf_iterator<char>(input)), {});
  const auto value = ParseJson(text);
  if (!value.IsObject() || value.At("version") != Value(1) ||
      !value.At("case_directory").IsString())
    throw std::runtime_error("Invalid preferences format");
  const auto& directory = value.At("case_directory").String();
  if (directory.find('\0') != std::string::npos)
    throw std::runtime_error("Invalid case directory in preferences");
  auto result = std::filesystem::u8path(directory);
  if (!result.empty() && !result.is_absolute())
    throw std::runtime_error("Remembered case directory must be absolute");
  return {std::move(result)};
}
void WritePreferences(const std::filesystem::path& path,
                      const Preferences& preferences) {
  auto directory = preferences.case_directory;
  if (!directory.empty())
    directory = std::filesystem::absolute(directory).lexically_normal();
  const auto text = ToJson(
      Value::Object{{"version", 1}, {"case_directory", directory.u8string()}});
  if (text.size() > 64 * 1024)
    throw std::runtime_error("Preferences exceed 64 KiB");
  std::filesystem::create_directories(path.parent_path());
  static std::atomic<unsigned> sequence{0};
  auto temporary = path;
  temporary += L".tmp." + std::to_wstring(GetCurrentProcessId()) + L"." +
               std::to_wstring(++sequence);
  try {
    {
      std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
      output.exceptions(std::ios::failbit | std::ios::badbit);
      output << text;
      output.flush();
      output.close();
    }
    if (!MoveFileExW(temporary.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
      throw std::runtime_error("Cannot replace preferences (Windows error " +
                               std::to_string(GetLastError()) + ")");
  } catch (...) {
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    throw;
  }
}
}  // namespace drawer
