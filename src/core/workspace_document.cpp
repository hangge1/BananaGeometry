#include "core/workspace_document.h"
#include <cmath>
#include <set>
#include <stdexcept>
namespace drawer {
Value EncodeWorkspace(const WorkspaceDocument& document) {
  if (document.name.empty() || document.name.size() > 1024 ||
      document.name.find('\0') != std::string::npos)
    throw std::runtime_error("Invalid document name");
  Value::Array instances;
  std::set<int> ids;
  for (const auto& instance : document.instances) {
    if (instance.id < 1 || instance.id > 2147483646 ||
        !ids.insert(instance.id).second || instance.name.empty() ||
        instance.name.size() > 1024 ||
        instance.name.find('\0') != std::string::npos ||
        instance.data.case_id.empty() || instance.data.case_version < 1 ||
        instance.data.case_version > 1000000)
      throw std::runtime_error("Invalid instance name or ID");
    instances.emplace_back(
        Value::Object{{"id", instance.id},
                      {"name", instance.name},
                      {"data", EncodeDocument(instance.data)}});
  }
  return Value::Object{{"format", "drawer"},
                       {"format_version", 2},
                       {"name", document.name},
                       {"instances", std::move(instances)}};
}
WorkspaceDocument DecodeWorkspace(const Value& root) {
  WorkspaceDocument result;
  if (root.At("format_version") == Value(1)) {
    auto data = DecodeDocument(root);
    result.instances.push_back({1, data.case_id, std::move(data)});
    result.imported_legacy = true;
    return result;
  }
  if (root.At("format") != Value("drawer") ||
      root.At("format_version") != Value(2) || !root.At("instances").IsArray())
    throw std::runtime_error("Unsupported workspace format");
  result.name = root.At("name").StringOr("");
  for (const auto& item : root.At("instances").Items()) {
    const double id = item.At("id").NumberOr(0);
    if (id < 1 || id > 2147483646 || id != std::floor(id))
      throw std::runtime_error("Invalid instance ID");
    result.instances.push_back({static_cast<int>(id),
                                item.At("name").StringOr(""),
                                DecodeDocument(item.At("data"))});
  }
  EncodeWorkspace(result);
  return result;
}
}  // namespace drawer
