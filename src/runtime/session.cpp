#include "runtime/session.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "runtime/detail/lua_support.h"

namespace drawer {
using lua_detail::LoadFile;
using lua_detail::PushLua;
using lua_detail::ReadLua;
using lua_detail::Safe;
using lua_detail::Text;

bool Session::ProtectedCall(int arguments, int results) {
  int function = lua_gettop(lua_) - arguments;
  lua_pushcfunction(lua_, Trace);
  lua_insert(lua_, function);
  instruction_count_ = 0;
  lua_sethook(lua_, InstructionHook, LUA_MASKCOUNT, 10000);
  int status = lua_pcall(lua_, arguments, results, function);
  lua_sethook(lua_, nullptr, 0, 0);
  lua_remove(lua_, function);
  if (status == LUA_OK) return true;
  const char* message = lua_tostring(lua_, -1);
  error_ = message ? message : "Unknown Lua error";
  lua_pop(lua_, 1);
  return false;
}

Session::Session(const std::filesystem::path& sdk,
                 const std::filesystem::path& entry)
    : sdk_(sdk), entry_(entry) {
  lua_ = luaL_newstate();
  if (!lua_) throw std::runtime_error("Cannot create Lua environment");
  *static_cast<Session**>(lua_getextraspace(lua_)) = this;
  try {
    luaL_openlibs(lua_);
    // Cases are local code, but these process-wide operations are not case
    // APIs.
    lua_getglobal(lua_, "os");
    lua_pushnil(lua_);
    lua_setfield(lua_, -2, "exit");
    lua_pushnil(lua_);
    lua_setfield(lua_, -2, "execute");
    lua_pop(lua_, 1);
    lua_getglobal(lua_, "package");
    auto search_path = entry.parent_path().u8string() + "/?.lua;" +
                       entry.parent_path().u8string() + "/?/init.lua";
    lua_pushlstring(lua_, search_path.data(), search_path.size());
    lua_setfield(lua_, -2, "path");
    lua_pushliteral(lua_, "");
    lua_setfield(lua_, -2, "cpath");
    lua_pushnil(lua_);
    lua_setfield(lua_, -2, "loadlib");
    lua_getfield(lua_, -1, "searchers");
    lua_pushlightuserdata(lua_, this);
    lua_pushcclosure(lua_, Safe<Module>, 1);
    lua_rawseti(lua_, -2, 2);
    lua_pushnil(lua_);
    lua_rawseti(lua_, -2, 3);
    lua_pushnil(lua_);
    lua_rawseti(lua_, -2, 4);
    lua_pop(lua_, 1);
    lua_pop(lua_, 1);
    lua_newtable(lua_);
    const std::pair<const char*, lua_CFunction> functions[] = {
        {"begin_edit", Safe<Begin>},
        {"commit_edit", Safe<Commit>},
        {"cancel_edit", Safe<CancelEdit>},
        {"emit", Safe<Emit>},
        {"ui", Safe<Ui>},
        {"array", Safe<Array>},
        {"measure_text", Safe<MeasureText>},
        {"bezier_polyline", Safe<BezierPolyline>}};
    for (const auto& [name, function] : functions) {
      lua_pushlightuserdata(lua_, this);
      lua_pushcclosure(lua_, function, 1);
      lua_setfield(lua_, -2, name);
    }
    lua_setglobal(lua_, "_host");
    if (LoadFile(lua_, sdk) != LUA_OK || !ProtectedCall(0, 1)) {
      if (error_.empty()) error_ = lua_tostring(lua_, -1);
      throw std::runtime_error(error_);
    }
    lua_getfield(lua_, -1, "new");
    if (!ProtectedCall(0, 1)) throw std::runtime_error(error_);
    context_ = luaL_ref(lua_, LUA_REGISTRYINDEX);
    lua_pop(lua_, 1);
    if (LoadFile(lua_, entry) != LUA_OK || !ProtectedCall(0, 1)) {
      if (error_.empty()) error_ = lua_tostring(lua_, -1);
      throw std::runtime_error(error_);
    }
    if (!lua_istable(lua_, -1))
      throw std::runtime_error("Case must return a definition table");
    lua_getfield(lua_, -1, "id");
    id_ = Text(lua_, -1);
    lua_pop(lua_, 1);
    lua_getfield(lua_, -1, "name");
    name_ = Text(lua_, -1);
    lua_pop(lua_, 1);
    lua_getfield(lua_, -1, "data_version");
    auto version = lua_tonumber(lua_, -1);
    if (!lua_isnumber(lua_, -1) || version < 1 || version > 1000000 ||
        version != std::floor(version))
      throw std::runtime_error("Case data_version must be a positive integer");
    case_version_ = static_cast<int>(version);
    lua_pop(lua_, 1);
    if (id_.empty() || id_.size() > 200 || name_.empty())
      throw std::runtime_error("Case ID and name must be nonempty");
    definition_ = luaL_ref(lua_, LUA_REGISTRYINDEX);
    if (!ResetInteraction() || !Call("on_init"))
      throw std::runtime_error(error_);
    auto initial = ReadState();
    ValidateState(initial);
    history_.Reset(std::move(initial));
    initialized_ = true;
  } catch (...) {
    lua_close(lua_);
    lua_ = nullptr;
    throw;
  }
}
Session::~Session() {
  if (lua_) lua_close(lua_);
}

Value Session::ReadState() const {
  int top = lua_gettop(lua_);
  try {
    lua_rawgeti(lua_, LUA_REGISTRYINDEX, context_);
    lua_getfield(lua_, -1, "state");
    auto value = ReadLua(lua_, -1);
    lua_settop(lua_, top);
    return value;
  } catch (...) {
    lua_settop(lua_, top);
    throw;
  }
}
void Session::SetState(const Value& state) {
  lua_rawgeti(lua_, LUA_REGISTRYINDEX, context_);
  PushLua(lua_, state);
  lua_setfield(lua_, -2, "state");
  lua_pop(lua_, 1);
}
bool Session::Call(const char* name, const Value* event) {
  const int top = lua_gettop(lua_);
  lua_rawgeti(lua_, LUA_REGISTRYINDEX, definition_);
  lua_getfield(lua_, -1, name);
  if (lua_isnil(lua_, -1)) {
    lua_settop(lua_, top);
    return true;
  }
  lua_rawgeti(lua_, LUA_REGISTRYINDEX, context_);
  if (event) PushLua(lua_, *event);
  bool ok = ProtectedCall(event ? 2 : 1, 0);
  lua_settop(lua_, top);
  if (!ok && initialized_) {
    editing_ = false;
    SetState(history_.Current());
    // Drop transient handles referring to the replaced state without calling
    // case code.
    lua_rawgeti(lua_, LUA_REGISTRYINDEX, context_);
    lua_newtable(lua_);
    lua_setfield(lua_, -2, "temp");
    lua_pop(lua_, 1);
  }
  return ok;
}
bool Session::ResetInteraction(ModeReset mode_reset) {
  lua_rawgeti(lua_, LUA_REGISTRYINDEX, context_);
  lua_newtable(lua_);
  lua_setfield(lua_, -2, "temp");
  if (mode_reset == ModeReset::kDefault) {
    lua_rawgeti(lua_, LUA_REGISTRYINDEX, definition_);
    lua_getfield(lua_, -1, "default_mode");
    if (!lua_isstring(lua_, -1)) {
      lua_pop(lua_, 1);
      lua_pushliteral(lua_, "draw");
    }
    lua_setfield(lua_, -3, "mode");
    lua_pop(lua_, 1);
  }
  lua_pop(lua_, 1);
  return true;
}
bool Session::Event(const Value& event) {
  lua_rawgeti(lua_, LUA_REGISTRYINDEX, context_);
  lua_pushnumber(lua_, view.zoom);
  lua_setfield(lua_, -2, "zoom");
  lua_pop(lua_, 1);
  return Call("on_event", &event);
}
bool Session::Controls(UiCallback ui) {
  ui_ = std::move(ui);
  bool result = Call("on_ui");
  ui_ = {};
  return result;
}
bool Session::Draw(double zoom) {
  draws_.clear();
  lua_rawgeti(lua_, LUA_REGISTRYINDEX, context_);
  lua_pushnumber(lua_, zoom);
  lua_setfield(lua_, -2, "zoom");
  lua_pop(lua_, 1);
  return Call("on_draw");
}
bool Session::Execute(const std::string& source,
                      const std::string& chunk_name) {
  const int top = lua_gettop(lua_);
  lua_rawgeti(lua_, LUA_REGISTRYINDEX, context_);
  lua_setglobal(lua_, "ctx");
  bool ok = luaL_loadbuffer(lua_, source.data(), source.size(),
                            chunk_name.c_str()) == LUA_OK;
  if (ok)
    ok = ProtectedCall(0, 0);
  else
    error_ = lua_tostring(lua_, -1);
  lua_settop(lua_, top);
  if (!ok) {
    editing_ = false;
    SetState(history_.Current());
  }
  return ok;
}
void Session::Cancel() {
  if (editing_) {
    editing_ = false;
    SetState(history_.Current());
  }
  ResetInteraction(ModeReset::kPreserve);
}
void Session::Undo() {
  Cancel();
  if (history_.Undo()) {
    SetState(history_.Current());
    ResetInteraction(ModeReset::kPreserve);
  }
}
void Session::Redo() {
  Cancel();
  if (history_.Redo()) {
    SetState(history_.Current());
    ResetInteraction(ModeReset::kPreserve);
  }
}
bool Session::Clear() {
  Cancel();
  lua_rawgeti(lua_, LUA_REGISTRYINDEX, definition_);
  lua_getfield(lua_, -1, "on_clear");
  lua_setglobal(lua_, "_case_clear");
  lua_pop(lua_, 1);
  return Execute("ctx:clear(_case_clear)");
}
Document Session::SaveData() const {
  if (editing_)
    throw std::runtime_error("Finish or cancel the current edit before saving");
  return {id_, case_version_, history_.Current(), view};
}
void Session::Restore(const Document& document) {
  if (document.case_id != id_ || document.case_version != case_version_)
    throw std::runtime_error("Missing case or incompatible case data version");
  ValidateState(document.state);
  SetState(document.state);
  history_.Reset(document.state);
  view = document.view;
  ResetInteraction();
  if (!Call("on_load")) throw std::runtime_error(error_);
  // Compatible default additions become the loaded clean baseline, rather
  // than existing only in Lua and disappearing on the next undo or save.
  if (editing_)
    throw std::runtime_error("on_load left an edit transaction open");
  auto restored = ReadState();
  ValidateState(restored);
  history_.Reset(std::move(restored));
}
std::vector<std::filesystem::path> DiscoverCases(
    const std::filesystem::path& directory) {
  std::vector<std::filesystem::path> result;
  if (!std::filesystem::exists(directory)) return result;
  for (const auto& entry : std::filesystem::directory_iterator(directory))
    if (entry.is_directory() &&
        std::filesystem::is_regular_file(entry.path() / "main.lua"))
      result.push_back(entry.path() / "main.lua");
  std::sort(result.begin(), result.end());
  return result;
}
}  // namespace drawer
