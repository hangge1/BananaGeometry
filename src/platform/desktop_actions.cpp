#include "platform/desktop_actions.h"

#include <windows.h>

#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <wrl/client.h>

#include <memory>
#include <stdexcept>
#include <string>

namespace drawer {
namespace {
using Microsoft::WRL::ComPtr;
void Check(HRESULT result, const char* action) {
  if (FAILED(result))
    throw std::runtime_error(std::string(action) + " (HRESULT " +
                             std::to_string(result) + ")");
}
class ComApartment {
 public:
  ComApartment() {
    Check(CoInitializeEx(nullptr,
                         COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE),
          "Cannot initialize desktop services");
  }
  ~ComApartment() { CoUninitialize(); }
  ComApartment(const ComApartment&) = delete;
  ComApartment& operator=(const ComApartment&) = delete;
};
using ShellString = std::unique_ptr<wchar_t, decltype(&CoTaskMemFree)>;
}  // namespace
std::filesystem::path ChooseCaseDirectory(
    const std::filesystem::path& initial_directory) {
  ComApartment apartment;
  ComPtr<IFileOpenDialog> dialog;
  Check(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER,
                         IID_PPV_ARGS(&dialog)),
        "Cannot create folder dialog");
  FILEOPENDIALOGOPTIONS options{};
  Check(dialog->GetOptions(&options), "Cannot read folder dialog options");
  Check(dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM |
                           FOS_PATHMUSTEXIST | FOS_NOCHANGEDIR),
        "Cannot configure folder dialog");
  Check(dialog->SetTitle(L"选择包含用例子目录的文件夹"),
        "Cannot set folder dialog title");
  Check(dialog->SetOkButtonLabel(L"打开用例目录"),
        "Cannot set folder dialog label");
  ComPtr<IShellItem> initial;
  if (std::filesystem::is_directory(initial_directory) &&
      SUCCEEDED(SHCreateItemFromParsingName(initial_directory.c_str(), nullptr,
                                            IID_PPV_ARGS(&initial))))
    Check(dialog->SetFolder(initial.Get()), "Cannot set initial directory");
  const auto result = dialog->Show(GetActiveWindow());
  if (result == HRESULT_FROM_WIN32(ERROR_CANCELLED)) return {};
  Check(result, "Cannot show folder dialog");
  ComPtr<IShellItem> selected;
  Check(dialog->GetResult(&selected), "Cannot read selected folder");
  PWSTR text = nullptr;
  Check(selected->GetDisplayName(SIGDN_FILESYSPATH, &text),
        "Cannot read selected folder path");
  ShellString owned(text, CoTaskMemFree);
  return std::filesystem::path(owned.get());
}
void EditScriptFile(const std::filesystem::path& path) {
  if (!std::filesystem::is_regular_file(path))
    throw std::runtime_error("Script does not exist: " + path.u8string());
  ComApartment apartment;
  const auto directory = path.parent_path();
  SHELLEXECUTEINFOW request{};
  request.cbSize = sizeof(request);
  request.fMask = SEE_MASK_FLAG_NO_UI | SEE_MASK_NOASYNC;
  request.hwnd = GetActiveWindow();
  request.lpVerb = L"edit";
  request.lpFile = path.c_str();
  request.lpDirectory = directory.c_str();
  request.nShow = SW_SHOWNORMAL;
  if (ShellExecuteExW(&request)) return;
  const auto error = GetLastError();
  if (error != ERROR_NO_ASSOCIATION)
    throw std::runtime_error("Cannot open script editor (Windows error " +
                             std::to_string(error) + ")");
  wchar_t system_directory[32768]{};
  const auto length = GetSystemDirectoryW(system_directory, 32768);
  if (!length || length >= 32768)
    throw std::runtime_error("Cannot locate Notepad");
  const auto notepad = std::filesystem::path(system_directory) / L"notepad.exe";
  const auto arguments = L"\"" + path.wstring() + L"\"";
  request.lpVerb = L"open";
  request.lpFile = notepad.c_str();
  request.lpParameters = arguments.c_str();
  if (!ShellExecuteExW(&request))
    throw std::runtime_error("Cannot open Notepad (Windows error " +
                             std::to_string(GetLastError()) + ")");
}
std::filesystem::path LocalPreferencesPath() {
  PWSTR text = nullptr;
  Check(SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &text),
        "Cannot locate local preferences");
  ShellString owned(text, CoTaskMemFree);
  return std::filesystem::path(owned.get()) / L"Drawer/preferences.json";
}
}  // namespace drawer
