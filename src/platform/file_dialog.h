#ifndef DRAWER_SRC_PLATFORM_FILE_DIALOG_H_
#define DRAWER_SRC_PLATFORM_FILE_DIALOG_H_

#include <filesystem>

namespace drawer {
enum class DocumentDialog { kOpen, kSave };
// Returns an empty path when canceled; throws on native dialog failures.
std::filesystem::path ChooseDocumentPath(DocumentDialog operation);
}  // namespace drawer

#endif  // DRAWER_SRC_PLATFORM_FILE_DIALOG_H_
