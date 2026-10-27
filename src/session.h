#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "document.h"

struct lua_State;
struct lua_Debug;

namespace drawer {
class Session {
public:
    using UiCallback = std::function<std::vector<Value>(const std::string&, const Value::Array&)>;
    Session(const std::filesystem::path& sdk, const std::filesystem::path& entry);
    ~Session();
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    bool Event(const Value& event);
    bool Controls(UiCallback ui);
    bool Draw(double zoom);
    bool Execute(const std::string& source);
    bool Clear();
    void Undo();
    void Redo();
    void Cancel();
    void Restore(const Document& document);
    Document SaveData() const;
    void MarkSaved() { history_.MarkSaved(); }
    bool Dirty() const { return history_.Dirty() || editing_; }
    bool Editing() const { return editing_; }
    History& GetHistory() { return history_; }
    const Value& State() const { return history_.Current(); }
    const std::vector<Value>& DrawItems() const { return draws_; }
    const std::string& Id() const { return id_; }
    const std::string& Name() const { return name_; }
    const std::string& Error() const { return error_; }
    const std::filesystem::path& Entry() const { return entry_; }
    const std::filesystem::path& Sdk() const { return sdk_; }
    View view;
    std::filesystem::path file;

private:
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
    bool ResetInteraction(bool reset_mode = true);
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

std::vector<std::filesystem::path> DiscoverCases(const std::filesystem::path& directory);
}  // namespace drawer
