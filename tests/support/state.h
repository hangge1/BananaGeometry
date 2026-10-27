#ifndef DRAWER_TESTS_SUPPORT_STATE_H_
#define DRAWER_TESTS_SUPPORT_STATE_H_
#include "core/value.h"

namespace drawer::testing {
inline Value EmptyState() {
  return Value::Object{
      {"objects", Value::Array{}}, {"params", Value::Object{}}, {"next_id", 1}};
}
}  // namespace drawer::testing
#endif  // DRAWER_TESTS_SUPPORT_STATE_H_
