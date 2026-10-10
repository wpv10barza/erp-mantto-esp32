from pathlib import Path
root = Path(__file__).resolve().parents[1]
source = (root / "platformio/src/panel_4848s040/main.cpp").read_text(encoding="utf-8")
mirror = (root / "src/main.cpp").read_text(encoding="utf-8")
assert source == mirror
for needle in (
    'bool hasSummary = false;',
    'sheetsWriteMode',
    'sheet_verify_state',
    'sheet_verify_http',
    'homePanel == HomePanel::Sheets',
    'queueNetworkRequest(NetworkAction::CloudAndSheets)',
    'queueNetworkRequest(NetworkAction::LoadHistory)',
    'loaded.persistent = fields[4] == "PERSIST"',
    'APR:%d RECH:%d PEND:%d INT:%d OK:%d BLO:%d',
    'AUD: SESION | HISTORIAL',
    'SOLO SESION ACTUAL',
    'Sin registros conservados',
    'REVISION / SHEETS',
    'WiFi.localIP().toString()',
    'checkCloudStack();\n      fetchHistory();',
):
    assert needle in source, needle
assert source.count('sheetVerifyState = 3') >= 3
assert source.count('sheetsWriteMode = 0;') >= 2
print("Sheets audit, live connection, and monitor history contract: PASS")
