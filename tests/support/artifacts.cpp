#include "support/artifacts.h"

#include <iostream>
#include <stdexcept>
#include <vector>

#include <catch2/reporters/catch_reporter_event_listener.hpp>
#include <catch2/reporters/catch_reporter_registrars.hpp>
#include <catch2/catch_test_case_info.hpp>

#include "test_config.h"

namespace drawer::testing {
namespace {
// Test-only state follows Catch2's serial test lifecycle within each process.
std::string current_name;
std::vector<std::filesystem::path> directories;
class ArtifactListener : public Catch::EventListenerBase {
 public:
  using EventListenerBase::EventListenerBase;
  void testCaseStarting(const Catch::TestCaseInfo& info) override {
    current_name = info.name;
    directories.clear();
  }
  void testCaseEnded(const Catch::TestCaseStats& stats) override {
    for (const auto& directory : directories) {
      if (stats.totals.assertions.failed > 0) {
        std::cerr << "Retained fixture: " << directory.u8string() << '\n';
      } else {
        std::error_code error;
        std::filesystem::remove_all(directory, error);
        if (error)
          std::cerr << "Fixture cleanup failed: " << directory.u8string()
                    << '\n';
      }
    }
    directories.clear();
  }
};
CATCH_REGISTER_LISTENER(ArtifactListener)
}  // namespace
std::string CurrentArtifactTestName() { return current_name; }
void RegisterTemporaryDirectory(const std::filesystem::path& path) {
  const auto root = std::filesystem::weakly_canonical(
      std::filesystem::u8path(DRAWER_TEST_OUTPUT_DIR));
  const auto target = std::filesystem::weakly_canonical(path);
  const auto relative = target.lexically_relative(root);
  if (relative.empty() || relative == "." || relative.is_absolute())
    throw std::runtime_error("Invalid fixture directory");
  for (const auto& part : relative)
    if (part == "..")
      throw std::runtime_error("Fixture escaped the temporary root");
  directories.push_back(target);
}
}  // namespace drawer::testing
