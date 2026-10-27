#ifndef DRAWER_TESTS_SUPPORT_ARTIFACTS_H_
#define DRAWER_TESTS_SUPPORT_ARTIFACTS_H_

#include <filesystem>
#include <string>

namespace drawer::testing {
// Catch2's listener owns fixture cleanup at the end of the current test.
// Failed tests retain fixtures for inspection; successful tests remove them.
std::string CurrentArtifactTestName();
void RegisterTemporaryDirectory(const std::filesystem::path& path);
}  // namespace drawer::testing

#endif  // DRAWER_TESTS_SUPPORT_ARTIFACTS_H_
