#pragma once

#include <array>
#include <cstdint>

namespace home_ui {

struct Rect {
  int16_t left;
  int16_t top;
  int16_t right;
  int16_t bottom;

  constexpr bool contains(int x, int y) const {
    return x >= left && x < right && y >= top && y < bottom;
  }

  constexpr Rect inset(int amount) const {
    return Rect{
        static_cast<int16_t>(left + amount),
        static_cast<int16_t>(top + amount),
        static_cast<int16_t>(right - amount),
        static_cast<int16_t>(bottom - amount)};
  }
};

enum class Action : uint8_t {
  None,
  Backend,
  Sheets,
  Github,
  Firmware,
  Wifi,
  Databricks,
  Diagnostics,
  Device,
  TestCloud,
  Send3C,
};

struct Target {
  Action action;
  Rect rect;
};

constexpr int kTouchInsetPx = 5;

constexpr std::array<Target, 10> targets() {
  return {{
    {Action::Backend, {14, 62, 466, 107}},
    {Action::Sheets, {14, 111, 466, 156}},
    {Action::Github, {14, 160, 466, 205}},
    {Action::Firmware, {14, 209, 466, 254}},
    {Action::Wifi, {14, 266, 466, 300}},
    {Action::Databricks, {14, 302, 466, 336}},
    {Action::Diagnostics, {14, 338, 466, 372}},
    {Action::Device, {14, 374, 466, 408}},
    {Action::TestCloud, {14, 426, 234, 468}},
    {Action::Send3C, {246, 426, 466, 468}},
  }};
}

constexpr Action hitTest(int x, int y, bool detailsExpanded) {
  for (const auto& target : targets()) {
    if (detailsExpanded &&
        (target.action == Action::Wifi ||
         target.action == Action::Databricks ||
         target.action == Action::Diagnostics ||
         target.action == Action::Device)) {
      continue;
    }
    if (target.rect.inset(kTouchInsetPx).contains(x, y)) return target.action;
  }
  return Action::None;
}

}  // namespace home_ui
