#include <catch2/catch_test_macros.hpp>

#include "support/gallery_fixture.h"

namespace drawer {
TEST_CASE_METHOD(
    testing::GalleryFixture,
    "Text properties use font size and keep continuous edits atomic",
    "[text][inspector]") {
  session.SetTextMeasurer([](const std::string& text, double requested) {
    const auto size = requested > 0 ? requested : 17;
    return text.empty() ? Point{} : Point{size * 6, size};
  });
  REQUIRE(session.SelectObject(6));
  const auto before = session.State();
  for (int frame = 0; frame < 3; ++frame) {
    const int size = 24 + frame * 8;
    bool saw_size = false, saw_width = false;
    REQUIRE(session.ObjectControls(
        [&](const std::string& op, const Value::Array& args) {
          saw_width |= args[0].StringOr("") == "线宽";
          if (op == "integer" && args[0].StringOr("") == "字号(px)") {
            saw_size = true;
            if (frame == 0) CHECK(args[1].Number() == 17);
            return std::vector<Value>{size, true, frame == 2};
          }
          return std::vector<Value>{args.size() > 1 ? args[1] : Value(), false,
                                    false};
        }));
    CHECK(saw_size);
    CHECK_FALSE(saw_width);
    REQUIRE(session.Draw(1));
    bool rendered = false;
    for (const auto& item : session.DrawItems())
      if (item.At("kind") == Value("text") &&
          item.At("text") == Value("二维图形 SDK")) {
        rendered = true;
        CHECK(item.At("font_size").Number() == size);
      }
    REQUIRE(rendered);
    const auto bounds = session.ObjectBounds(6);
    CHECK(bounds.Items()[1].At("x").Number() == 100 + size * 6);
    CHECK(bounds.Items()[1].At("y").Number() == -65 - size);
    CHECK(session.Editing() == (frame != 2));
  }
  CHECK(session.GetHistory().Count() == 1);
  const auto document = session.SaveData();
  Session restored(testing::SourceRoot() / "sdk/drawer.lua",
                   testing::SourceRoot() / "cases/gallery/main.lua");
  restored.Restore(DecodeDocument(ParseJson(ToJson(EncodeDocument(document)))));
  CHECK(restored.State() == session.State());
  session.Undo();
  CHECK(session.State() == before);
  session.Redo();
  CHECK(session.State().At("objects").Items()[5].At("font_size").Number() ==
        40);
}

TEST_CASE_METHOD(
    testing::GalleryFixture,
    "Text font sizes round trip while legacy text inherits its font",
    "[text][document]") {
  auto document = session.SaveData();
  auto& text = document.state["objects"].Items()[5];
  text["width"] = 32;  // Legacy, unused stroke metadata remains harmless.
  CHECK(DecodeDocument(EncodeDocument(document))
            .state.At("objects")
            .Items()[5]
            .At("font_size")
            .IsNull());
  for (double size : {6., 24.5, 256.}) {
    text["font_size"] = size;
    CHECK(DecodeDocument(EncodeDocument(document)).state == document.state);
  }
  for (const auto& size :
       {Value(0), Value(5), Value(257), Value("large"), Value(true)}) {
    text["font_size"] = size;
    CHECK_THROWS(EncodeDocument(document));
  }
}
}  // namespace drawer
