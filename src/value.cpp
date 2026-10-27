#include "value.h"

#include <cmath>
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>

namespace drawer {
namespace {
void CheckUtf8(const std::string& text) {
  for (size_t i = 0; i < text.size();) {
    auto c = static_cast<unsigned char>(text[i++]);
    if (c < 128) continue;
    int count = c >= 0xc2 && c <= 0xdf   ? 1
                : c >= 0xe0 && c <= 0xef ? 2
                : c >= 0xf0 && c <= 0xf4 ? 3
                                         : 0;
    if (!count || text.size() - i < static_cast<size_t>(count))
      throw std::runtime_error("Invalid UTF-8 string");
    unsigned code = c & ((1 << (6 - count)) - 1);
    for (int j = 0; j < count; ++j) {
      auto next = static_cast<unsigned char>(text[i++]);
      if ((next & 0xc0) != 0x80)
        throw std::runtime_error("Invalid UTF-8 continuation");
      code = (code << 6) | (next & 63);
    }
    if (code < (count == 1   ? 128u
                : count == 2 ? 2048u
                             : 65536u) ||
        code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
      throw std::runtime_error("Invalid UTF-8 codepoint");
  }
}
void Utf8(unsigned code, std::string* result) {
  if (code <= 0x7f)
    result->push_back(static_cast<char>(code));
  else if (code <= 0x7ff) {
    result->push_back(static_cast<char>(0xc0 | (code >> 6)));
    result->push_back(static_cast<char>(0x80 | (code & 63)));
  } else if (code <= 0xffff) {
    result->push_back(static_cast<char>(0xe0 | (code >> 12)));
    result->push_back(static_cast<char>(0x80 | ((code >> 6) & 63)));
    result->push_back(static_cast<char>(0x80 | (code & 63)));
  } else {
    result->push_back(static_cast<char>(0xf0 | (code >> 18)));
    result->push_back(static_cast<char>(0x80 | ((code >> 12) & 63)));
    result->push_back(static_cast<char>(0x80 | ((code >> 6) & 63)));
    result->push_back(static_cast<char>(0x80 | (code & 63)));
  }
}

class Parser {
 public:
  explicit Parser(const std::string& text) : text_(text) {}
  Value Parse() {
    if (text_.size() > 16 * 1024 * 1024) Fail("Document exceeds 16 MiB");
    Value result = Read(0);
    Space();
    if (position_ != text_.size()) Fail("Trailing data");
    return result;
  }

 private:
  [[noreturn]] void Fail(const char* reason) const {
    throw std::runtime_error(std::string(reason) + " at byte " +
                             std::to_string(position_));
  }
  void Space() {
    while (position_ < text_.size() &&
           (text_[position_] == ' ' || text_[position_] == '\n' ||
            text_[position_] == '\r' || text_[position_] == '\t'))
      ++position_;
  }
  bool Take(char c) {
    Space();
    if (position_ < text_.size() && text_[position_] == c) {
      ++position_;
      return true;
    }
    return false;
  }
  void Expect(char c) {
    if (!Take(c)) Fail("Unexpected token");
  }
  bool Digit() const {
    return position_ < text_.size() && text_[position_] >= '0' &&
           text_[position_] <= '9';
  }
  unsigned Hex() {
    unsigned result = 0;
    for (int i = 0; i < 4; ++i) {
      if (position_ == text_.size()) Fail("Incomplete Unicode escape");
      char c = text_[position_++];
      unsigned n = c >= '0' && c <= '9'   ? c - '0'
                   : c >= 'a' && c <= 'f' ? c - 'a' + 10
                   : c >= 'A' && c <= 'F' ? c - 'A' + 10
                                          : 16;
      if (n == 16) Fail("Invalid Unicode escape");
      result = result * 16 + n;
    }
    return result;
  }
  std::string String() {
    Expect('"');
    std::string result;
    while (position_ < text_.size()) {
      unsigned char c = static_cast<unsigned char>(text_[position_++]);
      if (c == '"') return result;
      if (c < 32) Fail("Unescaped control character");
      if (c != '\\') {
        if (c < 128)
          result.push_back(static_cast<char>(c));
        else {
          int count = c >= 0xc2 && c <= 0xdf   ? 1
                      : c >= 0xe0 && c <= 0xef ? 2
                      : c >= 0xf0 && c <= 0xf4 ? 3
                                               : 0;
          if (!count) Fail("Invalid UTF-8");
          unsigned code = c & ((1 << (6 - count)) - 1);
          for (int i = 0; i < count; ++i) {
            if (position_ == text_.size()) Fail("Incomplete UTF-8");
            unsigned char next = static_cast<unsigned char>(text_[position_++]);
            if ((next & 0xc0) != 0x80) Fail("Invalid UTF-8 continuation");
            code = (code << 6) | (next & 63);
          }
          if (code < (count == 1   ? 128u
                      : count == 2 ? 2048u
                                   : 65536u) ||
              code > 0x10ffff || (code >= 0xd800 && code <= 0xdfff))
            Fail("Invalid UTF-8 codepoint");
          Utf8(code, &result);
        }
        continue;
      }
      if (position_ == text_.size()) Fail("Incomplete escape");
      switch (text_[position_++]) {
        case '"':
          result += '"';
          break;
        case '\\':
          result += '\\';
          break;
        case '/':
          result += '/';
          break;
        case 'b':
          result += '\b';
          break;
        case 'f':
          result += '\f';
          break;
        case 'n':
          result += '\n';
          break;
        case 'r':
          result += '\r';
          break;
        case 't':
          result += '\t';
          break;
        case 'u': {
          unsigned code = Hex();
          if (code >= 0xd800 && code <= 0xdbff) {
            if (position_ + 2 > text_.size() || text_[position_++] != '\\' ||
                text_[position_++] != 'u')
              Fail("Missing low surrogate");
            unsigned low = Hex();
            if (low < 0xdc00 || low > 0xdfff) Fail("Invalid low surrogate");
            code = 0x10000 + ((code - 0xd800) << 10) + low - 0xdc00;
          } else if (code >= 0xdc00 && code <= 0xdfff)
            Fail("Unexpected low surrogate");
          Utf8(code, &result);
          break;
        }
        default:
          Fail("Invalid escape");
      }
    }
    Fail("Unterminated string");
  }
  Value Read(int depth) {
    if (depth > 64) Fail("Nesting limit exceeded");
    Space();
    if (position_ == text_.size()) Fail("Unexpected end");
    if (text_[position_] == '"') return String();
    if (Take('{')) {
      Value::Object result;
      if (Take('}')) return result;
      do {
        std::string key = String();
        Expect(':');
        if (!result.emplace(key, Read(depth + 1)).second)
          Fail("Duplicate object key");
      } while (Take(','));
      Expect('}');
      return result;
    }
    if (Take('[')) {
      Value::Array result;
      if (Take(']')) return result;
      do {
        result.push_back(Read(depth + 1));
      } while (Take(','));
      Expect(']');
      return result;
    }
    for (const auto& literal :
         {std::string("true"), std::string("false"), std::string("null")}) {
      if (text_.compare(position_, literal.size(), literal) == 0) {
        position_ += literal.size();
        return literal == "null" ? Value() : Value(literal == "true");
      }
    }
    const auto begin = position_;
    if (text_[position_] == '-') ++position_;
    if (!Digit()) Fail("Expected number");
    if (text_[position_] == '0')
      ++position_;
    else
      while (Digit()) ++position_;
    if (position_ < text_.size() && text_[position_] == '.') {
      ++position_;
      if (!Digit()) Fail("Expected fractional digit");
      while (Digit()) ++position_;
    }
    if (position_ < text_.size() &&
        (text_[position_] == 'e' || text_[position_] == 'E')) {
      ++position_;
      if (position_ < text_.size() &&
          (text_[position_] == '+' || text_[position_] == '-'))
        ++position_;
      if (!Digit()) Fail("Expected exponent digit");
      while (Digit()) ++position_;
    }
    std::istringstream input(text_.substr(begin, position_ - begin));
    input.imbue(std::locale::classic());
    double number = 0;
    input >> number;
    if (input.fail() || !std::isfinite(number))
      Fail("Invalid or nonfinite number");
    return number;
  }
  const std::string& text_;
  size_t position_ = 0;
};

void Write(const Value& value, std::ostream* out, int depth) {
  if (depth > 64) throw std::runtime_error("JSON nesting limit exceeded");
  if (value.IsNull())
    *out << "null";
  else if (value.IsBool())
    *out << (value.Bool() ? "true" : "false");
  else if (value.IsNumber()) {
    if (!std::isfinite(value.Number()))
      throw std::runtime_error("Cannot encode nonfinite number");
    *out << std::setprecision(std::numeric_limits<double>::max_digits10)
         << value.Number();
  } else if (value.IsString()) {
    CheckUtf8(value.String());
    *out << '"';
    constexpr char kHex[] = "0123456789abcdef";
    for (unsigned char c : value.String()) {
      if (c == '"' || c == '\\')
        *out << '\\' << static_cast<char>(c);
      else if (c < 32)
        *out << "\\u00" << kHex[c >> 4] << kHex[c & 15];
      else
        *out << static_cast<char>(c);
    }
    *out << '"';
  } else if (value.IsArray()) {
    *out << '[';
    bool first = true;
    for (const auto& item : value.Items()) {
      if (!first) *out << ',';
      first = false;
      Write(item, out, depth + 1);
    }
    *out << ']';
  } else {
    *out << '{';
    bool first = true;
    for (const auto& [key, item] : value.Fields()) {
      if (!first) *out << ',';
      first = false;
      Write(key, out, depth + 1);
      *out << ':';
      Write(item, out, depth + 1);
    }
    *out << '}';
  }
}
}  // namespace

const Value& Value::At(const std::string& key) const {
  static const Value kNull;
  if (!IsObject()) return kNull;
  auto it = Fields().find(key);
  return it == Fields().end() ? kNull : it->second;
}

std::string ToJson(const Value& value) {
  std::ostringstream output;
  output.imbue(std::locale::classic());
  Write(value, &output, 0);
  return output.str();
}
Value ParseJson(const std::string& text) { return Parser(text).Parse(); }
}  // namespace drawer
