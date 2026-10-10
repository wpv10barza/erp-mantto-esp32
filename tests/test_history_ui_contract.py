from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = (ROOT / "platformio/src/panel_4848s040/main.cpp").read_text(encoding="utf-8")
MIRROR = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
assert FIRMWARE == MIRROR, "both firmware entry points must be identical"
for needle in [
    'kFirmwareVersion[] = "2.7.0-ota-commit"',
    'drawHistoryScreen()',
    'showHistoryScreen(false)',
    'void handleHistoryTap(int x, int y)',
    'if (historyScreenOpen)',
    'queueNetworkRequest(NetworkAction::LoadHistory)',
    'case NetworkAction::LoadHistory:',
    'void fetchHistory()',
    '/api/device/v1/history/panel?device_id=',
    'addRequestAuth(http)',
    'historySheetTab',
    '"ORDEN 3C"',
    '"SHEETS"',
    '"HISTORIAL"',
    '"Cambios ejecutados"',
    'portENTER_CRITICAL(&historyMux)',
    'portEXIT_CRITICAL(&historyMux)',
]:
    assert needle in FIRMWARE, f"Missing authenticated history UI contract: {needle}"
loop = FIRMWARE[FIRMWARE.index("void loop() {"):]
assert "fetchHistory();" not in loop
assert "http.GET()" not in loop
print("ESP32 order history + applied Sheets read-only UI contract: PASS")
