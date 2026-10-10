from pathlib import Path

root = Path(__file__).resolve().parents[1]
main = (root / "platformio/src/panel_4848s040/main.cpp").read_text()
mirror = (root / "src/main.cpp").read_text()
home = (root / "include/home_menu.h").read_text()
script = (root / "scripts/prepare_publish_ota_2_5_3.ps1").read_text()
assert main == mirror
assert 'kOtaVersion[] = "2.5.3"' in main
for needle in (
    "Historial 3C",
    "HomePanel::History",
    "NetworkAction::HistoryFetch",
    "/api/device/v1/commands/history?device_id=",
    "historyLoaded",
    "historyChanges",
    "Sin cambios confirmados",
    "REVISAR ORDEN 3C",
    "commandPreviewOpen = true",
    "queueNetworkRequest(NetworkAction::Send3C, app_config::commandBuffer);",
    "fetchCommandHistory(0);",
):
    assert needle in main, needle
assert "Action::History" in home
assert '$version = "2.5.3"' in script
assert "local_config.h" in script
assert "firmware.bin" in script
print("3C editor preview + recent history read-only + OTA 2.5.3: PASS")
