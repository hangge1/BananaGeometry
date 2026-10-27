#include "runtime/detail/lua_support.h"

#include <cmath>
#include <fstream>
#include <set>
#include <stdexcept>

namespace drawer::lua_detail {
std::string ReadScript(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input)
    throw std::runtime_error("Cannot open Lua script: " + path.u8string());
  if (std::filesystem::file_size(path) > 1024 * 1024)
    throw std::runtime_error("Script exceeds 1 MiB");
  return std::string((std::istreambuf_iterator<char>(input)), {});
}
int LoadFile(lua_State* state, const std::filesystem::path& path) {
  const auto source = ReadScript(path);
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
}  // namespace drawer::lua_detail
