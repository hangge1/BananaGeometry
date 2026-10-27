#include <catch2/catch_test_macros.hpp>
#include "test_support.h"

namespace drawer {
TEST_CASE("History starts at a clean save point", "[history]") {
  History history;
  history.Reset(testing::EmptyState());
  CHECK_FALSE(history.Dirty());
  CHECK_FALSE(history.CanUndo());
  CHECK_FALSE(history.Undo());
  CHECK_FALSE(history.Redo());
}
TEST_CASE("History tracks saved content through undo and redo", "[history]") {
  History history;
  history.Reset(testing::EmptyState());
  auto changed = history.Current();
  changed["params"]["size"] = 10;
  history.Push(changed, "size");
  CHECK(history.Dirty());
  history.MarkSaved();
  CHECK_FALSE(history.Dirty());
  REQUIRE(history.Undo());
  CHECK(history.Dirty());
  REQUIRE(history.Redo());
  CHECK_FALSE(history.Dirty());
  CHECK(history.UndoLabel() == "size");
}
TEST_CASE("Empty edits do not add history", "[history]") {
  History history;
  history.Reset(testing::EmptyState());
  history.Push(history.Current(), "empty");
  CHECK(history.Count() == 0);
  CHECK_FALSE(history.Dirty());
}
TEST_CASE("A new edit after undo truncates redo", "[history]") {
  History history;
  history.Reset(testing::EmptyState());
  auto changed = history.Current();
  changed["params"]["size"] = 10;
  history.Push(changed, "first");
  REQUIRE(history.Undo());
  changed["params"]["size"] = 20;
  history.Push(changed, "branch");
  CHECK_FALSE(history.CanRedo());
  CHECK(history.Current().At("params").At("size").Number() == 20);
}
TEST_CASE("History trims old snapshots without losing current data",
          "[history]") {
  History history;
  history.Reset(testing::EmptyState());
  for (int value = 0; value < 150; ++value) {
    auto changed = history.Current();
    changed["params"]["size"] = value;
    history.Push(std::move(changed), "change");
  }
  CHECK(history.Count() == 100);
  CHECK(history.Current().At("params").At("size").Number() == 149);
}
}  // namespace drawer
