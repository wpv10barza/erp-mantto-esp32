#include <unity.h>
#include <cstddef>

#include "editor_components.h"
#include "virtual_keyboard.h"

using editor_ui::ToolbarAction;
using virtual_keyboard::KeyboardMode;

void test_keyboard_is_horizontally_centered() {
  const int left = virtual_keyboard::KeyboardLayout::left();
  const int right = left + virtual_keyboard::KeyboardLayout::width();
  TEST_ASSERT_INT_WITHIN(1, left, virtual_keyboard::kScreenWidth - right);
}

void test_keyboard_modes_are_vertically_centered_in_their_area() {
  const KeyboardMode modes[] = {KeyboardMode::Alpha, KeyboardMode::NumericSymbols};
  for (auto mode : modes) {
    const int topGap = virtual_keyboard::KeyboardLayout::top(mode) -
                       virtual_keyboard::kKeyboardAreaTop;
    const int bottomGap = virtual_keyboard::kKeyboardAreaBottom -
                          virtual_keyboard::KeyboardLayout::bottom(mode);
    TEST_ASSERT_INT_WITHIN(1, topGap, bottomGap);
  }
}

void test_toolbar_buttons_do_not_overlap_and_fit_screen() {
  const auto buttons = editor_ui::ToolbarComponent::buttons();
  for (size_t i = 0; i < buttons.size(); ++i) {
    TEST_ASSERT_TRUE(buttons[i].rect.left >= 0);
    TEST_ASSERT_TRUE(buttons[i].rect.right <= editor_ui::EditorLayout::kScreenWidth);
    TEST_ASSERT_TRUE(buttons[i].rect.top >= 0);
    TEST_ASSERT_TRUE(buttons[i].rect.bottom <= editor_ui::EditorLayout::kScreenHeight);
    for (size_t j = i + 1; j < buttons.size(); ++j) {
      const bool overlap =
          buttons[i].rect.left < buttons[j].rect.right &&
          buttons[j].rect.left < buttons[i].rect.right &&
          buttons[i].rect.top < buttons[j].rect.bottom &&
          buttons[j].rect.top < buttons[i].rect.bottom;
      TEST_ASSERT_FALSE(overlap);
    }
  }
}

void test_toolbar_actions_are_independently_hit_testable() {
  const auto buttons = editor_ui::ToolbarComponent::buttons();
  for (const auto& button : buttons) {
    const int x = (button.rect.left + button.rect.right) / 2;
    const int y = (button.rect.top + button.rect.bottom) / 2;
    TEST_ASSERT_EQUAL_INT(static_cast<int>(button.action),
                          static_cast<int>(editor_ui::ToolbarComponent::hitTest(x, y)));
  }
  TEST_ASSERT_EQUAL_INT(static_cast<int>(ToolbarAction::None),
                        static_cast<int>(editor_ui::ToolbarComponent::hitTest(0, 0)));
}

void test_keyboard_stays_below_toolbar() {
  const auto toolbar = editor_ui::EditorLayout::toolbar();
  TEST_ASSERT_TRUE(toolbar.bottom < virtual_keyboard::KeyboardLayout::top(KeyboardMode::Alpha));
  TEST_ASSERT_TRUE(toolbar.bottom < virtual_keyboard::KeyboardLayout::top(KeyboardMode::NumericSymbols));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_keyboard_is_horizontally_centered);
  RUN_TEST(test_keyboard_modes_are_vertically_centered_in_their_area);
  RUN_TEST(test_toolbar_buttons_do_not_overlap_and_fit_screen);
  RUN_TEST(test_toolbar_actions_are_independently_hit_testable);
  RUN_TEST(test_keyboard_stays_below_toolbar);
  return UNITY_END();
}
