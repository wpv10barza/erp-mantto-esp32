from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
MIRROR = (ROOT / "platformio" / "src" / "panel_4848s040" / "main.cpp").read_text(encoding="utf-8")
CONFIG = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")

assert MAIN == MIRROR, "Production firmware sources must remain identical"
for text in (
    'status == "pending_review"',
    'status == "approved_not_applied"',
    'status == "applied"',
    'status == "rejected"',
    'if (code == 404)',
    '"EXPIRADO: REENVIAR"',
    'status != lastObservedCommandStatus',
    'commandSentAtMs < 30000UL',
    'app_config::commandIdlePollMs',
):
    assert text in MAIN, f"Missing flow requirement: {text}"
assert "commandPollMs = 2500UL" in CONFIG
assert "commandIdlePollMs = 10000UL" in CONFIG
print("3C command flow state + polling contract: PASS")
