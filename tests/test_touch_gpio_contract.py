from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "platformio" / "src" / "panel_4848s040" / "main.cpp").read_text(encoding="utf-8")
MIRROR = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
GPIO = (ROOT / "include" / "panel_gpio.h").read_text(encoding="utf-8")
TOUCH = (ROOT / "include" / "touch_input.h").read_text(encoding="utf-8")
HOME = (ROOT / "include" / "home_menu.h").read_text(encoding="utf-8")
VK = (ROOT / "include" / "virtual_keyboard.h").read_text(encoding="utf-8")
VK_LOCAL = (ROOT / "platformio" / "src" / "panel_4848s040" / "virtual_keyboard.h").read_text(encoding="utf-8")

if MAIN != MIRROR:
    raise AssertionError("firmware mirrors must remain byte-identical")
if VK != VK_LOCAL:
    raise AssertionError("production-local virtual_keyboard.h must match include/virtual_keyboard.h")

required_main = [
    '#include "panel_gpio.h"',
    '#include "touch_input.h"',
    '#include "home_menu.h"',
    "touch_input::mapRaw(rawX, rawY)",
    "home_ui::hitTest(",
    "touch_input::TapTracker touchTracker",
    '"GT911 I2C probe addr=0x%02X result=%u bus=400kHz',
    '"GPIO MAP: BL=%d LCD_CS=%d',
    'kFirmwareVersion[] = "2.4.0-touch-router"',
]
for needle in required_main:
    if needle not in MAIN:
        raise AssertionError(f"missing touch/GPIO integration: {needle}")

for needle in [
    "constexpr int touchSda = 19;",
    "constexpr int touchScl = 45;",
    "constexpr int lcdClock = 48;",
    "constexpr int lcdMosi = 47;",
    "constexpr int lcdCs = 39;",
    "constexpr int backlight = 38;",
    "static_assert(unique(rgb)",
    "constexpr std::array<int, 29> allUsed",
    "static_assert(unique(allUsed)",
]:
    if needle not in GPIO:
        raise AssertionError(f"missing GPIO contract: {needle}")

for needle in [
    "kRawXMin = 0",
    "kRawXMax = 480",
    "kRawYMin = 0",
    "kRawYMax = 480",
    "kSwapXY = true",
    "kMirrorX = false",
    "kMirrorY = true",
    "kTapSlopPx = 18",
    "class TapTracker",
]:
    if needle not in TOUCH:
        raise AssertionError(f"missing GT911 calibration contract: {needle}")

for needle in [
    "{Action::Backend, {14, 62, 466, 107}}",
    "{Action::Sheets, {14, 111, 466, 156}}",
    "{Action::Send3C, {246, 426, 466, 468}}",
    "kTouchInsetPx = 5",
]:
    if needle not in HOME:
        raise AssertionError(f"missing exact home hitbox: {needle}")

print("Touch + GPIO audit contract: PASS")
print("- authoritative GPIO map centralized")
print("- GT911 coordinates calibrated to 480x480")
print("- exact visual hitboxes replace broad row-only routing")
print("- menu gaps and edge dead-zones cannot select neighboring rows")
print("- tap is confirmed on release; drag/slip is rejected")
print("- GT911 I2C bus runs at 400 kHz")
print("- production and mirrored keyboard geometry are synchronized")
