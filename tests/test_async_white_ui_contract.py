from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "platformio/src/panel_4848s040/main.cpp").read_text(encoding="utf-8")
for expected in (
    '#include <freertos/queue.h>',
    'xTaskCreatePinnedToCore(networkWorker',
    'xQueueSend(networkQueue, &request, 0)',
    'xQueueOverwrite(uiQueue, &notice)',
    'void processNetworkUiUpdates()',
    'queueNetworkRequest(NetworkAction::CloudAndSheets)',
    'queueNetworkRequest(NetworkAction::Send3C, app_config::commandBuffer)',
    'constexpr unsigned long kMissingReleaseMs = 80',
    'display->fillScreen(WHITE)',
    'display->setTextColor(BLACK)',
    'kFirmwareVersion[] = "2.7.7"',
):
    assert expected in MAIN, f"missing async/white UI invariant: {expected}"
loop = MAIN[MAIN.index("void loop() {"):]
for forbidden in ("checkBackendHealth();", "pollCommandStatus();", "send3CCommand(", "checkCloudStack();", "ensureDatabricksAccessToken"):
    assert forbidden not in loop, f"UI loop still has blocking code: {forbidden}"
renderer = MAIN[MAIN.index("void drawButton("):MAIN.index("void playTone(")]
assert "setTextColor(WHITE)" not in renderer
# Color is permitted ONLY in the independent 18px editor status strip.
status_colors = renderer[renderer.index("uint16_t editorFlowColor("):
                         renderer.index("void drawEditorToolbar()")]
assert "color565(" in status_colors
assert "color565(" not in renderer.replace(status_colors, "")
print("Async Databricks worker and white/black touchscreen: PASS")
