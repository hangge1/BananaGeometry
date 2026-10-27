#ifndef DRAWER_SRC_APP_APPLICATION_H_
#define DRAWER_SRC_APP_APPLICATION_H_

#include <memory>
#include <string>
#include <vector>

#include "app/canvas_view.h"
#include "runtime/session.h"

namespace drawer {
// Coordinates desktop UI and independent case tabs. Destructive actions are
// deferred until the save/discard/cancel dialog has resolved.
class Application {
 public:
  struct CaseInfo {
    std::filesystem::path entry;
    std::string id;
    std::string name;
  };
  explicit Application(std::filesystem::path root);
  void Frame();
  void RequestExit();
  bool ShouldExit() const { return exit_; }
  // Rescans metadata without replacing open sessions or their document data.
  void RefreshCases();
  const std::vector<CaseInfo>& Cases() const { return cases_; }
  const std::string& Error() const { return error_; }
  // Borrowed until the tab closes or successfully reloads.
  Session* Active();
  bool SmokeCheck(const std::string& case_id = "drawer.gallery",
                  const std::filesystem::path& fixture = {});

 private:
  struct Tab {
    int key;
    std::unique_ptr<Session> session;
  };
  enum class Action { kNone, kClose, kReload, kExit };
  void New(const std::filesystem::path& entry);
  void Open();
  bool Save(Session* session, bool save_as = false);
  void Request(Action action, int key);
  void Complete();
  void Confirm();
  void Controls(Session* session);
  std::filesystem::path root_;
  std::vector<CaseInfo> cases_;
  std::vector<Tab> tabs_;
  int next_key_ = 1;
  int active_ = 0;
  int target_ = 0;
  Action pending_ = Action::kNone;
  bool exit_ = false;
  int select_key_ = 0;
  std::string error_;
  std::string notice_;
  CanvasView canvas_;
};
}  // namespace drawer

#endif  // DRAWER_SRC_APP_APPLICATION_H_
