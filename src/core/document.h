#ifndef DRAWER_SRC_CORE_DOCUMENT_H_
#define DRAWER_SRC_CORE_DOCUMENT_H_
#include <string>
#include "core/value.h"
#include "core/geometry.h"
namespace drawer {
// Editable data and view only; never contains callbacks or execution resources.
struct Document {
  std::string case_id;
  int case_version = 1;
  Value state;
  View view;
};
void ValidateState(const Value& state);
Value EncodeDocument(const Document& document);
Document DecodeDocument(const Value& root);
void FitView(View* view, const Value& state, Point size);
}  // namespace drawer
#endif  // DRAWER_SRC_CORE_DOCUMENT_H_
