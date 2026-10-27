#include <windows.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

#include "document.h"
#include "session.h"

namespace {
using drawer::Value;
#define CHECK(condition)                                      \
  do {                                                        \
    if (!(condition))                                         \
      throw std::runtime_error(std::string(__FILE__) + ":" +  \
                               std::to_string(__LINE__) +     \
                               " CHECK failed: " #condition); \
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
  return Value::Object{
      {"objects", Value::Array{}}, {"params", Value::Object{}}, {"next_id", 1}};
}
void JsonTest() {
  auto root = drawer::ParseJson(
      R"({"label":"绘制\n测试","empty_array":[],"empty_object":{},"n":-1.2345678901234567e10,"v":[true,false,null,"\uD83D\uDE80"]})");
  CHECK(drawer::ParseJson(drawer::ToJson(root)) == root);
  CHECK(root.At("empty_array").IsArray());
  CHECK(root.At("empty_object").IsObject());
  CHECK(root.At("v").Items()[3].String() == "🚀");
  for (const char* invalid :
       {"", "[1,]", "[01]", "[+1]", "[1.]", "[1e]", "[1e999]",
        "{\"a\":1,\"a\":2}", "\"\\uD800\"", "\"\\uDC00\"", "true false",
        "{\"x\":NaN}", "\"a\n\""})
    Rejects([&] { drawer::ParseJson(invalid); });
  Rejects([] { drawer::ParseJson(std::string("\"\xc0\xaf\"")); });
  Rejects([] {
    drawer::ParseJson(std::string(70, '[') + "0" + std::string(70, ']'));
  });
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
  CHECK(history.Count() == 100 &&
        history.Current().At("params").At("size").Number() == 149);
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
  CHECK(std::abs(after.x - before.x - 10) < 1e-10 &&
        std::abs(after.y - before.y + 25) < 1e-10);
  CHECK(drawer::SegmentDistance({1, 2}, {0, 0}, {2, 0}) == 2);
  CHECK(drawer::SegmentDistance({3, 4}, {0, 0}, {0, 0}) == 5);
  view.ZoomAt({0, 0}, size, 1e100);
  CHECK(view.zoom == 10000);
}
Value Event(const char* type, double x = 0, double y = 0,
            const char* key = "") {
  return Value::Object{
      {"type", type}, {"x", x}, {"y", y}, {"key", key}, {"button", 1}};
}
void CreateTriangle(drawer::Session* session) {
  CHECK(session->Event(Event("down", 0, 0)));
  CHECK(session->Event(Event("down", 100, 0)));
  CHECK(session->Event(Event("down", 50, 100)));
}
void LuaTest(const std::filesystem::path& root) {
  auto sdk = root / "sdk/drawer.lua", entry = root / "cases/triangle/main.lua";
  drawer::Session first(sdk, entry), second(sdk, entry);
  CHECK(first.Id() == "drawer.triangle");
  CreateTriangle(&first);
  CHECK(first.State().At("objects").Items().size() == 1 &&
        second.State().At("objects").Items().empty());
  CHECK(first.GetHistory().Count() == 1);
  auto original = first.State();
  CHECK(first.Execute("ctx:set_mode('edit')"));
  CHECK(first.Event(Event("down", 0, 0)) && first.Editing());
  for (int i = 1; i <= 30; ++i) CHECK(first.Event(Event("move", -i, -i)));
  CHECK(first.Event(Event("up", -30, -30)) && !first.Editing());
  CHECK(first.GetHistory().Count() == 2);
  auto moved = first.State();
  CHECK(moved != original);
  first.Undo();
  CHECK(first.State() == original);
  first.Redo();
  CHECK(first.State() == moved);
  CHECK(first.Execute("assert(ctx.mode=='edit')"));
  CHECK(first.Execute("ctx:set_mode('edit')"));
  CHECK(first.Event(Event("down", -30, -30)));
  CHECK(first.Event(Event("move", -50, -50)));
  CHECK(first.Event(Event("key", 0, 0, "escape")));
  CHECK(first.State() == moved && first.GetHistory().Count() == 2);
  CHECK(first.Event(Event("down", -30, -30)));
  CHECK(first.Event(Event("move", -80, -80)));
  first.Cancel();
  CHECK(!first.Editing() && first.State() == moved);
  CHECK(first.Event(Event("move", 10, 10)) && first.Event(Event("up", 10, 10)));
  CHECK(first.State() == moved);
  CHECK(first.Clear());
  CHECK(first.State().At("objects").Items().empty());
  first.Undo();
  CHECK(first.State() == moved);
  CHECK(
      !first.Execute("ctx:edit('failure',function() ctx.state.params.bad=1; "
                     "error('rollback marker') end)"));
  CHECK(first.State() == moved &&
        first.Error().find("rollback marker") != std::string::npos);
  CHECK(!first.Execute(
      "ctx:edit('cycle',function() ctx.state.loop=ctx.state end)"));
  CHECK(first.State() == moved);
  CHECK(!first.Execute(
      "ctx:edit('sparse',function() ctx.state.bad={[1]=1,[3]=3} end)"));
  CHECK(first.State() == moved);
  CHECK(!first.Execute("while true do end"));
  CHECK(first.Error().find("instruction budget") != std::string::npos);
  CHECK(first.Execute(
      "assert(ctx.math.segment_distance({x=5,y=2},{x=0,y=0},{x=10,y=0})==2)"));
  CHECK(first.Execute("assert(ctx:hit(ctx.state.objects[1],{x=50,y=20},5))"));
  for (const auto& path : drawer::DiscoverCases(root / "cases")) {
    drawer::Session session(sdk, path);
    CHECK(session.Controls({}));
    CHECK(session.Draw(1));
    CHECK(session.Execute("assert(ctx.math.distance({x=0,y=0},{x=3,y=4})==5)"));
  }
  drawer::Session parameters(sdk, entry);
  CHECK(parameters.Controls([](const std::string& operation,
                               const Value::Array& args) -> std::vector<Value> {
    if (operation == "slider") return {4, true, true};
    return {args.size() > 1 ? args[1] : Value(false), false, false};
  }));
  CHECK(parameters.State().At("params").At("width").Number() == 4);
  CHECK(parameters.GetHistory().Count() == 1);
  parameters.Undo();
  CHECK(parameters.State().At("params").At("width").Number() == 2);
  drawer::Session bezier(sdk, root / "cases/bezier/main.lua");
  for (int i = 0; i < 4; ++i)
    CHECK(bezier.Event(Event("down", i * 30, i % 2 * 50)));
  CHECK(bezier.State().At("objects").Items().size() == 1);
  CHECK(
      bezier.Execute("local p=ctx.math.bezier(ctx.state.objects[1].points,0); "
                     "assert(p.x==0 and p.y==0)"));
  auto directory = root / "build/test-output/reload";
  std::filesystem::create_directories(directory);
  auto custom = directory / "main.lua";
  std::ofstream(custom)
      << "return {id='reload',name='Reload',data_version=1,on_init=function(c) "
         "c.state.params.value=1 end}";
  drawer::Session old(sdk, custom);
  std::ofstream(custom) << "this is invalid lua!";
  Rejects([&] { drawer::Session bad(sdk, custom); });
  CHECK(old.State().At("params").At("value").Number() == 1);
  std::ofstream(custom)
      << "return {id='reload',name='Reload',data_version=1,on_init=function(c) "
         "c.state.params.value=2 end}";
  drawer::Session replacement(sdk, custom);
  CHECK(replacement.State().At("params").At("value").Number() == 2);
  CHECK(old.State().At("params").At("value").Number() == 1);
  std::ofstream(directory / "helper.lua") << "return {answer=42}";
  CHECK(old.Execute("assert(require('helper').answer==42)"));
  auto unicode = root / L"build/test-output/中文用例";
  std::filesystem::create_directories(unicode);
  std::ofstream(unicode / "main.lua")
      << "local h=require('helper'); return "
         "{id='unicode',name='Unicode',data_version=1,on_init="
         "function(c) c.state.params.answer=h.answer end}";
  std::ofstream(unicode / "helper.lua") << "return {answer=42}";
  drawer::Session unicode_session(sdk, unicode / "main.lua");
  CHECK(unicode_session.State().At("params").At("answer").Number() == 42);
  CHECK(!unicode_session.Execute("require('..outside')"));
}
void DocumentTest(const std::filesystem::path& root) {
  auto sdk = root / "sdk/drawer.lua", entry = root / "cases/triangle/main.lua";
  drawer::Session source(sdk, entry);
  CreateTriangle(&source);
  source.view = {{-123.45, 678.9}, 2.5};
  auto path = root / L"build/test-output/中文文档.drawer.json";
  std::filesystem::create_directories(path.parent_path());
  drawer::WriteDocument(path, source.SaveData());
  auto data = drawer::ReadDocument(path);
  drawer::Session restored(sdk, entry);
  restored.Restore(data);
  CHECK(restored.State() == source.State());
  CHECK(!restored.Dirty() && !restored.GetHistory().CanUndo());
  CHECK(restored.view.center.x == -123.45 && restored.view.zoom == 2.5);
  auto invalid = data;
  invalid.case_version = 99;
  Rejects([&] { restored.Restore(invalid); });
  CHECK(restored.State() == source.State());
  auto encoded = drawer::EncodeDocument(data);
  encoded["view"]["zoom"] = 0;
  Rejects([&] { drawer::DecodeDocument(encoded); });
  encoded = drawer::EncodeDocument(data);
  encoded["state"]["params"]["bad"] = Value();
  Rejects([&] { drawer::DecodeDocument(encoded); });
  encoded = drawer::EncodeDocument(data);
  encoded["format_version"] = 2;
  Rejects([&] { drawer::DecodeDocument(encoded); });
  encoded = drawer::EncodeDocument(data);
  encoded["state"]["objects"].Items().push_back(
      encoded.At("state").At("objects").Items()[0]);
  Rejects([&] { drawer::DecodeDocument(encoded); });
  auto before = drawer::ReadDocument(path).state;
  auto corrupt = data;
  corrupt.state["params"]["number"] = std::numeric_limits<double>::quiet_NaN();
  Rejects([&] { drawer::WriteDocument(path, corrupt); });
  CHECK(drawer::ReadDocument(path).state == before);
  drawer::Session gallery(sdk, root / "cases/gallery/main.lua");
  auto gallery_data = gallery.SaveData();
  drawer::Session gallery_copy(sdk, root / "cases/gallery/main.lua");
  gallery_copy.Restore(drawer::DecodeDocument(
      drawer::ParseJson(drawer::ToJson(drawer::EncodeDocument(gallery_data)))));
  CHECK(gallery_copy.State() == gallery.State() && gallery_copy.Draw(1));
}
void Benchmark(const std::filesystem::path& root) {
  drawer::Session session(root / "sdk/drawer.lua",
                          root / "cases/triangle/main.lua");
  auto begin = std::chrono::steady_clock::now();
  CHECK(session.Execute(
      "ctx:edit('1000 objects',function() for i=1,1000 do "
      "ctx:add('triangle',{{x=i,y=0},{x=i+3,y=0},{x=i,y=3}}) end end)"));
  auto created = std::chrono::steady_clock::now();
  for (int i = 0; i < 30; ++i) CHECK(session.Draw(1));
  auto drawn = std::chrono::steady_clock::now();
  CHECK(session.DrawItems().size() == 1000);
  auto encoded = drawer::ToJson(drawer::EncodeDocument(session.SaveData()));
  CHECK(drawer::DecodeDocument(drawer::ParseJson(encoded)).state ==
        session.State());
  std::cout
      << "1000 objects: create+transaction "
      << std::chrono::duration<double, std::milli>(created - begin).count()
      << " ms, Lua draw extraction avg "
      << std::chrono::duration<double, std::milli>(drawn - created).count() / 30
      << " ms, JSON " << encoded.size()
      << " bytes (not a GPU FPS measurement)\n";
}
}  // namespace
int main(int argc, char** argv) {
  try {
    CHECK(argc == 3);
    std::string suite = argv[1];
    std::filesystem::path root = argv[2];
    if (suite == "json")
      JsonTest();
    else if (suite == "history")
      HistoryTest();
    else if (suite == "view")
      ViewTest();
    else if (suite == "lua")
      LuaTest(root);
    else if (suite == "document")
      DocumentTest(root);
    else if (suite == "benchmark")
      Benchmark(root);
    else
      throw std::runtime_error("Unknown suite");
    std::cout << suite << ": PASS\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
