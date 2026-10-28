#ifndef DRAWER_SRC_CORE_WORKSPACE_DOCUMENT_H_
#define DRAWER_SRC_CORE_WORKSPACE_DOCUMENT_H_
#include <string>
#include <vector>
#include "core/document.h"
namespace drawer {
struct InstanceDocument {
  int id = 1;
  std::string name;
  Document data;
};
// File container. Session snapshots retain their v1 API and validation.
struct WorkspaceDocument {
  std::string name = "未命名文档";
  std::vector<InstanceDocument> instances;
  bool imported_legacy = false;
};
// Invalid containers throw; legacy v1 input imports as one instance.
Value EncodeWorkspace(const WorkspaceDocument& document);
WorkspaceDocument DecodeWorkspace(const Value& root);
}  // namespace drawer
#endif  // DRAWER_SRC_CORE_WORKSPACE_DOCUMENT_H_
