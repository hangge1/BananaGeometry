#include <iostream>

#include <catch2/catch_test_macros.hpp>

#include "support/paths.h"

TEST_CASE("Temporary fixture lifecycle probe", "[validation-tools]") {
  drawer::testing::TestDirectory directory;
  drawer::testing::WriteScript(directory.Path() / "evidence.txt",
                               "fixture evidence");
  std::cout << "DRAWER_FIXTURE_PATH=" << directory.Path().u8string() << '\n';
  char flag[2]{};
  const bool expected_failure =
      GetEnvironmentVariableA("DRAWER_ARTIFACT_PROBE_FAIL", flag,
                              sizeof(flag)) > 0;
  CHECK_FALSE(expected_failure);
}
