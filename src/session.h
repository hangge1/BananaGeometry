#ifndef DRAWER_SRC_SESSION_H_
#define DRAWER_SRC_SESSION_H_

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "core/document.h"
#include "core/history.h"

struct lua_State;
struct lua_Debug;

namespace drawer {
// Owns one Lua VM, its committed data and edit history. Separate instances
// never share mutable Lua globals. Lua callbacks run on the owning UI/test
// thread.
class Session {
 public:
  using UiCallback = std::function<std::vector<Value>(const std::string&,
                                                      const Value::Array&)>;
  Session(const std::filesystem::path& sdk, const std::filesystem::path& entry);
  ~Session();
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  bool Event(const Value& event);
  // The callback is borrowed for this call only and returns
  // value/changed/ended.
  bool Controls(UiCallback ui);
  bool Draw(double zoom);
  // A chunk name beginning with '@' gives Lua failures their source file path.
  bool Execute(const std::string& source,
               const std::string& chunk_name = "console");
  bool Clear();
  void Undo();
  void Redo();
  void Cancel();
  // Use a fresh candidate session: an on_load callback can fail after data has
  // been restored. The host keeps the old session until the candidate succeeds.
  void Restore(const Document& document);
  // Returns committed data. Throws while a preview transaction is active.
  Document SaveData() const;
  void MarkSaved() { history_.MarkSaved(); }
  bool Dirty() const { return history_.Dirty() || editing_; }
  bool Editing() const { return editing_; }
  const History& GetHistory() const { return history_; }
  // A committed snapshot, not the current drag preview; edits invalidate it.
  const Value& State() const { return history_.Current(); }
  // Render data remains valid until the next Draw or session destruction.
  const std::vector<Value>& DrawItems() const { return draws_; }
  const std::string& Id() const { return id_; }
  const std::string& Name() const { return name_; }
  const std::string& Error() const { return error_; }
  const std::filesystem::path& Entry() const { return entry_; }
  const std::filesystem::path& Sdk() const { return sdk_; }
  View view;
  std::filesystem::path file;

 private:
  enum class ModeReset { kPreserve, kDefault };
  static Session* Self(lua_State* state);
  static int Begin(lua_State* state);
  static int Commit(lua_State* state);
  static int CancelEdit(lua_State* state);
  static int Emit(lua_State* state);
  static int Ui(lua_State* state);
  static int Array(lua_State* state);
  static int Module(lua_State* state);
  static int Trace(lua_State* state);
  static void InstructionHook(lua_State* state, lua_Debug* debug);
  bool Call(const char* name, const Value* event = nullptr);
  bool ProtectedCall(int arguments, int results);
  void SetState(const Value& state);
  Value ReadState() const;
  bool ResetInteraction(ModeReset mode_reset = ModeReset::kDefault);
  std::filesystem::path sdk_, entry_;
  lua_State* lua_ = nullptr;
  int context_ = 0;
  int definition_ = 0;
  int case_version_ = 1;
  std::string id_, name_, error_, transaction_label_;
  bool editing_ = false;
  bool initialized_ = false;
  int instruction_count_ = 0;
  History history_;
  std::vector<Value> draws_;
  UiCallback ui_;
};

std::vector<std::filesystem::path> DiscoverCases(
    const std::filesystem::path& directory);
}  // namespace drawer

#endif  // DRAWER_SRC_SESSION_H_
