#ifndef DRAWER_SRC_APP_LUA_UI_H_
#define DRAWER_SRC_APP_LUA_UI_H_

#include <string>
#include <vector>

#include "core/value.h"

namespace drawer {
// Adapts one Lua widget request to value/changed/edit-ended results.
// Call only inside the current ImGui frame on its owning UI thread.
std::vector<Value> DrawLuaWidget(const std::string& operation,
                                 const Value::Array& arguments);
}  // namespace drawer

#endif  // DRAWER_SRC_APP_LUA_UI_H_
