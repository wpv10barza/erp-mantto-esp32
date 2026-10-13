from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FIRMWARE = (ROOT / "platformio/src/panel_4848s040/main.cpp").read_text(encoding="utf-8")
MIRROR = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
assert FIRMWARE == MIRROR, "Firmware copies diverge"
assert "kVoicePollIntervalMs = 9000UL" in FIRMWARE
assert "kVoicePollActiveIntervalMs = 2500UL" in FIRMWARE
assert "voicePcRequested.load()" in FIRMWARE
assert "voiceInterval = voicePcRequested.load()" in FIRMWARE
assert 'endpoint("/api/device/v1/voice/inbox/panel?device_id="' in FIRMWARE
worker = FIRMWARE[FIRMWARE.index("void networkWorker("):FIRMWARE.index("const char controlPage[]")]
assert "pollVoiceDraft();" in worker
loop = FIRMWARE[FIRMWARE.index("void loop() {"):]
assert "http.GET()" not in loop, "Blocking poll in UI task"
assert "voicePcRequested = false;" in FIRMWARE, "Return to idle after dictation"
print("Adaptive voice polling: contract PASS (2.5 s active / 9 s idle)")
