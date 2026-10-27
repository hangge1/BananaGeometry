#ifndef DRAWER_SRC_APPLICATION_H_
#define DRAWER_SRC_APPLICATION_H_

#include <memory>
#include <string>
#include <vector>

#include "runtime/session.h"

namespace drawer {
// Coordinates desktop UI and independent case tabs. Destructive actions are
// deferred until the save/discard/cancel dialog has resolved.
class Application {
 public:
  explicit Application(std::filesystem::path root);
  void Frame();
  void RequestExit();
  bool ShouldExit() const { return exit_; }
  bool SmokeCheck();

 private:
  struct CaseInfo {
    std::filesystem::path entry;
    std::string id, name;
  };
  struct Tab {
    int key;
    std::unique_ptr<Session> session;
  };
  enum class Action { kNone, kClose, kReload, kExit };
  void Scan();
  void New(const std::filesystem::path& entry);
  void Open();
  bool Save(Session* session, bool save_as = false);
  void Request(Action action, int key);
  void Complete();
  void Confirm();
  Session* Active();
  void Canvas(Session* session);
  void Controls(Session* session);
  std::vector<Value> Ui(const std::string& operation,
                        const Value::Array& arguments);
  std::filesystem::path root_;
  std::vector<CaseInfo> cases_;
  std::vector<Tab> tabs_;
  int next_key_ = 1, active_ = 0, target_ = 0;
  Action pending_ = Action::kNone;
  bool exit_ = false;
  int select_key_ = 0;
  std::string error_, notice_;
  double last_x_ = 0, last_y_ = 0;
};
}  // namespace drawer

#endif  // DRAWER_SRC_APPLICATION_H_
