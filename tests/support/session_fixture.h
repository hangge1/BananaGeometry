#ifndef DRAWER_TESTS_SUPPORT_SESSION_FIXTURE_H_
#define DRAWER_TESTS_SUPPORT_SESSION_FIXTURE_H_
#include <fstream>
#include <string>
#include <catch2/catch_test_macros.hpp>
#include "session.h"
#include "support/paths.h"

namespace drawer::testing {
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
#endif  // DRAWER_TESTS_SUPPORT_SESSION_FIXTURE_H_
