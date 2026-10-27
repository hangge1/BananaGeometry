#ifndef DRAWER_TESTS_SUPPORT_PATHS_H_
#define DRAWER_TESTS_SUPPORT_PATHS_H_
#include <windows.h>

#include <atomic>
#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include "support/artifacts.h"
#include "test_config.h"

namespace drawer::testing {
inline std::filesystem::path SourceRoot() {
  return std::filesystem::u8path(DRAWER_TEST_SOURCE_DIR);
}
// Each test owns its temporary files, even across parallel CTest processes.
class TestDirectory {
 public:
  TestDirectory() {
    static std::atomic<unsigned> sequence{0};
    const auto time =
        std::chrono::steady_clock::now().time_since_epoch().count();
    char run_id[80]{};
    const auto length = GetEnvironmentVariableA("DRAWER_VALIDATION_RUN_ID",
                                                run_id, sizeof(run_id));
    std::string run =
        length > 0 && length < sizeof(run_id) ? run_id : "standalone";
    if (!std::all_of(run.begin(), run.end(), [](unsigned char c) {
          return std::isalnum(c) || c == '-';
        }))
      run = "standalone";
    std::string name = CurrentArtifactTestName().substr(0, 48);
    for (auto& character : name) {
      const auto c = static_cast<unsigned char>(character);
      character = std::isalnum(c) ? static_cast<char>(std::tolower(c)) : '-';
    }
    path_ = std::filesystem::u8path(DRAWER_TEST_OUTPUT_DIR) / run / "ctest" /
            (name + "-" + std::to_string(GetCurrentProcessId()) + "-" +
             std::to_string(time) + "-" + std::to_string(++sequence));
    std::filesystem::create_directories(path_);
    RegisterTemporaryDirectory(path_);
  }
  ~TestDirectory() = default;
  TestDirectory(const TestDirectory&) = delete;
  TestDirectory& operator=(const TestDirectory&) = delete;
  const std::filesystem::path& Path() const { return path_; }

 private:
  std::filesystem::path path_;
};
inline void WriteScript(const std::filesystem::path& path,
                        const std::string& source) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.exceptions(std::ios::failbit | std::ios::badbit);
  output << source;
}
}  // namespace drawer::testing
#endif  // DRAWER_TESTS_SUPPORT_PATHS_H_
