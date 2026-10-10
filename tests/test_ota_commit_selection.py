from pathlib import Path
root = Path(__file__).resolve().parents[1]
s = (root / "platformio/src/panel_4848s040/main.cpp").read_text()
for expected in (
    "BUSCAR OTA", "COMMIT", "INSTALAR",
    "editingFirmwareCommit = true", "selectedOtaCommit = commitRef",
    "NetworkAction::OtaInstall, selectedOtaCommit",
    "/api/device/v1/firmware/commits/", "resolvedSha.startsWith(commitRef)",
    "validOtaCommitRef(", 'commandBuffer.set(app_config::commandBuffer.c_str())',
):
    assert expected in s, expected
assert s == (root / "src/main.cpp").read_text()
print("OTA manual commit selection and 3C editor isolation: PASS")
