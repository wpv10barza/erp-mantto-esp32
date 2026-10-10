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
    'kFirmwareVersion[] = "2.5.1-white-async"',
):
    assert expected in MAIN, f"missing async/white UI invariant: {expected}"
loop = MAIN[MAIN.index("void loop() {"):]
for forbidden in ("checkBackendHealth();", "pollCommandStatus();", "send3CCommand(", "checkCloudStack();", "ensureDatabricksAccessToken"):
    assert forbidden not in loop, f"UI loop still has blocking code: {forbidden}"
renderer = MAIN[MAIN.index("void drawButton("):MAIN.index("void playTone(")]
assert "setTextColor(WHITE)" not in renderer
assert "color565(" not in renderer
print("Async Databricks worker and white/black touchscreen: PASS")
