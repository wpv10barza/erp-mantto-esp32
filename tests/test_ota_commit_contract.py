from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "platformio/src/panel_4848s040/main.cpp").read_text(encoding="utf-8")
MIRROR = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
PIO = (ROOT / "platformio.ini").read_text(encoding="utf-8")
PARTITIONS = (ROOT / "partitions_ota_16mb.csv").read_text(encoding="utf-8")
assert MAIN == MIRROR, "firmware and mirror diverged"
assert "board_build.partitions = partitions_ota_16mb.csv" in PIO
for name in ("otadata", "ota_0", "ota_1"):
    assert name in PARTITIONS, f"missing OTA partition {name}"
required = [
    'kFirmwareVersion[] = "2.7.0-ota-commit"',
    'otaCommitEditorOpen',
    'void drawOtaCommitEditor()',
    'void drawOtaCommitField()',
    '"EDITAR SHA"',
    '"BUSCAR OTA"',
    '"CONFIRMAR"',
    'otaCommitBuffer.insert',
    'otaCommitBuffer.backspace',
    'otaCommitBuffer.clear',
    'bool validCommitSha(String value)',
    'NetworkAction::CheckOtaCommit',
    'NetworkAction::InstallOta',
    '/api/device/v1/firmware/by-commit/',
    'const String fullSha = jsonStringValue(body, "source_sha");',
    'String(selected.url).startsWith("/api/device/v1/firmware/")',
    'esp_ota_get_next_update_partition',
    'Update.begin(selected.size, U_FLASH)',
    'mbedtls_sha256_update_ret',
    'Update.abort();',
    'Update.end(true)',
    'esp_ota_mark_app_valid_cancel_rollback',
    'queueNetworkRequest(NetworkAction::CheckOtaCommit',
    'queueNetworkRequest(NetworkAction::InstallOta',
]
for item in required:
    assert item in MAIN, f"OTA contract missing: {item}"
loop = MAIN[MAIN.index("void loop() {"):]
for forbidden in ("http.GET()", "installSelectedOta()", "checkOtaCommit("):
    assert forbidden not in loop, f"blocking network in UI loop: {forbidden}"
assert re.search(r'otaVersionCompare\(version.c_str\(\), kFirmwareVersion\) <= 0', MAIN)
assert 'commandBuffer.set(app_config::commandBuffer.c_str());' in MAIN
print("Commit-select OTA v2.7 contract: PASS (USB bootstrap still required)")
