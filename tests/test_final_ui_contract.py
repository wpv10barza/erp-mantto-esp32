from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "platformio" / "src" / "panel_4848s040" / "main.cpp").read_text(encoding="utf-8")
MIRROR = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")\nEDITOR = (ROOT / "include" / "editor_components.h").read_text(encoding="utf-8")


def require(needle: str, label: str) -> None:
    if needle not in MAIN:
        raise AssertionError(f"missing final UI contract: {label}: {needle}")


if MAIN != MIRROR:
    raise AssertionError("final UI must be byte-identical in both firmware mirrors")

for needle, label in [
    ("enum class HomePanel", "accordion state"),
    ('"Interfaz Portatil"', "main title"),
    ('"Opciones del sistema"', "main subtitle"),
    ('"Conexion al backend"', "backend menu"),
    ('"Google Sheets"', "Sheets menu"),
    ('"GitHub Actions"', "Actions menu"),
    ('"Actualizar firmware"', "firmware menu"),
    ('"Wi-Fi 2.4 GHz"', "Wi-Fi accordion"),
    ('"Nube Databricks"', "Databricks accordion"),
    ('"Diagnostico"', "diagnostic accordion"),
    ('"Estado del dispositivo"', "device accordion"),
    ('"PROBAR CLOUD"', "cloud action"),
    ('"ENVIAR 3C"', "3C action"),
    ("drawExpandedPanel()", "expandable detail renderer"),
    ('"INICIO"', "editor return-to-main control"),
    ("drawEditorTextField()", "partial editor redraw"),
    ("checkCloudStack()", "cloud plus Sheets verifier"),
    ('endpoint("/api/sheet/verify")', "read-only Sheets verification"),
    ('kFirmwareVersion[] = "2.2.0-editor-ui"', "final firmware identity"),
]:
    require(needle, label)

# The final home screen must never embed or display a Wi-Fi password.
for forbidden in ["12345678", "WIFI_PASSWORD_VALUE \"NOKIA", "Clave:"]:
    if forbidden in MAIN:
        raise AssertionError(f"forbidden credential/UI disclosure: {forbidden}")

# Keep the existing command editor and human-confirmation workflow.
for required in [
    "commandEditorOpen",
    "send3CCommand",
    "pending_confirmation",
    "CONFIRMACIÓN REQUERIDA EN WEB",
]:
    require(required, "existing 3C workflow")

print("Final principal UI contract: PASS")
print("- compact app-style home menu")
print("- expandable technical sections")
print("- backend, Sheets, GitHub Actions and firmware options")
print("- Wi-Fi password is not embedded or displayed")
print("- Probar Cloud validates Databricks + Sheets")
print("- Enviar 3C keeps the existing editor and review flow")
