#include "runtime/session.h"
#include "runtime/detail/lua_support.h"
namespace drawer {
bool Session::ContextCall(const char* method, const Value::Array& arguments,
                          Value* result) {
  const int top = lua_gettop(lua_);
  try {
    lua_rawgeti(lua_, LUA_REGISTRYINDEX, context_);
    lua_getfield(lua_, -1, method);
    lua_pushvalue(lua_, -2);
    for (const auto& argument : arguments) lua_detail::PushLua(lua_, argument);
    const bool ok =
        ProtectedCall(static_cast<int>(arguments.size()) + 1, result ? 1 : 0);
    if (ok && result) *result = lua_detail::ReadLua(lua_, -1);
    lua_settop(lua_, top);
    if (!ok) Cancel();
    return ok;
  } catch (const std::exception& error) {
    error_ = error.what();
    lua_settop(lua_, top);
    Cancel();
    return false;
  }
}
bool Session::SelectObject(double id) {
  readonly_inspection_ = editor_mode_.empty();
  return ContextCall("select_object", {id, editor_mode_});
}
bool Session::FinishUiEdit() { return ContextCall("finish_ui_edit", {}); }
double Session::SelectedObject() {
  Value result;
  return ContextCall("selected_object_id", {}, &result) ? result.NumberOr(0)
                                                        : 0;
}
bool Session::DeleteSelectedObject() {
  return ContextCall("delete_selected_object", {});
}
bool Session::ObjectControls(UiCallback ui) {
  ui_ = std::move(ui);
  const bool ok = ContextCall("inspect_object", {SelectedObject()});
  ui_ = {};
  return ok;
}
Value Session::ObjectBounds(double id) {
  Value result = Value::Array{};
  ContextCall("object_bounds", {id}, &result);
  return result;
}
}  // namespace drawer
