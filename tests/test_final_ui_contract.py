from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "platformio" / "src" / "panel_4848s040" / "main.cpp").read_text(encoding="utf-8")
MIRROR = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
EDITOR = (ROOT / "include" / "editor_components.h").read_text(encoding="utf-8")


def require_main(needle: str, label: str) -> None:
    if needle not in MAIN:
        raise AssertionError(f"missing final UI contract: {label}: {needle}")


if MAIN != MIRROR:
    raise AssertionError("final UI must be byte-identical in both firmware mirrors")

if '"INICIO"' not in EDITOR:
    raise AssertionError('missing final UI contract: editor return-to-main control: "INICIO"')

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
    ("drawEditorTextField()", "partial editor redraw"),
    ("checkCloudStack()", "cloud plus Sheets verifier"),
    ('endpoint("/api/sheet/verify")', "read-only Sheets verification"),
    ('kFirmwareVersion[] = "2.5.1-white-async"', "final firmware identity"),
]:
    require_main(needle, label)

for forbidden in ["12345678", 'WIFI_PASSWORD_VALUE "NOKIA', "Clave:"]:
    if forbidden in MAIN:
        raise AssertionError(f"forbidden credential/UI disclosure: {forbidden}")

for required in [
    "commandEditorOpen",
    "send3CCommand",
    "pending_confirmation",
    "CONFIRMACIÓN REQUERIDA EN WEB",
]:
    require_main(required, "existing 3C workflow")

print("Final principal UI contract: PASS")
print("- compact app-style home menu")
print("- modular editor with INICIO return control")
print("- partial editor redraw and isolated keyboard geometry")
print("- Wi-Fi password is not embedded or displayed")
print("- Probar Cloud validates Databricks + Sheets")
print("- Enviar 3C keeps the existing editor and review flow")
