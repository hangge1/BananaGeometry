#include "runtime/session.h"

#include <chrono>

#include "runtime/detail/lua_support.h"

namespace drawer {
int Session::Clock(lua_State* state) {
  const auto elapsed = std::chrono::steady_clock::now().time_since_epoch();
  lua_pushnumber(state, std::chrono::duration<double>(elapsed).count());
  return 1;
}
Session::ModeFeedback Session::GetModeFeedback() {
  const int top = lua_gettop(lua_);
  ModeFeedback result;
  try {
    lua_rawgeti(lua_, LUA_REGISTRYINDEX, context_);
    lua_pushliteral(lua_, "mode");
    lua_rawget(lua_, -2);
    if (lua_type(lua_, -1) == LUA_TSTRING) result.id = lua_tostring(lua_, -1);
    lua_pop(lua_, 1);
    result.title = result.id.empty() ? "未声明模式" : result.id;
    lua_pushliteral(lua_, "mode_feedback");
    lua_rawget(lua_, -2);
    if (lua_isfunction(lua_, -1)) {
      lua_pushvalue(lua_, -2);
      if (ProtectedCall(1, 1)) {
        const auto value = lua_detail::ReadLua(lua_, -1);
        result.title = value.At("title").StringOr(result.title);
        result.hint = value.At("hint").StringOr("");
        result.cursor = value.At("cursor").StringOr("arrow");
        result.notice = value.At("notice").StringOr("");
      }
    }
  } catch (const std::exception& error) {
    error_ = error.what();
  }
  lua_settop(lua_, top);
  return result;
}
}  // namespace drawer
