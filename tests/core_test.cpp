#include <windows.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "value.h"

namespace {
using drawer::Value;
#define CHECK(condition)                                                                      \
    do {                                                                                      \
        if (!(condition))                                                                     \
            throw std::runtime_error(std::string(__FILE__) + ":" + std::to_string(__LINE__) + \
                                     " CHECK failed: " #condition);                           \
    } while (false)
template <class Function>
void Rejects(Function function) {
    bool rejected = false;
    try {
        function();
    } catch (const std::exception&) {
        rejected = true;
    }
    CHECK(rejected);
}
Value EmptyState() {
    return Value::Object{{"objects", Value::Array{}}, {"params", Value::Object{}}, {"next_id", 1}};
}
void JsonTest() {
    auto root = drawer::ParseJson(
        R"({"label":"绘制\n测试","empty_array":[],"empty_object":{},"n":-1.2345678901234567e10,"v":[true,false,null,"\uD83D\uDE80"]})");
    CHECK(drawer::ParseJson(drawer::ToJson(root)) == root);
    CHECK(root.At("empty_array").IsArray());
    CHECK(root.At("empty_object").IsObject());
    CHECK(root.At("v").Items()[3].String() == "🚀");
    for (const char* invalid :
         {"", "[1,]", "[01]", "[+1]", "[1.]", "[1e]", "[1e999]", "{\"a\":1,\"a\":2}", "\"\\uD800\"",
          "\"\\uDC00\"", "true false", "{\"x\":NaN}", "\"a\n\""})
        Rejects([&] { drawer::ParseJson(invalid); });
    Rejects([] { drawer::ParseJson(std::string("\"\xc0\xaf\"")); });
    Rejects([] { drawer::ParseJson(std::string(70, '[') + "0" + std::string(70, ']')); });
    Rejects([] { drawer::ToJson(std::numeric_limits<double>::infinity()); });
    Rejects([] { drawer::ToJson(std::string("\xc0\xaf")); });
    for (double value : {0.0, -0.0, 1e-300, 1e300, 1.2345678901234567})
        CHECK(drawer::ParseJson(drawer::ToJson(value)).Number() == value);
}

} // namespace
int main(int argc,char** argv) { try {
CHECK(argc==3); std::string suite=argv[1];
if(suite=="json") JsonTest();
else throw std::runtime_error("Unknown suite"); std::cout<<suite<<": PASS\n"; return 0;
} catch(const std::exception& e){ std::cerr<<e.what()<<'\n'; return 1; }}
