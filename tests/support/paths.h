#ifndef DRAWER_TESTS_SUPPORT_PATHS_H_
#define DRAWER_TESTS_SUPPORT_PATHS_H_
#include <windows.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
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
    path_ = std::filesystem::u8path(DRAWER_TEST_OUTPUT_DIR) /
            ("test-" + std::to_string(GetCurrentProcessId()) + "-" +
             std::to_string(time) + "-" + std::to_string(++sequence));
    std::filesystem::create_directories(path_);
  }
  ~TestDirectory() {
    std::error_code ignored;
    std::filesystem::remove_all(path_, ignored);
  }
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
