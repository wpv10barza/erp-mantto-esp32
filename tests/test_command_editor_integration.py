from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
APP = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")

def require(text, needle):
    assert needle in text, f"missing: {needle}"

require(APP, "static String commandBuffer = DEFAULT_3C_COMMAND_VALUE;")
require(MAIN, '#include "command_buffer.h"')
require(MAIN, '#include "command_text_viewport.h"')
require(MAIN, '#include "virtual_keyboard.h"')
require(MAIN, "CommandBuffer<kCommandCapacity> commandBuffer;")
require(MAIN, "commandBuffer.set(app_config::commandBuffer.c_str());")
require(MAIN, "app_config::commandBuffer = commandBuffer.c_str();")
require(MAIN, "send3CCommand(app_config::commandBuffer);")
require(MAIN, "virtual_keyboard::hitTest")
require(MAIN, "command_text_viewport::compute")
assert "send3CCommand(app_config::defaultCommand);" not in MAIN

print("runtime command editor regression: PASS")
