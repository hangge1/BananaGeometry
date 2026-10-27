#ifndef DRAWER_SRC_PLATFORM_DOCUMENT_IO_H_
#define DRAWER_SRC_PLATFORM_DOCUMENT_IO_H_
#include <filesystem>
#include "core/document.h"
namespace drawer {
Document ReadDocument(const std::filesystem::path& path);
// Replace only after a complete temporary file is written on the same volume.
// Failures preserve the old destination and remove the temporary file.
void WriteDocument(const std::filesystem::path& path, const Document& document);
}  // namespace drawer
#endif  // DRAWER_SRC_PLATFORM_DOCUMENT_IO_H_
