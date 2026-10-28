#ifndef DRAWER_SRC_PLATFORM_DOCUMENT_IO_H_
#define DRAWER_SRC_PLATFORM_DOCUMENT_IO_H_
#include <filesystem>
#include "core/document.h"
namespace drawer {
// Reads both compact and indented UTF-8 JSON documents.
Document ReadDocument(const std::filesystem::path& path);
// Replace only after a complete temporary file is written on the same volume.
// Failures preserve the old destination and remove the temporary file.
// Saves UTF-8 JSON with two-space indentation and a final LF.
void WriteDocument(const std::filesystem::path& path, const Document& document);
}  // namespace drawer
#endif  // DRAWER_SRC_PLATFORM_DOCUMENT_IO_H_
