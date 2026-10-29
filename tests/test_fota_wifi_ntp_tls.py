"""FOTA must distinguish Wi-Fi failure from UTC/NTP failure before HTTPS."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]
SRC = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
MIRROR = (ROOT / "platformio/src/panel_4848s040/main.cpp").read_text(encoding="utf-8")
MANIFEST = json.loads((ROOT / "firmware/firmware-sync.json").read_text(encoding="utf-8"))
assert SRC == MIRROR
assert 'kFirmwareVersion[] = "2.7.9"' in SRC
assert MANIFEST["ui"]["version"] == "2.7.9"

start = SRC.index("void beginGithubOtaClockSync()")
end = SRC.index("void requestPcVoiceCapture()")
ota = SRC[start:end]
assert 'configTime(0, 0, "pool.ntp.org", "time.google.com", "time.cloudflare.com");' in ota
assert "githubNtpStarted = true;" in ota
assert "for (int i = 0; i < 60 && WiFi.status() == WL_CONNECTED; ++i)" in ota
assert "vTaskDelay(pdMS_TO_TICKS(500));" in ota
assert 'FOTA sin WiFi: revise red 2.4GHz' in ota
assert 'FOTA: falta WiFi (NVS/USB)' in ota
assert 'FOTA: sin UTC NTP; revise Internet/DNS' in ota
assert 'FOTA NTP ERROR:' in ota
assert ota.count("if (!githubOtaNetworkReady()) return false;") == 2
assert 'client.setCACert(kGithubOtaTrustedRoots);' in ota
assert 'mbedtls_sha256_update_ret(&sha, data, n)' in ota
assert 'Update.end(true)' in ota
assert "client.setInsecure();" not in ota
assert "ensureDatabricksAccessToken()" not in ota
assert "addRequestAuth(http)" not in ota

check = ota[ota.index("bool checkOtaCommit("):ota.index("bool installSelectedOta()")]
assert check.index("String(kInstalledCommit).startsWith(sha)") < check.index("if (!githubOtaNetworkReady())")

worker = SRC[SRC.index("void networkWorker(void*"):SRC.index("const char controlPage[]")]
assert "beginGithubOtaClockSync();  // Background UTC sync before any GitHub FOTA." in worker
assert "githubNtpStarted = false;" in worker
assert "clock_utc_ready" in SRC
assert "time(nullptr) > 1700000000" in SRC
print("GitHub FOTA Wi-Fi/NTP split, 30s UTC, TLS verified, no Databricks: PASS")
