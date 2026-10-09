#include <unity.h>

// Explicit Unity lifecycle hooks for native toolchains without weak defaults.
extern "C" void setUp(void) {}
extern "C" void tearDown(void) {}

#include "home_menu.h"
#include "panel_gpio.h"
#include "touch_input.h"

using home_ui::Action;

void test_touch_calibration_corners_and_center() {
  const auto a = touch_input::mapRaw(0, 0);
  TEST_ASSERT_TRUE(a.valid);
  TEST_ASSERT_EQUAL_INT(0, a.x);
  TEST_ASSERT_EQUAL_INT(0, a.y);

  const auto b = touch_input::mapRaw(480, 480);
  TEST_ASSERT_TRUE(b.valid);
  TEST_ASSERT_EQUAL_INT(479, b.x);
  TEST_ASSERT_EQUAL_INT(479, b.y);

  const auto c = touch_input::mapRaw(240, 240);
  TEST_ASSERT_TRUE(c.valid);
  TEST_ASSERT_INT_WITHIN(1, 240, c.x);
  TEST_ASSERT_INT_WITHIN(1, 240, c.y);
}

void test_home_menu_gaps_do_not_trigger_adjacent_rows() {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::Backend),
                        static_cast<int>(home_ui::hitTest(100, 80, false)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::None),
                        static_cast<int>(home_ui::hitTest(100, 109, false)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::Sheets),
                        static_cast<int>(home_ui::hitTest(100, 120, false)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::None),
                        static_cast<int>(home_ui::hitTest(5, 120, false)));
}

void test_home_menu_bottom_buttons_match_visual_bounds() {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::TestCloud),
                        static_cast<int>(home_ui::hitTest(20, 440, false)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::Send3C),
                        static_cast<int>(home_ui::hitTest(300, 440, false)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::None),
                        static_cast<int>(home_ui::hitTest(240, 440, false)));
}

void test_expanded_details_disable_hidden_secondary_rows() {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::None),
                        static_cast<int>(home_ui::hitTest(100, 280, true)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::Backend),
                        static_cast<int>(home_ui::hitTest(100, 80, true)));
}

void test_tap_tracker_fires_only_on_release() {
  touch_input::TapTracker tracker;
  touch_input::Point tap{};
  TEST_ASSERT_FALSE(tracker.update(true, 100, 120, &tap));
  TEST_ASSERT_FALSE(tracker.update(true, 104, 123, &tap));
  TEST_ASSERT_TRUE(tracker.update(false, 0, 0, &tap));
  TEST_ASSERT_EQUAL_INT(100, tap.x);
  TEST_ASSERT_EQUAL_INT(120, tap.y);
}

void test_drag_across_neighbor_is_rejected() {
  touch_input::TapTracker tracker;
  touch_input::Point tap{};
  TEST_ASSERT_FALSE(tracker.update(true, 100, 100, &tap));
  TEST_ASSERT_FALSE(tracker.update(true, 100, 130, &tap));
  TEST_ASSERT_FALSE(tracker.update(false, 0, 0, &tap));
}

void test_hitbox_edges_are_dead_zones() {
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::None),
                        static_cast<int>(home_ui::hitTest(15, 80, false)));
  TEST_ASSERT_EQUAL_INT(static_cast<int>(Action::Backend),
                        static_cast<int>(home_ui::hitTest(100, 80, false)));
}

void test_critical_gpio_map() {
  TEST_ASSERT_EQUAL_INT(38, panel_gpio::backlight);
  TEST_ASSERT_EQUAL_INT(39, panel_gpio::lcdCs);
  TEST_ASSERT_EQUAL_INT(48, panel_gpio::lcdClock);
  TEST_ASSERT_EQUAL_INT(47, panel_gpio::lcdMosi);
  TEST_ASSERT_EQUAL_INT(19, panel_gpio::touchSda);
  TEST_ASSERT_EQUAL_INT(45, panel_gpio::touchScl);
  TEST_ASSERT_EQUAL_INT(18, panel_gpio::de);
  TEST_ASSERT_EQUAL_INT(17, panel_gpio::vsync);
  TEST_ASSERT_EQUAL_INT(16, panel_gpio::hsync);
  TEST_ASSERT_EQUAL_INT(21, panel_gpio::pclk);
  TEST_ASSERT_TRUE(panel_gpio::unique(panel_gpio::rgb));
  TEST_ASSERT_TRUE(panel_gpio::unique(panel_gpio::allUsed));
}

int main() {
  UNITY_BEGIN();
  RUN_TEST(test_touch_calibration_corners_and_center);
  RUN_TEST(test_home_menu_gaps_do_not_trigger_adjacent_rows);
  RUN_TEST(test_home_menu_bottom_buttons_match_visual_bounds);
  RUN_TEST(test_expanded_details_disable_hidden_secondary_rows);
  RUN_TEST(test_tap_tracker_fires_only_on_release);
  RUN_TEST(test_drag_across_neighbor_is_rejected);
  RUN_TEST(test_hitbox_edges_are_dead_zones);
  RUN_TEST(test_critical_gpio_map);
  return UNITY_END();
}
