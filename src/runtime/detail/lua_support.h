#ifndef DRAWER_SRC_RUNTIME_DETAIL_LUA_SUPPORT_H_
#define DRAWER_SRC_RUNTIME_DETAIL_LUA_SUPPORT_H_

#include <exception>
#include <filesystem>
#include <string>

#include "core/value.h"
#include "lua.hpp"

// Private stack conversion and callback adapter; never included by clients.
namespace drawer::lua_detail {
int LoadFile(lua_State* state, const std::filesystem::path& path);
std::string Text(lua_State* state, int index);
Value ReadLua(lua_State* state, int index);
void MarkArray(lua_State* state);
void PushLua(lua_State* state, const Value& value);

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
}  // namespace drawer::lua_detail
#endif  // DRAWER_SRC_RUNTIME_DETAIL_LUA_SUPPORT_H_
