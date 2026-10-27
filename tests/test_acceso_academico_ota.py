"""Firmware academic browser access must not bypass Databricks SSO or trigger compute."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]
FW = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
MIRROR = (ROOT / "platformio/src/panel_4848s040/main.cpp").read_text(encoding="utf-8")
SYNC = json.loads((ROOT / "firmware/firmware-sync.json").read_text(encoding="utf-8"))
PORTAL = "https://asistente-cloud-erp-alumno-7474657268940105.aws.databricksapps.com/acceso"
CONTROL = "https://github.com/wpv10barza/asistente-de-databricks--erp/actions/workflows/alumno-cloud-control.yml"

assert FW == MIRROR
assert 'kFirmwareVersion[] = "2.7.4"' in FW
assert SYNC["ui"]["version"] == "2.7.4"
assert SYNC["ui"]["portal"]["portal"] == PORTAL
assert 'web.on("/acceso", HTTP_GET' in FW
assert 'String(app_config::assistantBaseUrl) + "/acceso"' in FW
assert 'web.send(302, "text/plain; charset=utf-8", "Redireccionando a Databricks SSO")' in FW
assert 'href="/acceso" rel="noopener noreferrer"' in FW
assert CONTROL in FW
assert "PROBAR CLOUD verifica; no enciende la App" in FW
assert "Si OFF: profesor START desde GitHub Actions" in FW
assert "Cloud alumno: CONECTADO" in FW

# The device should never impersonate either person or control cloud compute.
for secret_or_identity in (
    "polovaleriow@gmail.com", "wpvbar11@gmail.com", "password_alumno",
    "databricks apps start", "databricks apps stop",
):
    assert secret_or_identity not in FW

print("Student SSO portal / OTA 2.7.4 contract: PASS")
