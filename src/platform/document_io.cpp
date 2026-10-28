#include "platform/document_io.h"
#include <windows.h>

#include <atomic>
#include <fstream>
#include <stdexcept>
namespace drawer {
Document ReadDocument(const std::filesystem::path& path) {
  if (std::filesystem::file_size(path) > 16 * 1024 * 1024)
    throw std::runtime_error("File exceeds 16 MiB");
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("Cannot open document");
  std::string text((std::istreambuf_iterator<char>(input)), {});
  return DecodeDocument(ParseJson(text));
}
namespace {
void WriteJsonFile(const std::filesystem::path& path, const Value& value) {
  const std::string text = ToJson(value, JsonFormat::kIndented) + '\n';
  if (text.size() > 16 * 1024 * 1024)
    throw std::runtime_error("Document exceeds 16 MiB");
  // Write beside the destination so replacement stays on the same filesystem.
  static std::atomic<unsigned> sequence{0};
  auto temporary = path;
  temporary += L".tmp." + std::to_wstring(GetCurrentProcessId()) + L"." +
               std::to_wstring(++sequence);
  try {
    {
      std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
      output.exceptions(std::ios::failbit | std::ios::badbit);
      output.write(text.data(), static_cast<std::streamsize>(text.size()));
      output.flush();
      output.close();
    }
    if (!MoveFileExW(temporary.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH))
      throw std::runtime_error(
          "Cannot replace destination document (Windows error " +
          std::to_string(GetLastError()) + ")");
  } catch (...) {
    std::error_code ignored;
    std::filesystem::remove(temporary, ignored);
    throw;
  }
}
}  // namespace
void WriteDocument(const std::filesystem::path& path,
                   const Document& document) {
  WriteJsonFile(path, EncodeDocument(document));
}
WorkspaceDocument ReadWorkspace(const std::filesystem::path& path) {
  if (std::filesystem::file_size(path) > 16 * 1024 * 1024)
    throw std::runtime_error("File exceeds 16 MiB");
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("Cannot open document");
  return DecodeWorkspace(
      ParseJson(std::string(std::istreambuf_iterator<char>(input), {})));
}
void WriteWorkspace(const std::filesystem::path& path,
                    const WorkspaceDocument& document) {
  WriteJsonFile(path, EncodeWorkspace(document));
}
}  // namespace drawer
