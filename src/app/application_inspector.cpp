#include "app/application.h"
#include <algorithm>
#include <cstring>
#include "app/lua_ui.h"
#include "imgui.h"
namespace drawer {
bool Application::NameField(const char* label, const std::string& current,
                            std::string* result) {
  const auto key = ImGui::GetID(label);
  if (rename_key_ != key ||
      (!ImGui::IsAnyItemActive() && current != rename_buffer_.data())) {
    rename_key_ = key;
    rename_buffer_.fill(0);
    memcpy(rename_buffer_.data(), current.data(),
           std::min(current.size(), rename_buffer_.size() - 1));
  }
  ImGui::InputText(label, rename_buffer_.data(), rename_buffer_.size());
  if (ImGui::IsItemEdited() && rename_buffer_[0]) {
    const int document = active_;
    const int instance = selection_ == Selection::kInstance && ActiveEntry()
                             ? ActiveEntry()->id
                             : 0;
    rename_edit_ = [this, document, instance,
                    name = std::string(rename_buffer_.data())] {
      if (auto* target = FindDocument(document)) {
        if (instance)
          target->RenameInstance(instance, name);
        else
          target->Rename(name);
      }
    };
  }
  if (ImGui::IsItemDeactivatedAfterEdit() && rename_buffer_[0]) {
    rename_edit_ = {};
    *result = rename_buffer_.data();
    return true;
  }
  return false;
}
void Application::Inspector() {
  auto* d = ActiveDocument();
  auto* e = ActiveEntry();
  auto* s = Active();
  ImGui::TextUnformatted("属性");
  ImGui::Separator();
  if (!d) {
    ImGui::TextUnformatted("未打开文档");
    return;
  }
  const auto ui = [](const auto& op, const auto& args) {
    return DrawLuaWidget(op, args);
  };
  std::string renamed;
  ImGui::PushID(active_);
  if (selection_ == Selection::kDocument) {
    if (NameField("文档名称", d->Name(), &renamed)) d->Rename(renamed);
    ImGui::TextWrapped(
        "文件：%s", d->file.empty() ? "尚未保存" : d->file.u8string().c_str());
    ImGui::Text("实例数量：%d", static_cast<int>(d->Instances().size()));
    ImGui::TextUnformatted(d->Dirty() ? "有未保存的修改" : "已保存 / 无修改");
    if (d->legacy) ImGui::TextWrapped("旧版文件首次保存默认另存为。");
  } else if (e) {
    ImGui::PushID(e->id);
    if (selection_ == Selection::kObject && s && s->SelectedObject()) {
      if (ImGui::CollapsingHeader("所选对象", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID("object-properties");
        s->ObjectControls(ui);
        ImGui::PopID();
      }
    } else {
      if (NameField("实例名称", e->name, &renamed))
        d->RenameInstance(e->id, renamed);
      e = ActiveEntry();
      if (e) {
        ImGui::TextWrapped(
            "用例：%s",
            s ? s->Id().c_str() : e->runtime->unavailable.case_id.c_str());
        if (!s) {
          ImGui::TextWrapped("%s", e->runtime->error.c_str());
          if (ImGui::Button("重试加载")) RetryInstance();
        }
      }
    }
    if (s) {
      if (ImGui::CollapsingHeader("用例工具", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::PushID("case-tools");
        s->Controls(ui);
        ImGui::PopID();
      }
      if (ImGui::CollapsingHeader("实例与历史")) {
        ImGui::TextWrapped("脚本：%s", s->Entry().u8string().c_str());
        ImGui::TextWrapped("撤销：%s\n重做：%s",
                           s->GetHistory().UndoLabel().c_str(),
                           s->GetHistory().RedoLabel().c_str());
      }
    }
    ImGui::PopID();
  }
  ImGui::PopID();
}
}  // namespace drawer
