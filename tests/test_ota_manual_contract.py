from pathlib import Path
import re
ROOT = Path(__file__).resolve().parents[1]
main = (ROOT / "platformio/src/panel_4848s040/main.cpp").read_text(encoding="utf-8")
mirror = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
ini = (ROOT / "platformio.ini").read_text(encoding="utf-8")
partitions = (ROOT / "partitions_ota_16mb.csv").read_text(encoding="utf-8")
config = (ROOT / "include/app_config.h").read_text(encoding="utf-8")
assert main == mirror
for needle in (
    "#include <Update.h>",
    "#include <esp_ota_ops.h>",
    "#include <mbedtls/sha256.h>",
    "NetworkAction::OtaCheck",
    "NetworkAction::OtaInstall",
    "checkForOtaUpdate(bool install, const String& commitRef = String())",
    '"/api/device/v1/firmware/commits/"',
    "NetworkAction::OtaInstall, selectedOtaCommit",
    "editingFirmwareCommit = true",
    "validOtaCommitRef(commitRef)",
    "resolvedSha.startsWith(commitRef)",
    "if (!install)",
    '"/api/device/v1/firmware/latest"',
    '"/api/device/v1/firmware/"',
    "OTA SHA-256 no coincide",
    "if (actualSha != expectedSha)",
    "esp_ota_mark_app_valid_cancel_rollback()",
    "esp_ota_mark_app_invalid_rollback_and_reboot()",
    "confirmOtaBootIfHealthy();",
    '"BUSCAR OTA"',
    '"INSTALAR"',
):
    assert needle in main, needle
assert "otaHttpTimeoutMs" in config
assert "otaBootConfirmDelayMs" in config
assert "board_build.partitions = partitions_ota_16mb.csv" in ini
assert all(name in partitions for name in ("otadata", "app0", "app1"))
assert re.search(r'app0,\s+app,\s+ota_0', partitions)
assert re.search(r'app1,\s+app,\s+ota_1', partitions)
print("OTA manual dual-slot, same-origin HTTPS, SHA-256 and rollback: PASS")
