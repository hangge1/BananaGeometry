#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include "core/value.h"

namespace drawer {
TEST_CASE("JSON preserves Unicode and empty container types", "[json]") {
  const auto format = GENERATE(JsonFormat::kCompact, JsonFormat::kIndented);
  const auto value = ParseJson(
      R"({"label":"绘制\n测试","array":[],"object":{},"v":[true,false,null,"\uD83D\uDE80"]})");
  CHECK(ParseJson(ToJson(value, format)) == value);
  CHECK(value.At("array").IsArray());
  CHECK(value.At("object").IsObject());
  CHECK(value.At("v").Items()[3].String() == "🚀");
}
TEST_CASE("JSON rejects malformed syntax and ambiguous objects", "[json]") {
  const auto input =
      GENERATE(std::string(""), std::string("[1,]"), std::string("[01]"),
               std::string("[+1]"), std::string("[1.]"), std::string("[1e]"),
               std::string("[1e999]"), std::string("{\"a\":1,\"a\":2}"),
               std::string("\"\\uD800\""), std::string("\"\\uDC00\""),
               std::string("true false"), std::string("{\"x\":NaN}"),
               std::string("\"a\n\""));
  CAPTURE(input);
  REQUIRE_THROWS_AS(ParseJson(input), std::runtime_error);
}
TEST_CASE("JSON rejects invalid UTF-8 on input and output", "[json]") {
  const auto format = GENERATE(JsonFormat::kCompact, JsonFormat::kIndented);
  CHECK_THROWS_AS(ParseJson(std::string("\"\xc0\xaf\"")), std::runtime_error);
  CHECK_THROWS_AS(ToJson(std::string("\xc0\xaf"), format), std::runtime_error);
}
TEST_CASE("JSON limits nesting and nonfinite numbers", "[json]") {
  const auto format = GENERATE(JsonFormat::kCompact, JsonFormat::kIndented);
  CHECK_THROWS_AS(ParseJson(std::string(70, '[') + "0" + std::string(70, ']')),
                  std::runtime_error);
  CHECK_THROWS_AS(ToJson(std::numeric_limits<double>::infinity(), format),
                  std::runtime_error);
  Value nested = 0;
  for (int i = 0; i < 65; ++i) nested = Value::Array{std::move(nested)};
  CHECK_THROWS_AS(ToJson(nested, format), std::runtime_error);
}
TEST_CASE("JSON numbers round trip without precision loss", "[json]") {
  const auto format = GENERATE(JsonFormat::kCompact, JsonFormat::kIndented);
  const double value = GENERATE(0.0, -0.0, 1e-300, 1e300, 1.2345678901234567);
  CAPTURE(value);
  CHECK(ParseJson(ToJson(value, format)).Number() == value);
}
TEST_CASE(
    "Indented JSON exposes container structure and preserves compact defaults",
    "[json]") {
  const auto value =
      ParseJson(R"({"a":[{"x":1},[],{}],"empty":[],"name":"中文"})");
  const std::string expected =
      "{\n"
      "  \"a\": [\n"
      "    {\n"
      "      \"x\": 1\n"
      "    },\n"
      "    [],\n"
      "    {}\n"
      "  ],\n"
      "  \"empty\": [],\n"
      "  \"name\": \"中文\"\n"
      "}";
  CHECK(ToJson(value, JsonFormat::kIndented) == expected);
  CHECK(ToJson(value) == R"({"a":[{"x":1},[],{}],"empty":[],"name":"中文"})");
}
TEST_CASE("Indented JSON keeps punctuation and controls inside strings intact",
          "[json]") {
  const Value value = Value::Object{{"a\"[,:]", "中文\n\r\t\"\\[{},]:🚀"},
                                    {"control", std::string("\0\b\f", 3)}};
  const auto text = ToJson(value, JsonFormat::kIndented);
  CHECK(ParseJson(text) == value);
  CHECK(text.find("中文") != std::string::npos);
  CHECK(text.find("🚀") != std::string::npos);
}
TEST_CASE("Missing fields return a null sentinel", "[json]") {
  const Value object = Value::Object{{"present", 1}};
  CHECK(object.At("absent").IsNull());
  CHECK(Value(2).At("absent").IsNull());
}
}  // namespace drawer
