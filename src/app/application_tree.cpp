#include "app/application.h"
#include "imgui.h"
namespace drawer {
void Application::Tree() {
  ImGui::TextUnformatted("文档管理器");
  ImGui::Separator();
  for (auto& tab : tabs_) {
    auto& d = *tab.document;
    const int doc_id = tab.key;
    ImGui::PushID(doc_id);
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow |
                               ImGuiTreeNodeFlags_DefaultOpen |
                               ImGuiTreeNodeFlags_SpanAvailWidth;
    if (active_ == doc_id && selection_ == Selection::kDocument)
      flags |= ImGuiTreeNodeFlags_Selected;
    const bool open = ImGui::TreeNodeEx(
        "document", flags, "%s%s", d.Name().c_str(), d.Dirty() ? " *" : "");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", d.Name().c_str());
    if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
      Queue([this, doc_id] { Activate(doc_id, 0); });
    if (ImGui::BeginPopupContextItem("document-context")) {
      if (ImGui::MenuItem("文档属性 / 重命名"))
        Queue([this, doc_id] { Activate(doc_id, 0); });
      if (ImGui::BeginMenu("添加用例实例")) {
        for (const auto& c : cases_)
          if (ImGui::MenuItem(c.name.c_str()))
            Queue([this, doc_id, path = c.entry] {
              Activate(doc_id, 0);
              NewCase(path);
            });
        ImGui::EndMenu();
      }
      if (ImGui::MenuItem("关闭文档")) Request(Action::kClose, doc_id);
      ImGui::EndPopup();
    }
    if (open) {
      for (const auto& entry : d.Instances()) {
        const int instance_id = entry.id;
        ImGui::PushID(instance_id);
        auto* s = entry.runtime->session.get();
        auto ef = ImGuiTreeNodeFlags_OpenOnArrow |
                  ImGuiTreeNodeFlags_DefaultOpen |
                  ImGuiTreeNodeFlags_SpanAvailWidth;
        if (active_ == doc_id && d.active_instance == instance_id &&
            selection_ == Selection::kInstance)
          ef |= ImGuiTreeNodeFlags_Selected;
        bool expanded = ImGui::TreeNodeEx(
            "instance", ef, "%s%s", entry.name.c_str(), s ? "" : " [不可用]");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", entry.name.c_str());
        if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
          Queue([this, doc_id, instance_id] { Activate(doc_id, instance_id); });
        if (ImGui::BeginPopupContextItem("instance-context")) {
          if (ImGui::MenuItem("实例属性 / 重命名"))
            Queue(
                [this, doc_id, instance_id] { Activate(doc_id, instance_id); });
          if (ImGui::MenuItem(s ? "定位实例" : "重试加载"))
            Queue([this, doc_id, instance_id] {
              Activate(doc_id, instance_id);
              if (Active())
                FitSelection();
              else
                RetryInstance();
            });
          if (ImGui::MenuItem("删除实例"))
            Request(Action::kRemoveInstance, doc_id, instance_id);
          ImGui::EndPopup();
        }
        if (expanded) {
          if (s) {
            const double selected_object = s->SelectedObject();
            for (const auto& object : s->State().At("objects").Items()) {
              const double id = object.At("id").Number();
              const auto name = object.At("display_name").StringOr("");
              const auto object_key = ToJson(Value(id));
              const auto label =
                  (name.empty() ? object.At("kind").String() + " " + object_key
                                : name);
              ImGui::PushID(object_key.c_str());
              bool selected =
                  active_ == doc_id && selection_ == Selection::kObject &&
                  d.active_instance == instance_id && selected_object == id;
              ImGui::TreeNodeEx(
                  "object",
                  ImGuiTreeNodeFlags_Leaf |
                      ImGuiTreeNodeFlags_NoTreePushOnOpen |
                      ImGuiTreeNodeFlags_SpanAvailWidth |
                      (selected ? ImGuiTreeNodeFlags_Selected : 0),
                  "%s", label.c_str());
              if (ImGui::IsItemHovered())
                ImGui::SetTooltip("%s", label.c_str());
              if (ImGui::IsItemClicked()) {
                const bool fit =
                    ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
                Queue([this, doc_id, instance_id, id, fit] {
                  Activate(doc_id, instance_id, id);
                  if (fit) FitSelection();
                });
              }
              if (ImGui::BeginPopupContextItem("object-context")) {
                if (ImGui::MenuItem("对象属性 / 重命名"))
                  Queue([this, doc_id, instance_id, id] {
                    Activate(doc_id, instance_id, id);
                  });
                if (ImGui::MenuItem("定位对象"))
                  Queue([this, doc_id, instance_id, id] {
                    Activate(doc_id, instance_id, id);
                    FitSelection();
                  });
                if (ImGui::MenuItem("删除对象", nullptr, false,
                                    !s->EditorMode().empty()))
                  Queue([this, doc_id, instance_id, id] {
                    Activate(doc_id, instance_id, id);
                    if (Active()) Active()->DeleteSelectedObject();
                  });
                ImGui::EndPopup();
              }
              ImGui::PopID();
            }
          } else
            ImGui::TextWrapped("%s", entry.runtime->error.c_str());
          ImGui::TreePop();
        }
        ImGui::PopID();
      }
      ImGui::TreePop();
    }
    ImGui::PopID();
  }
  if (tabs_.empty()) ImGui::TextWrapped("文件 → 新建文档\n用例 → 添加用例实例");
}
}  // namespace drawer
