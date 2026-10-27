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
    'kFirmwareVersion[] = "2.7.5"',
    'kInstalledCommit[] = FIRMWARE_SOURCE_COMMIT',
    'void drawOtaCommitEditor()',
    'String(kInstalledCommit).substring(0, 8)',
    '"EDITAR SHA"',
    '"SHA CORTO (8-12 caracteres hex)"',
    'content.substring(0, 12)',
    'String(content.length()) + "/12   ENTER = buscar OTA"',
    'bool validShortCommit(String value)',
    'if (value.length() < 8 || value.length() > 12) return false',
    'String(kInstalledCommit).startsWith(sha)',
    'fullSha.startsWith(sha)',
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
    'githubOtaAssetUrl(sha, "manifest.json")',
    'githubOtaAssetUrl(sha, "firmware.bin")',
    'const String fullSha = jsonStringValue(body, "source_sha");',
    'downloadUrl.startsWith("https://github.com/wpv10barza/erp-mantto-esp32/releases/download/ota-")',
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
print("GitHub FOTA v2.7.5 contract: PASS (USB bootstrap still required)")

assert 'extra_scripts = pre:scripts/inject_firmware_commit.py' in PIO
BUILD_SCRIPT = (ROOT / "scripts/inject_firmware_commit.py").read_text(encoding="utf-8")
assert 'git", "rev-parse", "HEAD"' in BUILD_SCRIPT
assert 'GITHUB_SHA' in BUILD_SCRIPT
assert 'firmware_source_commit.generated.h' in BUILD_SCRIPT
assert "Full source SHA stays embedded" in MAIN
assert "constexpr size_t kOtaCommitCapacity = 12;" in MAIN
assert "display->print(kInstalledCommit);" not in MAIN
assert "COMMIT COMPLETO (40 caracteres hex)" not in MAIN
