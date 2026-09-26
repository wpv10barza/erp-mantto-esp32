from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "platformio" / "src" / "panel_4848s040" / "main.cpp").read_text(encoding="utf-8")


def require(text: str, needle: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing required panel-state contract: {needle}")


# Every backend transport failure must be promoted to a visible ERROR state.
require(MAIN, 'updatePanel(PanelState::Error, message, true);')
require(MAIN, 'setTransportError("POST", code, lastBackendMessage, true);')
require(MAIN, 'setTransportError("POLL", code, body, true);')
require(MAIN, 'Serial.printf("[ERROR] health HTTP=%d detail=%s\\n", code, lastBackendMessage.c_str());')

# POST 200/202 is only valid when the backend actually returned a command_id.
require(MAIN, 'if (!lastCommandId.length()) {')
require(MAIN, 'setProtocolError("POST", "falta command_id");')
require(MAIN, 'CONFIRMACIÓN REQUERIDA EN WEB')

# Polling must classify all documented failure spellings case-insensitively.
require(MAIN, 'String normalizedStatus(String status)')
for status in ["error", "failed", "fallido"]:
    require(MAIN, f'status == "{status}"')
require(MAIN, 'setProtocolError("POLL", result.length() ? result : "Error reportado por WSL");')

# Unknown or missing backend status must not silently remain PENDING.
require(MAIN, 'else {\n    setProtocolError("POLL", status.length() ? String("estado desconocido \'") + status + "\'" : "falta status");')

# Success/rejection are terminal and visible; pending is the only polling state that remains active.
require(MAIN, 'updatePanel(PanelState::Applied, result.length() ? result : "Confirmado en WSL", true);')
require(MAIN, 'updatePanel(PanelState::Rejected, result.length() ? result : "Rechazado en WSL", true);')
require(MAIN, 'updatePanel(PanelState::Pending, "CONFIRMACIÓN REQUERIDA EN WEB");')

print("Panel state contract: PASS")
print("- Health HTTP failures -> visible ERROR")
print("- POST transport/protocol failures -> visible ERROR")
print("- Poll HTTP failures -> visible ERROR")
print("- error/failed/fallido -> visible ERROR")
print("- unknown/missing status -> visible ERROR")
print("- applied/rejected/pending transitions remain explicit")
