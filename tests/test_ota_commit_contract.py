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
    'kInstalledCommit[] = FIRMWARE_SOURCE_COMMIT',
    'void drawOtaCommitEditor()',
    'display->print(kInstalledCommit)',
    '"EDITAR COMMIT"',
    '"COMMIT COMPLETO (40 caracteres hex)"',
    'content.substring(0, 20)',
    'content.substring(20, 40)',
    'bool validFullCommit(String value)',
    'if (value.length() != 40) return false',
    'sha.equalsIgnoreCase(kInstalledCommit)',
    'fullSha.equalsIgnoreCase(sha)',
    'otaCommitEditorOpen',
    'void drawOtaCommitEditor()',
    'void drawOtaCommitField()',

    '"BUSCAR OTA"',
    '"CONFIRMAR"',
    'otaCommitBuffer.insert',
    'otaCommitBuffer.backspace',
    'otaCommitBuffer.clear',

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
assert re.search(r'otaVersionCompare\(version.c_str\(\), kFirmwareVersion\) < 0', MAIN)
assert 'commandBuffer.set(app_config::commandBuffer.c_str());' in MAIN
print("Commit-select OTA v2.7 contract: PASS (USB bootstrap still required)")

assert 'extra_scripts = pre:scripts/inject_firmware_commit.py' in PIO
BUILD_SCRIPT = (ROOT / "scripts/inject_firmware_commit.py").read_text(encoding="utf-8")
assert 'git", "rev-parse", "HEAD"' in BUILD_SCRIPT
assert 'GITHUB_SHA' in BUILD_SCRIPT
assert 'firmware_source_commit.generated.h' in BUILD_SCRIPT
assert "This 40-character commit is EMBEDDED" in MAIN
