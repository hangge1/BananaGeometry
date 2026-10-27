#ifndef DRAWER_TESTS_TEST_SUPPORT_H_
#define DRAWER_TESTS_TEST_SUPPORT_H_
#include <windows.h>

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "document.h"
#include "session.h"
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
inline Value EmptyState() {
  return Value::Object{
      {"objects", Value::Array{}}, {"params", Value::Object{}}, {"next_id", 1}};
}
inline Value PointerEvent(const char* type, double x = 0, double y = 0,
                          const char* key = "") {
  return Value::Object{
      {"type", type}, {"x", x}, {"y", y}, {"key", key}, {"button", 1}};
}
inline void ExecuteChecked(Session* session, const std::string& source,
                           const std::string& name = "test") {
  const bool success = session->Execute(source, name);
  INFO(session->Error());
  REQUIRE(success);
}
inline void CreateTriangle(Session* session) {
  REQUIRE(session->Event(PointerEvent("down", 0, 0)));
  REQUIRE(session->Event(PointerEvent("down", 100, 0)));
  REQUIRE(session->Event(PointerEvent("down", 50, 100)));
}
inline void WriteScript(const std::filesystem::path& path,
                        const std::string& source) {
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  output.exceptions(std::ios::failbit | std::ios::badbit);
  output << source;
}
inline void RunLuaSpec(Session* session, const char* file) {
  const auto path = SourceRoot() / "tests/lua" / file;
  std::ifstream input(path, std::ios::binary);
  REQUIRE(input.good());
  const std::string source((std::istreambuf_iterator<char>(input)), {});
  ExecuteChecked(session, source, "@" + path.u8string());
}
struct TriangleFixture {
  Session session{SourceRoot() / "sdk/drawer.lua",
                  SourceRoot() / "cases/triangle/main.lua"};
};
}  // namespace drawer::testing
#endif  // DRAWER_TESTS_TEST_SUPPORT_H_
