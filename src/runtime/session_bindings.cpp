#include "runtime/session.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

#include "runtime/detail/lua_support.h"

namespace drawer {
using lua_detail::LoadFile;
using lua_detail::MarkArray;
using lua_detail::PushLua;
using lua_detail::ReadLua;
using lua_detail::Text;

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
int Session::MeasureText(lua_State* state) {
  const auto text = Text(state, 1);
  const auto* self = Self(state);
  const auto size = self->text_measurer_ ? self->text_measurer_(text) : Point{};
  if (!std::isfinite(size.x) || !std::isfinite(size.y) || size.x < 0 ||
      size.y < 0)
    throw std::runtime_error("Text measurement must be finite and nonnegative");
  lua_pushnumber(state, size.x);
  lua_pushnumber(state, size.y);
  return 2;
}
int Session::Module(lua_State* state) {
  auto module = Text(state, 1);
  if (module.empty() || module.front() == '.' || module.back() == '.' ||
      module.find("..") != std::string::npos ||
      module.find_first_not_of(
          "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.") !=
          std::string::npos)
    throw std::runtime_error("Invalid Lua module name");
  const bool sdk_module = module.rfind("drawer.", 0) == 0;
  std::replace(module.begin(), module.end(), '.', '/');
  const auto& root = sdk_module ? Self(state)->sdk_ : Self(state)->entry_;
  auto base = root.parent_path() / std::filesystem::u8path(module);
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
}  // namespace drawer
