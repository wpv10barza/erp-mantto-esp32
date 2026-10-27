"""Regression: student Databricks endpoints and safe 3C error diagnostics."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]
config = (ROOT / "include/app_config.h").read_text(encoding="utf-8")
example = (ROOT / "include/local_config.example.h").read_text(encoding="utf-8")
firmware = (ROOT / "platformio/src/panel_4848s040/main.cpp").read_text(encoding="utf-8")
mirror = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
sync = json.loads((ROOT / "firmware/firmware-sync.json").read_text(encoding="utf-8"))

app_url = "https://asistente-cloud-erp-alumno-7474657268940105.aws.databricksapps.com"
workspace = "https://dbc-77f7771e-2520.cloud.databricks.com"
for label, body in (("defaults", config), ("local example", example)):
    assert app_url in body, f"{label}: old App default"
    assert workspace in body, f"{label}: old OAuth host"
    assert "dbc-a1aca8aa-28bd.cloud.databricks.com" not in body
    assert "asistente-cloud-erp-7474651957738908" not in body

assert 'kFirmwareVersion[] = "2.7.6"' in firmware
assert firmware == mirror, "production firmware mirrors diverged"
for needle in (
    '"CLOUD TARGET app=%s oauth_host=%s',
    'backendBaseUrl().c_str(), app_config::databricksWorkspaceUrl',
    '"Flujo: HTTP 401 OAuth/token"',
    '"Flujo: HTTP 403 permisos"',
    '"Flujo: HTTP 404 ruta"',
    '"Flujo: HTTP 5xx servidor"',
    '"Flujo: red/TLS/DNS"',
    'transport phase=%s code=%d',
    'setTransportError("POST", code, lastBackendMessage, true);',
    'setTransportError("POLL", code, body, true);',
):
    assert needle in firmware, f"missing: {needle}"
assert "transport phase=%s code=%d detail=%s" not in firmware
assert sync["ui"]["version"] == "2.7.6"
assert sync["backend"]["oauth_host"] == workspace
assert sync["backend"]["target_app"] == app_url

print("ALUMNO Databricks endpoint and 3C transport classification: PASS")
