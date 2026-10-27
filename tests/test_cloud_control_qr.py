"""ESP32 touch control must initiate only an authenticated teacher workflow."""
from pathlib import Path
import json,re

root=Path(__file__).resolve().parents[1]
cpp=(root/"src/main.cpp").read_text(encoding="utf-8")
mirror=(root/"platformio/src/panel_4848s040/main.cpp").read_text(encoding="utf-8")
qr=(root/"include/cloud_control_qr.h").read_text(encoding="utf-8")
sync=json.loads((root/"firmware/firmware-sync.json").read_text(encoding="utf-8"))
assert cpp==mirror
assert 'kFirmwareVersion[] = "2.7.7"' in cpp
assert sync["ui"]["teacher_control"]["direct_device_deploy"] is False
assert sync["ui"]["teacher_control"]["requires_provisioned_oidc"] is True
assert 'bool cloudControlQrOpen = false;' in cpp
assert 'void drawCloudControlScreen()' in cpp
assert "ACTIVAR / DEPLOY CLOUD" in cpp
assert "Solo profesor: autenticacion GitHub" in cpp
assert 'homePanel == HomePanel::Backend' in cpp
assert 'tap.y >= 354 && tap.y < 402' in cpp
assert "cloudControlQrOpen = true;" in cpp
assert "drawCloudControlScreen();" in cpp
assert "kCloudControlQrBits[byteIndex]" in cpp
assert 'if (tap.x >= 246) queueNetworkRequest(NetworkAction::Health);' in cpp
assert "PROBAR CLOUD" in cpp
assert "GitHub > Run workflow > start/redeploy" in cpp
assert "ACTIVAR / DEPLOY CLOUD" in cpp
assert "credential" not in qr.lower().replace("no credentials", "")
assert "https://github.com/wpv10barza/asistente-de-databricks--erp/actions/workflows/alumno-cloud-control.yml" in qr
assert "kCloudControlQrModules = 41" in qr
assert len(re.findall(r"0x[0-9a-f]{2}", qr)) == 211
# No Databricks administrative control API or GitHub token on firmware.
for forbidden in ("/api/2.0/apps/asistente-cloud-erp-alumno/start",
                  "github_pat_", "GITHUB_TOKEN=", "databricks apps deploy"):
    assert forbidden not in cpp
print("Student 2.7.7 QR touch controls and no device-admin secrets: PASS")
