#include "app/case_library.h"

#include <set>
#include <stdexcept>

#include "runtime/session.h"

namespace drawer {
CaseLibrary ScanCaseLibrary(const std::filesystem::path& sdk,
                            const std::filesystem::path& directory) {
  if (!std::filesystem::is_directory(directory))
    throw std::runtime_error("Case directory does not exist: " +
                             directory.u8string());
  CaseLibrary result;
  std::set<std::string> ids;
  for (const auto& entry : DiscoverCases(directory)) {
    try {
      Session session(sdk, entry);
      if (!ids.insert(session.Id()).second)
        throw std::runtime_error("Duplicate case ID: " + session.Id());
      result.cases.push_back({entry, session.Id(), session.Name()});
    } catch (const std::exception& error) {
      result.diagnostics += entry.u8string() + ": " + error.what() + "\n";
    }
  }
  return result;
}
}  // namespace drawer
