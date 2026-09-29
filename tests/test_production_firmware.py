from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PIO = (ROOT / "platformio.ini").read_text(encoding="utf-8")
MAIN = (ROOT / "platformio" / "src" / "panel_4848s040" / "main.cpp").read_text(encoding="utf-8")
MIRROR = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")

if MAIN != MIRROR:
    raise AssertionError("production firmware sources must be byte-identical")

panel = PIO[PIO.index("[env:panel_4848s040]"):]
for needle in ("build_type = release", "-D PANEL_PRODUCTION_BUILD=1"):
    if needle not in panel:
        raise AssertionError(f"missing production setting: {needle}")

if "#if !defined(PANEL_PRODUCTION_BUILD) || !PANEL_PRODUCTION_BUILD" not in MAIN:
    raise AssertionError("startup RGB diagnostic is not development-only")

required = 'Serial.println("DISPLAY: production build; startup RGB diagnostic disabled");'
if required not in MAIN:
    raise AssertionError("production startup path is missing")

print("Production firmware contract: PASS")
print("- panel_4848s040 is release + production macro")
print("- startup RGB diagnostic is disabled in production")
print("- mirrored firmware sources are byte-identical")
