#include <windows.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "document.h"

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
void HistoryTest() {
    drawer::History history;
    history.Reset(EmptyState());
    CHECK(!history.Dirty() && !history.CanUndo());
    auto changed = history.Current();
    changed["params"]["size"] = 10;
    history.Push(changed, "size");
    CHECK(history.Dirty() && history.Count() == 1);
    history.Push(changed, "empty");
    CHECK(history.Count() == 1);
    history.MarkSaved();
    CHECK(!history.Dirty());
    CHECK(history.Undo() && history.Dirty());
    CHECK(history.Redo() && !history.Dirty());
    CHECK(history.Undo());
    changed["params"]["size"] = 20;
    history.Push(changed, "branch");
    CHECK(!history.CanRedo());
    for (int i = 0; i < 150; ++i) {
        changed["params"]["size"] = i;
        history.Push(changed, "many");
    }
    CHECK(history.Count() == 100 && history.Current().At("params").At("size").Number() == 149);
}
void ViewTest() {
    drawer::View view{{125.4, -87.2}, 3.25};
    drawer::Point size{1000, 720}, p{12.34, 56.78};
    auto screen = view.ToScreen(p, size), back = view.ToWorld(screen, size);
    CHECK(drawer::Distance(p, back) < 1e-10);
    CHECK(view.ToScreen({0, 100}, size).y < view.ToScreen({0, 0}, size).y);
    auto anchor = view.ToWorld({123, 456}, size);
    view.ZoomAt({123, 456}, size, 2);
    CHECK(drawer::Distance(anchor, view.ToWorld({123, 456}, size)) < 1e-10);
    auto before = view.ToScreen(p, size);
    view.Pan({10, -25});
    auto after = view.ToScreen(p, size);
    CHECK(std::abs(after.x - before.x - 10) < 1e-10 && std::abs(after.y - before.y + 25) < 1e-10);
    CHECK(drawer::SegmentDistance({1, 2}, {0, 0}, {2, 0}) == 2);
    CHECK(drawer::SegmentDistance({3, 4}, {0, 0}, {0, 0}) == 5);
    view.ZoomAt({0, 0}, size, 1e100);
    CHECK(view.zoom == 10000);
}

void CodecTest() {
    drawer::Document document{"example.codec", 1, EmptyState(), {{12, -34}, 2.5}};
    auto decoded = drawer::DecodeDocument(drawer::ParseJson(drawer::ToJson(drawer::EncodeDocument(document))));
    CHECK(decoded.case_id == document.case_id && decoded.state == document.state);
    CHECK(decoded.view.center.y == -34 && decoded.view.zoom == 2.5);
    auto invalid = drawer::EncodeDocument(document);
    invalid["view"]["zoom"] = 0;
    Rejects([&] { drawer::DecodeDocument(invalid); });
}

} // namespace
int main(int argc,char** argv) { try {
CHECK(argc==3); std::string suite=argv[1];
if(suite=="json") JsonTest();
else if(suite=="history") HistoryTest(); else if(suite=="view") ViewTest(); else if(suite=="codec") CodecTest();
else throw std::runtime_error("Unknown suite"); std::cout<<suite<<": PASS\n"; return 0;
} catch(const std::exception& e){ std::cerr<<e.what()<<'\n'; return 1; }}
