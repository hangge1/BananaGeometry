#include <filesystem>
#include <fstream>
#include <string>

#include <catch2/catch_test_macros.hpp>

#include "platform/desktop_actions.h"
#include "platform/preferences.h"
#include "support/paths.h"

namespace drawer {
TEST_CASE("Case directory preferences round trip and reset to bundled cases",
          "[preferences]") {
  testing::TestDirectory directory;
  const auto path = directory.Path() / "settings/preferences.json";
  CHECK(ReadPreferences(path).case_directory.empty());
  const auto cases = directory.Path() / L"中文 用例目录";
  WritePreferences(path, {cases});
  CHECK(ReadPreferences(path).case_directory == cases);
  WritePreferences(path, {});
  CHECK(ReadPreferences(path).case_directory.empty());
  CHECK(std::distance(std::filesystem::directory_iterator(path.parent_path()),
                      std::filesystem::directory_iterator{}) == 1);
}
TEST_CASE("Broken preferences are rejected without rewriting the file",
          "[preferences]") {
  testing::TestDirectory directory;
  const auto path = directory.Path() / "preferences.json";
  for (const auto& source :
       {std::string("not json"),
        std::string("{\"version\":2,\"case_directory\":\"\"}"),
        std::string("{\"version\":1,\"case_directory\":\"relative\"}"),
        std::string(64 * 1024 + 1, ' ')}) {
    testing::WriteScript(path, source);
    CHECK_THROWS(ReadPreferences(path));
    std::ifstream input(path, std::ios::binary);
    CHECK(std::string((std::istreambuf_iterator<char>(input)), {}) == source);
  }
}
TEST_CASE("Panel preferences coexist with directory settings and older files",
          "[preferences]") {
  testing::TestDirectory directory;
  const auto path = directory.Path() / "preferences.json";
  testing::WriteScript(path, "{\"version\":1,\"case_directory\":\"\"}");
  CHECK(ReadPreferences(path).layout.IsObject());
  Preferences value;
  value.case_directory = directory.Path();
  value.layout = Value::Object{{"tree", false}, {"tree_width", 300}};
  WritePreferences(path, value);
  auto restored = ReadPreferences(path);
  CHECK(restored.case_directory == value.case_directory);
  CHECK(restored.layout == value.layout);
  restored.case_directory.clear();
  WritePreferences(path, restored);
  CHECK(ReadPreferences(path).layout == value.layout);
}
TEST_CASE("A failed preferences replacement retains the previous settings",
          "[preferences]") {
  testing::TestDirectory directory;
  const auto path = directory.Path() / "preferences.json";
  const auto cases = directory.Path() / "external cases";
  WritePreferences(path, {cases});
  SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_READONLY);
  CHECK_THROWS(WritePreferences(path, {}));
  SetFileAttributesW(path.c_str(), FILE_ATTRIBUTE_NORMAL);
  CHECK(ReadPreferences(path).case_directory == cases);
  CHECK(std::distance(std::filesystem::directory_iterator(directory.Path()),
                      std::filesystem::directory_iterator{}) == 1);
}
TEST_CASE("Editing a missing script fails without launching a process",
          "[platform][editor]") {
  testing::TestDirectory directory;
  CHECK_THROWS(EditScriptFile(directory.Path() / "missing.lua"));
}
}  // namespace drawer
