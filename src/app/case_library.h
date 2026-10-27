#ifndef DRAWER_SRC_APP_CASE_LIBRARY_H_
#define DRAWER_SRC_APP_CASE_LIBRARY_H_

#include <filesystem>
#include <string>
#include <vector>

namespace drawer {
struct CaseInfo {
  std::filesystem::path entry;
  std::string id;
  std::string name;
};
struct CaseLibrary {
  std::vector<CaseInfo> cases;
  std::string diagnostics;
};
// Individual invalid cases are diagnosed; inaccessible directories throw.
CaseLibrary ScanCaseLibrary(const std::filesystem::path& sdk,
                            const std::filesystem::path& directory);
}  // namespace drawer

#endif  // DRAWER_SRC_APP_CASE_LIBRARY_H_
