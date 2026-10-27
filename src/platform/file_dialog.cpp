#include "platform/file_dialog.h"

#include <windows.h>

#include <commdlg.h>

#include <stdexcept>

namespace drawer {
std::filesystem::path ChooseDocumentPath(DocumentDialog operation) {
  const bool save = operation == DocumentDialog::kSave;
  wchar_t path[32768]{};
  OPENFILENAMEW dialog{};
  dialog.lStructSize = sizeof(dialog);
  dialog.hwndOwner = GetActiveWindow();
  dialog.lpstrFilter =
      L"Drawer document (*.drawer.json)\0*.drawer.json\0JSON "
      L"(*.json)\0*.json\0\0";
  dialog.lpstrFile = path;
  dialog.nMaxFile = 32768;
  dialog.lpstrDefExt = L"drawer.json";
  dialog.Flags = OFN_NOCHANGEDIR | OFN_EXPLORER | OFN_PATHMUSTEXIST |
                 (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
  if (save ? GetSaveFileNameW(&dialog) : GetOpenFileNameW(&dialog)) return path;
  if (CommDlgExtendedError())
    throw std::runtime_error("System file dialog failed");
  return {};
}
}  // namespace drawer
