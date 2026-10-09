from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "platformio" / "src" / "panel_4848s040" / "main.cpp").read_text(encoding="utf-8")


def require(needle: str, label: str) -> None:
    if needle not in MAIN:
        raise AssertionError(f"missing command-editor integration contract: {label}: {needle}")


# The final app-style home screen must still enter the existing editor from ENVIAR 3C.
require('drawButton(246, 426, 220, 42, "ENVIAR 3C"', "final Enviar 3C action")
require("commandEditorOpen = true;", "editor opening")
require("keyboardMode = virtual_keyboard::KeyboardMode::Alpha;", "alpha keyboard default")
require("drawEditor();", "editor rendering")

# Runtime command text remains editable and is the exact value sent to the Device API.
require("CommandBuffer<kCommandCapacity> commandBuffer;", "runtime command buffer")
require("commandBuffer.set(app_config::commandBuffer.c_str());", "default command initialization")
require("send3CCommand(app_config::commandBuffer)", "send current edited command")
require('endpoint("/api/device/v1/commands")', "Databricks Device API command endpoint")

# The touch path is aligned with the final 480x480 layout.
require("if (sample.y >= 426)", "bottom action touch zone")
require("if (sample.x < 240)", "Probar Cloud / Enviar 3C split")
require("homePanel = HomePanel::None;", "menu reset before action")

# Editor controls remain available after the UI migration.
for needle, label in [
    ("virtual_keyboard::buildKeys", "virtual keyboard layout"),
    ("commandBuffer.insert", "insert"),
    ("commandBuffer.backspace", "backspace"),
    ("commandBuffer.forwardDelete", "forward delete"),
    ("commandBuffer.setCursor", "cursor placement"),
    ("keyboardMode == virtual_keyboard::KeyboardMode::Alpha", "ABC/123 switch"),
]:
    require(needle, label)

print("Final UI + command editor integration: PASS")
print("- ENVIAR 3C opens the original editable command workflow")
print("- final touch geometry routes bottom actions correctly")
print("- commandBuffer remains the transmitted runtime source")
print("- keyboard/cursor/edit operations remain integrated")
