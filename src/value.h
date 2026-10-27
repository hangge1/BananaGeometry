#ifndef DRAWER_SRC_VALUE_H_
#define DRAWER_SRC_VALUE_H_

#include <map>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace drawer {

// Owns a JSON-compatible value. Conversion constructors intentionally remain
// implicit so callers can compose object and array literals without wrappers.
class Value {
 public:
  using Array = std::vector<Value>;
  using Object = std::map<std::string, Value>;
  Value() = default;
  Value(bool value) : data_(value) {}
  Value(double value) : data_(value) {}
  Value(int value) : data_(static_cast<double>(value)) {}
  Value(const char* value) : data_(std::string(value)) {}
  Value(std::string value) : data_(std::move(value)) {}
  Value(Array value) : data_(std::move(value)) {}
  Value(Object value) : data_(std::move(value)) {}
  bool IsNull() const { return std::holds_alternative<std::monostate>(data_); }
  bool IsBool() const { return std::holds_alternative<bool>(data_); }
  bool IsNumber() const { return std::holds_alternative<double>(data_); }
  bool IsString() const { return std::holds_alternative<std::string>(data_); }
  bool IsArray() const { return std::holds_alternative<Array>(data_); }
  bool IsObject() const { return std::holds_alternative<Object>(data_); }
  bool Bool() const { return std::get<bool>(data_); }
  double Number() const { return std::get<double>(data_); }
  const std::string& String() const { return std::get<std::string>(data_); }
  const Array& Items() const { return std::get<Array>(data_); }
  Array& Items() { return std::get<Array>(data_); }
  const Object& Fields() const { return std::get<Object>(data_); }
  Object& Fields() { return std::get<Object>(data_); }
  // Returns a stable null sentinel for missing keys and non-object values.
  const Value& At(const std::string& key) const;
  Value& operator[](const std::string& key) { return Fields()[key]; }
  double NumberOr(double fallback) const {
    return IsNumber() ? Number() : fallback;
  }
  std::string StringOr(const std::string& fallback) const {
    return IsString() ? String() : fallback;
  }
  bool operator==(const Value& other) const { return data_ == other.data_; }
  bool operator!=(const Value& other) const { return !(*this == other); }

 private:
  std::variant<std::monostate, bool, double, std::string, Array, Object> data_;
};

// These functions reject invalid UTF-8, nonfinite numbers and excessive depth.
// ParseJson also rejects duplicate keys; neither silently repairs invalid data.
std::string ToJson(const Value& value);
Value ParseJson(const std::string& text);

}  // namespace drawer

#endif  // DRAWER_SRC_VALUE_H_
