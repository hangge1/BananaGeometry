#include "session.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>

#include "lua.hpp"

namespace drawer {
namespace {
int LoadFile(lua_State* state, const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input)
    throw std::runtime_error("Cannot open Lua script: " + path.u8string());
  if (std::filesystem::file_size(path) > 1024 * 1024)
    throw std::runtime_error("Script exceeds 1 MiB");
  std::string source((std::istreambuf_iterator<char>(input)), {});
  auto name = "@" + path.u8string();
  return luaL_loadbufferx(state, source.data(), source.size(), name.c_str(),
                          "t");
}
std::string Text(lua_State* state, int index) {
  if (lua_type(state, index) != LUA_TSTRING)
    throw std::runtime_error("Expected string argument");
  size_t size = 0;
  auto* text = lua_tolstring(state, index, &size);
  return std::string(text, size);
}
Value ReadLua(lua_State* state, int index, std::set<const void*>* visited,
              int depth = 0) {
  if (depth > 64) throw std::runtime_error("State nesting exceeds 64");
  index = lua_absindex(state, index);
  switch (lua_type(state, index)) {
    case LUA_TNIL:
      return {};
    case LUA_TBOOLEAN:
      return lua_toboolean(state, index) != 0;
    case LUA_TNUMBER: {
      double number = lua_tonumber(state, index);
      if (!std::isfinite(number))
        throw std::runtime_error("State contains a nonfinite number");
      return number;
    }
    case LUA_TSTRING:
      return Text(state, index);
    case LUA_TTABLE:
      break;
    default:
      throw std::runtime_error(
          "State supports only numbers, booleans, strings and plain tables");
  }
  auto* identity = lua_topointer(state, index);
  if (!visited->insert(identity).second)
    throw std::runtime_error("Cyclic state is not serializable");
  const size_t length = lua_rawlen(state, index);
  if (length > 1000000) throw std::runtime_error("Array exceeds 1000000 items");
  bool array = length > 0;
  if (lua_getmetatable(state, index)) {
    lua_getfield(state, -1, "__drawer_array");
    array = array || lua_toboolean(state, -1);
    lua_pop(state, 2);
  }
  Value::Array items;
  Value::Object fields;
  if (array) {
    items.reserve(length);
    for (size_t i = 1; i <= length; ++i) {
      lua_rawgeti(state, index, static_cast<lua_Integer>(i));
      if (lua_isnil(state, -1))
        throw std::runtime_error("Sparse arrays are not supported");
      items.push_back(ReadLua(state, -1, visited, depth + 1));
      lua_pop(state, 1);
    }
  }
  lua_pushnil(state);
  while (lua_next(state, index)) {
    if (array) {
      if (!lua_isinteger(state, -2) || lua_tointeger(state, -2) < 1 ||
          static_cast<lua_Unsigned>(lua_tointeger(state, -2)) > length)
        throw std::runtime_error("Cannot mix array and named keys");
    } else {
      if (lua_type(state, -2) != LUA_TSTRING)
        throw std::runtime_error("Object keys must be strings");
      fields.emplace(Text(state, -2), ReadLua(state, -1, visited, depth + 1));
    }
    lua_pop(state, 1);
  }
  visited->erase(identity);
  return array ? Value(std::move(items)) : Value(std::move(fields));
}
Value ReadLua(lua_State* state, int index) {
  std::set<const void*> visited;
  return ReadLua(state, index, &visited);
}
void MarkArray(lua_State* state) {
  lua_newtable(state);
  lua_pushboolean(state, true);
  lua_setfield(state, -2, "__drawer_array");
  lua_setmetatable(state, -2);
}
void PushLua(lua_State* state, const Value& value) {
  if (value.IsNull())
    lua_pushnil(state);
  else if (value.IsBool())
    lua_pushboolean(state, value.Bool());
  else if (value.IsNumber())
    lua_pushnumber(state, value.Number());
  else if (value.IsString())
    lua_pushlstring(state, value.String().data(), value.String().size());
  else if (value.IsArray()) {
    lua_createtable(state, static_cast<int>(value.Items().size()), 0);
    MarkArray(state);
    lua_Integer i = 0;
    for (const auto& item : value.Items()) {
      PushLua(state, item);
      lua_rawseti(state, -2, ++i);
    }
  } else {
    lua_createtable(state, 0, static_cast<int>(value.Fields().size()));
    for (const auto& [key, item] : value.Fields()) {
      lua_pushlstring(state, key.data(), key.size());
      PushLua(state, item);
      lua_rawset(state, -3);
    }
  }
}
template <int (*Function)(lua_State*)>
int Safe(lua_State* state) {
  try {
    return Function(state);
  } catch (const std::exception& error) {
    lua_pushstring(state, error.what());
  }
  // Exception objects and C++ locals have been destroyed before Lua longjmps.
  return lua_error(state);
}
}  // namespace

Session* Session::Self(lua_State* state) {
  return static_cast<Session*>(lua_touserdata(state, lua_upvalueindex(1)));
}
int Session::Begin(lua_State* state) {
  auto* self = Self(state);
  if (self->editing_)
    throw std::runtime_error("Nested edit transactions are not supported");
  self->transaction_label_ = Text(state, 1);
  self->editing_ = true;
  return 0;
}
int Session::Commit(lua_State* state) {
  auto* self = Self(state);
  if (!self->editing_) throw std::runtime_error("No active edit transaction");
  auto value = self->ReadState();
  ValidateState(value);
  if (self->initialized_)
    self->history_.Push(std::move(value), self->transaction_label_);
  self->editing_ = false;
  return 0;
}
int Session::CancelEdit(lua_State* state) {
  Self(state)->Cancel();
  return 0;
}
int Session::Emit(lua_State* state) {
  auto value = ReadLua(state, 1);
  auto* self = Self(state);
  if (value.IsArray()) {
    self->draws_.insert(self->draws_.end(), value.Items().begin(),
                        value.Items().end());
  } else if (value.IsObject())
    self->draws_.push_back(std::move(value));
  else
    throw std::runtime_error("Draw expects an object or an array");
  if (self->draws_.size() > 100000)
    throw std::runtime_error("Draw item limit exceeded");
  return 0;
}
int Session::Ui(lua_State* state) {
  auto operation = Text(state, 1);
  Value::Array arguments;
  for (int i = 2; i <= lua_gettop(state); ++i)
    arguments.push_back(ReadLua(state, i));
  auto* self = Self(state);
  std::vector<Value> results;
  if (self->ui_)
    results = self->ui_(operation, arguments);
  else
    results = {operation == "button"  ? Value(false)
               : arguments.size() > 1 ? arguments[1]
                                      : Value(),
               false, false};
  for (const auto& result : results) PushLua(state, result);
  return static_cast<int>(results.size());
}
int Session::Array(lua_State* state) {
  lua_newtable(state);
  MarkArray(state);
  return 1;
}
int Session::Module(lua_State* state) {
  auto module = Text(state, 1);
  if (module.empty() || module.front() == '.' || module.back() == '.' ||
      module.find("..") != std::string::npos ||
      module.find_first_not_of(
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.") !=
          std::string::npos)
    throw std::runtime_error("Invalid Lua module name");
  std::replace(module.begin(), module.end(), '.', '/');
  auto base =
      Self(state)->entry_.parent_path() / std::filesystem::u8path(module);
  auto file = base;
  file += ".lua";
  if (!std::filesystem::is_regular_file(file)) file = base / "init.lua";
  if (!std::filesystem::is_regular_file(file)) {
    auto message = "\n\tno local module '" + module + "'";
    lua_pushlstring(state, message.data(), message.size());
    return 1;
  }
  if (LoadFile(state, file) != LUA_OK) {
    auto error = Text(state, -1);
    lua_pop(state, 1);
    throw std::runtime_error(error);
  }
  auto name = file.u8string();
  lua_pushlstring(state, name.data(), name.size());
  return 2;
}
int Session::Trace(lua_State* state) {
  const char* message = lua_tostring(state, 1);
  luaL_traceback(state, state, message ? message : "Lua error", 1);
  return 1;
}
void Session::InstructionHook(lua_State* state, lua_Debug*) {
  auto* self = *static_cast<Session**>(lua_getextraspace(state));
  if (++self->instruction_count_ > 1000)
    luaL_error(state, "Callback instruction budget exceeded");
}
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
        {"array", Safe<Array>}};
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
