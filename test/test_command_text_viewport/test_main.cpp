#include <unity.h>

// Explicit Unity lifecycle hooks for native toolchains without weak defaults.
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}
#include <cstddef>
#include <cstdint>

#include "command_text_viewport.h"

using command_text_viewport::Window;

namespace {

constexpr uint16_t kWidths[] = {0, 8, 18, 26, 36, 44, 54, 64, 72, 84, 96, 106, 118};
constexpr int kViewport = 40;
constexpr int kCursorWidth = 2;

void test_initial_cursor_does_not_scroll() {
  Window view = command_text_viewport::compute(kWidths, 12, 0, kViewport, kCursorWidth);
  TEST_ASSERT_EQUAL_UINT(0, view.first);
  TEST_ASSERT_EQUAL_INT(0, view.cursorX);
  TEST_ASSERT_TRUE(view.textWidth <= kViewport - kCursorWidth);
}

void test_middle_cursor_scrolls_horizontally() {
  Window view = command_text_viewport::compute(kWidths, 12, 5, kViewport, kCursorWidth);
  TEST_ASSERT_TRUE(view.first > 0);
  TEST_ASSERT_TRUE(view.first <= 5);
  TEST_ASSERT_TRUE(view.cursorX >= 0);
  TEST_ASSERT_TRUE(view.cursorX <= kViewport - kCursorWidth);
  TEST_ASSERT_TRUE(kWidths[view.last] - kWidths[view.first] <=
                   kViewport - kCursorWidth);
}

void test_end_cursor_keeps_insertion_point_visible() {
  Window view = command_text_viewport::compute(kWidths, 12, 12, kViewport, kCursorWidth);
  TEST_ASSERT_EQUAL_UINT(12, view.last);
  TEST_ASSERT_TRUE(view.first > 0);
  TEST_ASSERT_TRUE(view.cursorX <= kViewport - kCursorWidth);
  TEST_ASSERT_TRUE(view.cursorX >= 0);
}

void test_short_text_does_not_scroll() {
  Window view = command_text_viewport::compute(kWidths, 4, 4, 100, kCursorWidth);
  TEST_ASSERT_EQUAL_UINT(0, view.first);
  TEST_ASSERT_EQUAL_UINT(4, view.last);
  TEST_ASSERT_EQUAL_INT(kWidths[4], view.cursorX);
}

void test_out_of_range_lengths_are_clamped() {
  Window view =
      command_text_viewport::compute(kWidths, 999, 999, kViewport, kCursorWidth);
  TEST_ASSERT_TRUE(view.last <= 12);
  TEST_ASSERT_TRUE(view.first <= view.last);
  TEST_ASSERT_TRUE(view.cursorX <= kViewport - kCursorWidth);
}

void test_degenerate_viewport_returns_empty_window() {
  Window view = command_text_viewport::compute(kWidths, 12, 4, 2, kCursorWidth);
  TEST_ASSERT_EQUAL_UINT(0, view.first);
  TEST_ASSERT_EQUAL_UINT(0, view.last);
  TEST_ASSERT_EQUAL_INT(0, view.cursorX);
}

}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_initial_cursor_does_not_scroll);
  RUN_TEST(test_middle_cursor_scrolls_horizontally);
  RUN_TEST(test_end_cursor_keeps_insertion_point_visible);
  RUN_TEST(test_short_text_does_not_scroll);
  RUN_TEST(test_out_of_range_lengths_are_clamped);
  RUN_TEST(test_degenerate_viewport_returns_empty_window);
  return UNITY_END();
}
