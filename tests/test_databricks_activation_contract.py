from pathlib import Path
r=Path(__file__).resolve().parents[1]
s=(r/'src/main.cpp').read_text()
assert s==(r/'platformio/src/panel_4848s040/main.cpp').read_text()
assert 'if (authorizeLocalAction()) sendQueuedAction(NetworkAction::StartDatabricks);' in s
assert 'home_ui::databricksStartHit(tap.x, tap.y)' in s
assert 'case NetworkAction::StartDatabricks:' in s
assert 'appActivation.begin(appStartTransport, millis());' in s
assert 'appActivation.tick(appStartTransport, millis());' in s
assert 'appActivationBusy.exchange(true)' in s
assert 'workspace + "/api/2.0/apps/"' in s
assert 'http.POST("{}")' in s
assert 'bool healthy() { return checkBackendHealthOnce(); }' in s
for field in ('databricks_start_state','databricks_start_http','databricks_start_busy'): assert field in s
flash=(r/'scripts/Flash-ESP32-3C.ps1').read_text()
assert flash.index('--chip esp32s3') < flash.index('platformio run -e panel_4848s040 -t upload')
assert "$ChipExit -ne 0" in flash and 'Chip is ESP32-S3' in flash
assert '$Commit -ne $ExpectedCommit.ToLowerInvariant()' in flash
print('PASS: activation UI/REST/worker wiring and flash guards')
