#ifndef DRAWER_SRC_APP_APPLICATION_H_
#define DRAWER_SRC_APP_APPLICATION_H_
#include <array>
#include <functional>
#include <memory>
#include <vector>
#include "app/canvas_view.h"
#include "app/case_library.h"
#include "app/desktop_services.h"
#include "app/working_document.h"
namespace drawer {
class Application {
 public:
  explicit Application(std::filesystem::path runtime_root,
                       std::filesystem::path case_directory = {},
                       DesktopServices desktop = {});
  void Frame();
  void RequestExit();
  bool ShouldExit() const { return exit_; }
  void RefreshCases();
  bool SetCaseDirectory(const std::filesystem::path& directory);
  bool OpenCaseDirectory();
  bool EditCurrentScript();
  void NewDocument();
  void NewCase(const std::filesystem::path& entry);
  bool OpenDocument(const std::filesystem::path& path);
  bool SaveDocument(const std::filesystem::path& path);
  bool RetryInstance();
  void ReportError(const std::string& message);
  const std::filesystem::path& CaseDirectory() const { return case_directory_; }
  const std::vector<CaseInfo>& Cases() const { return cases_; }
  const std::string& Error() const { return error_; }
  // No interactive session while the document node is selected.
  Session* Active();
  // Last viewed instance, retained for document preview without case input.
  Session* Preview();
  bool SelectDocument();
  bool SelectInstance(int instance);
  WorkingDocument* ActiveDocument();
  bool SmokeCheck(const std::string& case_id = "drawer.gallery",
                  const std::filesystem::path& fixture = {});

 private:
  struct Tab {
    int key;
    std::unique_ptr<WorkingDocument> document;
  };
  enum class Action { kNone, kClose, kReload, kExit, kRemoveInstance };
  enum class Selection { kDocument, kInstance, kObject };
  void Open();
  bool Save(bool save_as = false);
  void Request(Action action, int document = 0, int instance = 0);
  void Complete();
  void Confirm();
  void Menus();
  void Tree();
  void Inspector();
  void Shortcuts();
  void FitSelection();
  bool Activate(int document, int instance, double object = 0);
  bool NameField(const char* label, const std::string& value,
                 std::string* result);
  Value Layout() const;
  void RememberLayout();
  WorkingDocument* FindDocument(int key);
  InstanceEntry* ActiveEntry();
  InstanceEntry* PreviewEntry();
  bool FinishActiveEdit();
  std::shared_ptr<RuntimeInstance> RestoreInstance(const Document& data);
  void Queue(std::function<void()> action) { queued_ = std::move(action); }
  std::filesystem::path runtime_root_, case_directory_;
  DesktopServices desktop_;
  std::vector<CaseInfo> cases_;
  std::vector<Tab> tabs_;
  int next_key_ = 1, active_ = 0, target_ = 0, target_instance_ = 0;
  Action pending_ = Action::kNone;
  Selection selection_ = Selection::kInstance;
  bool exit_ = false, show_tree_ = true, show_properties_ = true,
       show_diagnostics_ = false, show_help_ = false;
  float tree_width_ = 245, properties_width_ = 325;
  Point canvas_size_{700, 500};
  int select_key_ = 0;
  std::string error_, notice_;
  std::function<void()> queued_;
  std::function<void()> rename_edit_;
  unsigned rename_key_ = 0;
  std::array<char, 1025> rename_buffer_{};
  Value saved_layout_;
  CanvasView canvas_;
};
}  // namespace drawer
#endif  // DRAWER_SRC_APP_APPLICATION_H_
