#pragma once

#include <cstdint>

namespace touch_input {

constexpr int kScreenWidth = 480;
constexpr int kScreenHeight = 480;

// The repository's ESPHome reference for this exact panel uses GT911
// calibration 0..480 with mirror_x=false and mirror_y=false.
constexpr int kRawXMin = 0;
constexpr int kRawXMax = 480;
constexpr int kRawYMin = 0;
constexpr int kRawYMax = 480;
constexpr bool kSwapXY = false;
constexpr bool kMirrorX = false;
constexpr bool kMirrorY = false;

struct Point {
  int16_t x = 0;
  int16_t y = 0;
  bool valid = false;
};

constexpr int clampInt(int value, int low, int high) {
  return value < low ? low : (value > high ? high : value);
}

constexpr int scaleAxis(int raw, int rawMin, int rawMax, int screenSize) {
  const int clamped = clampInt(raw, rawMin, rawMax);
  const int span = rawMax - rawMin;
  return span <= 0 ? 0 : ((clamped - rawMin) * (screenSize - 1) + span / 2) / span;
}

constexpr Point mapRaw(int rawX, int rawY) {
  int x = scaleAxis(rawX, kRawXMin, kRawXMax, kScreenWidth);
  int y = scaleAxis(rawY, kRawYMin, kRawYMax, kScreenHeight);

  if (kSwapXY) {
    const int tmp = x;
    x = y;
    y = tmp;
  }
  if (kMirrorX) x = kScreenWidth - 1 - x;
  if (kMirrorY) y = kScreenHeight - 1 - y;

  return Point{static_cast<int16_t>(x), static_cast<int16_t>(y),
               x >= 0 && x < kScreenWidth && y >= 0 && y < kScreenHeight};
}

constexpr int kTapSlopPx = 18;

class TapTracker {
 public:
  bool update(bool touched, int x, int y, Point* releasedTap = nullptr) {
    if (touched) {
      const Point current{static_cast<int16_t>(x), static_cast<int16_t>(y),
                          x >= 0 && x < kScreenWidth && y >= 0 && y < kScreenHeight};
      if (!current.valid) {
        cancel();
        return false;
      }
      if (!active_) {
        active_ = true;
        moved_ = false;
        start_ = current;
        last_ = current;
        return false;
      }

      last_ = current;
      const int dx = last_.x - start_.x;
      const int dy = last_.y - start_.y;
      if (dx > kTapSlopPx || dx < -kTapSlopPx ||
          dy > kTapSlopPx || dy < -kTapSlopPx) {
        moved_ = true;
      }
      return false;
    }

    if (!active_) return false;
    const bool accepted = !moved_ && start_.valid;
    const Point acceptedPoint = start_;
    cancel();
    if (accepted && releasedTap) *releasedTap = acceptedPoint;
    return accepted;
  }

  void cancel() {
    active_ = false;
    moved_ = false;
    start_ = Point{};
    last_ = Point{};
  }

  bool active() const { return active_; }

 private:
  bool active_ = false;
  bool moved_ = false;
  Point start_{};
  Point last_{};
};

}  // namespace touch_input
