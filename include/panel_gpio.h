#pragma once

#include <array>
#include <cstddef>

namespace panel_gpio {

// Authoritative Guition ESP32-S3-4848S040 pin map.
// ESPHome src/main.yaml parity: BL38, SPI 48/47 + CS39, I2C 19/45,
// RGB timing 18/17/16/21 and the 16 data lines below.
constexpr int backlight = 38;
constexpr int lcdCs = 39;
constexpr int lcdClock = 48;
constexpr int lcdMosi = 47;
constexpr int touchSda = 19;
constexpr int touchScl = 45;

constexpr int de = 18;
constexpr int vsync = 17;
constexpr int hsync = 16;
constexpr int pclk = 21;

constexpr std::array<int, 5> red = {{11, 12, 13, 14, 0}};
constexpr std::array<int, 6> green = {{8, 20, 3, 46, 9, 10}};
constexpr std::array<int, 5> blue = {{4, 5, 6, 7, 15}};

// On-board I2S/L1-L3 lines. They are not part of touch or display buses.
constexpr int audioBclk = 1;
constexpr int audioLrclk = 2;
constexpr int audioData = 40;

template <size_t N>
constexpr bool uniqueFrom(const std::array<int, N>& pins, size_t i, size_t j) {
  return i >= N
      ? true
      : (j >= N
          ? uniqueFrom(pins, i + 1, i + 2)
          : (pins[i] != pins[j] && uniqueFrom(pins, i, j + 1)));
}

template <size_t N>
constexpr bool unique(const std::array<int, N>& pins) {
  return N < 2 ? true : uniqueFrom(pins, 0, 1);
}

constexpr std::array<int, 16> rgb = {{
    11, 12, 13, 14, 0,
    8, 20, 3, 46, 9, 10,
    4, 5, 6, 7, 15
}};

// Complete GPIO ownership map used by the production PlatformIO target.
// A duplicate here means two firmware subsystems are trying to own one line.
// Uniqueness is verified by the native GPIO contract test because the Arduino
// toolchain for this target still parses parts of the build with C++11 constexpr rules.
constexpr std::array<int, 29> allUsed = {{
    backlight, lcdCs, lcdClock, lcdMosi,
    touchSda, touchScl,
    de, vsync, hsync, pclk,
    11, 12, 13, 14, 0,
    8, 20, 3, 46, 9, 10,
    4, 5, 6, 7, 15,
    audioBclk, audioLrclk, audioData
}};

static_assert(unique(rgb), "RGB data pins must be unique");
static_assert(unique(allUsed), "No two production subsystems may own the same GPIO");
static_assert(touchSda != touchScl, "GT911 SDA/SCL must be distinct");
static_assert(lcdClock != lcdMosi && lcdClock != lcdCs && lcdMosi != lcdCs,
              "LCD control SPI pins must be distinct");
static_assert(backlight != lcdCs && backlight != touchSda && backlight != touchScl,
              "Backlight must not overlap LCD CS or GT911 I2C");
static_assert(de != vsync && de != hsync && de != pclk &&
              vsync != hsync && vsync != pclk && hsync != pclk,
              "RGB timing pins must be distinct");

}  // namespace panel_gpio
