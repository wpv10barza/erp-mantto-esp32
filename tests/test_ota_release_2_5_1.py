from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
main = (root / "platformio/src/panel_4848s040/main.cpp").read_text()
mirror = (root / "src/main.cpp").read_text()
script = (root / "scripts/prepare_publish_ota_2_5_1.ps1").read_text()
ignore = (root / ".gitignore").read_text()

assert main == mirror
assert 'kOtaVersion[] = "2.5.3"' in main
assert 'kFirmwareVersion[] = "2.5.3-white-async"' in main
assert "ota-release/" in ignore
for needle in (
    "include\\local_config.h",
    "DATABRICKS_CLIENT_ID_VALUE",
    "DATABRICKS_CLIENT_SECRET_VALUE",
    "ESP32_API_TOKEN_VALUE",
    "WIFI_PASSWORD_VALUE",
    'Get-FileHash',
    'git rev-parse HEAD',
    'pio',
    'firmware.bin',
    'manifest.json',
    'git_commit_sha',
    'AcknowledgeEmbeddedSecrets',
    'remoteSha',
    '2.5.1',
):
    assert needle in script, needle
assert script.index('"$directory/firmware.bin"') < script.index('"$directory/manifest.json"')
assert re.search(r'if \(-not \$Publish\)', script)
assert '"volumes", "read", $Volume' in script
assert '"volumes", "get", $Volume' not in script
assert script.index('Invoke-Checked "databricks" @("volumes", "read"') < script.index('Invoke-Checked "pio" @("run", "-e", $envName)')
print("OTA 2.5.1 provenance, build, safe publish gate and manifest contract: PASS")
