#include <unity.h>
#include <cstddef>

#include "virtual_keyboard.h"

namespace {

using virtual_keyboard::Key;
using virtual_keyboard::KeyKind;
using virtual_keyboard::KeyboardMode;
using virtual_keyboard::KeyRect;

void assertNoOverlaps(KeyboardMode mode) {
  Key keys[50]{};
  const size_t count = virtual_keyboard::buildKeys(mode, keys, 50);
  for (size_t i = 0; i < count; ++i) {
    TEST_ASSERT_TRUE(keys[i].rect.left < keys[i].rect.right);
    TEST_ASSERT_TRUE(keys[i].rect.top < keys[i].rect.bottom);
    TEST_ASSERT_TRUE(keys[i].rect.left >= 0);
    TEST_ASSERT_TRUE(keys[i].rect.top >= 0);
    TEST_ASSERT_TRUE(keys[i].rect.right <= virtual_keyboard::kScreenWidth);
    TEST_ASSERT_TRUE(keys[i].rect.bottom <= virtual_keyboard::kScreenHeight);
    for (size_t j = i + 1; j < count; ++j) {
      TEST_ASSERT_FALSE(virtual_keyboard::rectanglesOverlap(keys[i].rect, keys[j].rect));
    }
  }
}

void assertHorizontalBoundaryIsExclusive(KeyboardMode mode) {
  Key keys[50]{};
  const size_t count = virtual_keyboard::buildKeys(mode, keys, 50);
  TEST_ASSERT_TRUE(count >= 2);

  const Key& first = keys[0];
  const Key& second = keys[1];
  TEST_ASSERT_EQUAL_INT(first.rect.bottom, second.rect.bottom);
  TEST_ASSERT_EQUAL_INT(first.rect.top, second.rect.top);
  TEST_ASSERT_TRUE(first.rect.right < second.rect.left);

  const int y = first.rect.top + virtual_keyboard::kHitInsetPx;
  TEST_ASSERT_EQUAL_INT(-1, virtual_keyboard::hitTestIndex(mode, first.rect.left, y));
  TEST_ASSERT_EQUAL_INT(-1, virtual_keyboard::hitTestIndex(mode, first.rect.right - 1, y));
  TEST_ASSERT_EQUAL_INT(0, virtual_keyboard::hitTestIndex(
      mode, first.rect.left + virtual_keyboard::kHitInsetPx, y));
  TEST_ASSERT_EQUAL_INT(-1, virtual_keyboard::hitTestIndex(mode, first.rect.right, y));
  TEST_ASSERT_EQUAL_INT(-1, virtual_keyboard::hitTestIndex(mode, second.rect.left - 1, y));
  TEST_ASSERT_EQUAL_INT(-1, virtual_keyboard::hitTestIndex(mode, second.rect.left, y));
  TEST_ASSERT_EQUAL_INT(1, virtual_keyboard::hitTestIndex(
      mode, second.rect.left + virtual_keyboard::kHitInsetPx, y));
}

void assertVerticalBoundaryIsExclusive(KeyboardMode mode) {
  Key keys[50]{};
  const size_t count = virtual_keyboard::buildKeys(mode, keys, 50);
  TEST_ASSERT_TRUE(count > 10);

  const Key& row0 = keys[0];
  const Key& row1 = keys[10];
  TEST_ASSERT_EQUAL_INT(row0.rect.left, row1.rect.left);
  TEST_ASSERT_TRUE(row0.rect.bottom < row1.rect.top);

  const int x = row0.rect.left + virtual_keyboard::kHitInsetPx;
  TEST_ASSERT_EQUAL_INT(-1, virtual_keyboard::hitTestIndex(mode, x, row0.rect.bottom - 1));
  TEST_ASSERT_EQUAL_INT(0, virtual_keyboard::hitTestIndex(
      mode, x, row0.rect.bottom - 1 - virtual_keyboard::kHitInsetPx));
  TEST_ASSERT_EQUAL_INT(-1, virtual_keyboard::hitTestIndex(mode, x, row0.rect.bottom));
  TEST_ASSERT_EQUAL_INT(-1, virtual_keyboard::hitTestIndex(mode, x, row1.rect.top - 1));
  TEST_ASSERT_EQUAL_INT(-1, virtual_keyboard::hitTestIndex(mode, x, row1.rect.top));
  TEST_ASSERT_EQUAL_INT(10, virtual_keyboard::hitTestIndex(
      mode, x, row1.rect.top + virtual_keyboard::kHitInsetPx));
}

void assertOutsideKeyboardIsRejected(KeyboardMode mode) {
  TEST_ASSERT_EQUAL_INT(-1, virtual_keyboard::hitTestIndex(mode, -1, -1));
  TEST_ASSERT_EQUAL_INT(-1, virtual_keyboard::hitTestIndex(
      mode, virtual_keyboard::kScreenWidth, 300));
  TEST_ASSERT_EQUAL_INT(-1, virtual_keyboard::hitTestIndex(
      mode, 100, virtual_keyboard::kScreenHeight));
  TEST_ASSERT_FALSE(virtual_keyboard::hitTest(mode, -1, 300));

  Key matched{};
  TEST_ASSERT_FALSE(virtual_keyboard::hitTest(mode, -1, 300, &matched));
}

void assertControlKeysHit(KeyboardMode mode) {
  Key keys[50]{};
  const size_t count = virtual_keyboard::buildKeys(mode, keys, 50);
  TEST_ASSERT_TRUE(count >= 33);

  bool foundToggle = false;
  bool foundSpace = false;
  bool foundEnter = false;

  for (size_t i = 0; i < count; ++i) {
    const Key& key = keys[i];
    const int x = (static_cast<int>(key.rect.left) + key.rect.right) / 2;
    const int y = (static_cast<int>(key.rect.top) + key.rect.bottom) / 2;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(i),
                          virtual_keyboard::hitTestIndex(mode, x, y));

    if (key.definition.kind == KeyKind::ToggleAlphaNumeric) foundToggle = true;
    if (key.definition.kind == KeyKind::Space) foundSpace = true;
    if (key.definition.kind == KeyKind::Enter) foundEnter = true;
  }

  TEST_ASSERT_TRUE(foundToggle);
  TEST_ASSERT_TRUE(foundSpace);
  TEST_ASSERT_TRUE(foundEnter);
}

void test_key_rect_is_half_open() {
  constexpr KeyRect rect{10, 20, 30, 40};
  static_assert(rect.contains(10, 20));
  static_assert(rect.contains(29, 39));
  static_assert(!rect.contains(30, 20));
  static_assert(!rect.contains(10, 40));

  TEST_ASSERT_TRUE(rect.contains(10, 20));
  TEST_ASSERT_TRUE(rect.contains(29, 39));
  TEST_ASSERT_FALSE(rect.contains(30, 20));
  TEST_ASSERT_FALSE(rect.contains(10, 40));
}

void test_alpha_keyboard_has_no_overlaps() { assertNoOverlaps(KeyboardMode::Alpha); }
void test_numeric_keyboard_has_no_overlaps() { assertNoOverlaps(KeyboardMode::NumericSymbols); }
void test_alpha_horizontal_boundaries_are_exclusive() {
  assertHorizontalBoundaryIsExclusive(KeyboardMode::Alpha);
}
void test_numeric_horizontal_boundaries_are_exclusive() {
  assertHorizontalBoundaryIsExclusive(KeyboardMode::NumericSymbols);
}
void test_alpha_vertical_boundaries_are_exclusive() {
  assertVerticalBoundaryIsExclusive(KeyboardMode::Alpha);
}
void test_numeric_vertical_boundaries_are_exclusive() {
  assertVerticalBoundaryIsExclusive(KeyboardMode::NumericSymbols);
}
void test_alpha_outside_points_are_rejected() {
  assertOutsideKeyboardIsRejected(KeyboardMode::Alpha);
}
void test_numeric_outside_points_are_rejected() {
  assertOutsideKeyboardIsRejected(KeyboardMode::NumericSymbols);
}
void test_alpha_control_keys_are_hit_testable() {
  assertControlKeysHit(KeyboardMode::Alpha);
}
void test_numeric_control_keys_are_hit_testable() {
  assertControlKeysHit(KeyboardMode::NumericSymbols);
}

}  // namespace

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_key_rect_is_half_open);
  RUN_TEST(test_alpha_keyboard_has_no_overlaps);
  RUN_TEST(test_numeric_keyboard_has_no_overlaps);
  RUN_TEST(test_alpha_horizontal_boundaries_are_exclusive);
  RUN_TEST(test_numeric_horizontal_boundaries_are_exclusive);
  RUN_TEST(test_alpha_vertical_boundaries_are_exclusive);
  RUN_TEST(test_numeric_vertical_boundaries_are_exclusive);
  RUN_TEST(test_alpha_outside_points_are_rejected);
  RUN_TEST(test_numeric_outside_points_are_rejected);
  RUN_TEST(test_alpha_control_keys_are_hit_testable);
  RUN_TEST(test_numeric_control_keys_are_hit_testable);
  return UNITY_END();
}
