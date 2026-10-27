#include "app/lua_ui.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

#include "imgui.h"

namespace drawer {
namespace {
int Integer(const Value& value, int fallback) {
  const double number = value.NumberOr(fallback);
  if (!std::isfinite(number)) return fallback;
  return static_cast<int>(std::clamp(number,
                                     double(std::numeric_limits<int>::min()),
                                     double(std::numeric_limits<int>::max())));
}
}  // namespace
std::vector<Value> DrawLuaWidget(const std::string& op,
                                 const Value::Array& args) {
  auto arg = [&](size_t i) -> const Value& {
    static const Value null;
    return i < args.size() ? args[i] : null;
  };
  auto label = arg(0).StringOr("");
  bool changed = false;
  bool finished = false;
  Value value = arg(1);
  if (op == "slider" || op == "number" || op == "integer" || op == "input" ||
      op == "combo") {
    const float label_width =
        ImGui::CalcTextSize(label.c_str(), nullptr, true).x;
    ImGui::SetNextItemWidth(
        std::max(60.f, ImGui::GetContentRegionAvail().x - label_width -
                           ImGui::GetStyle().ItemInnerSpacing.x));
  }
  if (op == "button") {
    changed = ImGui::Button(label.c_str());
    return {changed, changed, changed};
  }
  if (op == "text")
    ImGui::TextWrapped("%s", label.c_str());
  else if (op == "same_line")
    ImGui::SameLine();
  else if (op == "separator")
    ImGui::Separator();
  else if (op == "slider" || op == "number") {
    double number = value.NumberOr(0);
    const double lower = arg(2).NumberOr(0);
    const double upper = arg(3).NumberOr(100);
    if (op == "slider")
      changed = ImGui::SliderScalar(label.c_str(), ImGuiDataType_Double,
                                    &number, &lower, &upper, "%.2f");
    else
      changed = ImGui::InputDouble(label.c_str(), &number, 1, 10, "%.2f");
    if (!std::isfinite(number)) number = 0;
    value = std::clamp(number, lower, upper);
    finished = ImGui::IsItemDeactivatedAfterEdit();
  } else if (op == "integer") {
    const int lower = Integer(arg(2), 0);
    const int upper = Integer(arg(3), 100);
    if (lower > upper) throw std::runtime_error("Invalid integer widget range");
    int number = std::clamp(Integer(value, lower), lower, upper);
    changed = ImGui::InputInt(label.c_str(), &number);
    value = std::clamp(number, lower, upper);
    finished = ImGui::IsItemDeactivatedAfterEdit();
  } else if (op == "checkbox") {
    bool boolean = value.IsBool() && value.Bool();
    changed = ImGui::Checkbox(label.c_str(), &boolean);
    value = boolean;
    finished = changed;
  } else if (op == "input") {
    char text[4096]{};
    auto string = value.StringOr("");
    memcpy(text, string.data(), std::min(string.size(), sizeof(text) - 1));
    changed = ImGui::InputText(label.c_str(), text, sizeof(text));
    value = std::string(text);
    finished = ImGui::IsItemDeactivatedAfterEdit();
  } else if (op == "combo" && arg(2).IsArray()) {
    const auto& choices = arg(2).Items();
    int current = static_cast<int>(value.NumberOr(1)) - 1;
    const char* preview = current >= 0 &&
                                  current < static_cast<int>(choices.size()) &&
                                  choices[current].IsString()
                              ? choices[current].String().c_str()
                              : "";
    if (ImGui::BeginCombo(label.c_str(), preview)) {
      for (size_t i = 0; i < choices.size(); ++i) {
        auto text = choices[i].StringOr("");
        if (ImGui::Selectable(text.c_str(), static_cast<int>(i) == current)) {
          value = static_cast<int>(i + 1);
          changed = finished = true;
        }
      }
      ImGui::EndCombo();
    }
  } else if (op != "text" && op != "same_line" && op != "separator")
    throw std::runtime_error("Unknown UI operation: " + op);
  return {value, changed, finished};
}
}  // namespace drawer
