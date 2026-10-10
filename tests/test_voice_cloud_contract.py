from pathlib import Path
ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "platformio/src/panel_4848s040/main.cpp").read_text(encoding="utf-8")
MIRROR = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
assert MAIN == MIRROR
for needle in [
    'endpoint("/api/device/v1/cloud/verify")',
    'body.indexOf("\\\"connected\\\":true")',
    'sheetsVerifyState',
    'sheetsVerifyHttp',
    'SHEETS VERIFY -> HTTP=',
    '"/api/device/v1/voice/inbox/panel?device_id="',
    'pollVoiceDraft()',
    'openReceivedVoiceInEditor();',
    'NetworkAction::AckVoiceDraft',
    '/api/device/v1/voice/drafts/',
    'void acknowledgeVoiceDraft(const String& id)',
    'commandBuffer.set(mailbox.text)',
    'commandEditorOpen = true',
    'VoiceMailbox incomingVoice',
]:
    assert needle in MAIN, f"Missing voice/cloud firmware contract: {needle}"
ui = MAIN[MAIN.index("void openReceivedVoiceInEditor() {"):MAIN.index("int send3CCommand(")]
assert "send3CCommand(" not in ui, "Voice must not auto-submit command"
assert "Update.begin(" not in ui, "Voice must not flash firmware"
loop = MAIN[MAIN.index("void loop() {"):]
assert "http.GET()" not in loop, "No blocking network calls in UI loop"
print("Voice inbox and live Google Sheets Cloud test contract: PASS")
