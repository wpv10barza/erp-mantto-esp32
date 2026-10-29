"""FOTA HTTP -1 should be diagnosed without disabling TLS and without indefinite retries."""
from pathlib import Path
import json

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
MIRROR = (ROOT / "platformio/src/panel_4848s040/main.cpp").read_text(encoding="utf-8")
SYNC = json.loads((ROOT / "firmware/firmware-sync.json").read_text(encoding="utf-8"))

assert SOURCE == MIRROR
assert SYNC["ui"]["version"] == "2.7.9"
assert 'kFirmwareVersion[] = "2.7.9"' in SOURCE
assert SYNC["ui"]["ota"]["independent_from_databricks"] is True

start = SOURCE.index("String githubOtaAssetUrl(")
end = SOURCE.index("void requestPcVoiceCapture()")
ota = SOURCE[start:end]

# Secure GitHub HTTPS, certificate validation and SHA-256 remain mandatory.
assert 'client.setCACert(kGithubOtaTrustedRoots);' in ota
assert "client.setHandshakeTimeout(25);" in ota
assert "http.setConnectTimeout(18000);" in ota
assert "http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS)" in ota
assert "client.setInsecure();" not in ota
assert 'http.setUserAgent("ERP-MANTTO-ESP32-FOTA/2.7.9")' in ota
assert "mbedtls_sha256_update_ret(&sha, data, n)" in ota
assert "Update.end(true)" in ota
assert "Update.abort()" in ota

# HTTP -1 is a connection/transport error, not 404 and not an invalid SHA.
assert "HTTPC_ERROR_CONNECTION_REFUSED" in ota
assert 'WiFi.hostByName("github.com", address)' in ota
assert "client.lastError(tlsDetail, sizeof(tlsDetail))" in ota
assert 'Serial.printf("FOTA HTTPS %s attempt=' in ota
assert "otaLastDnsOk.store(dnsOk ? 1 : 0)" in ota
assert "otaLastTlsError.store(tlsCode)" in ota
assert "otaLastHttpCode.store(code)" in ota
assert "githubOtaErrorForDisplay(code)" in ota
assert "FOTA: DNS github no resuelve" in ota
assert "FOTA: HTTPS/TLS error; ver health" in ota

# Retry only transient connect errors; max 2 GETs, fresh HTTPS begin.
assert "for (int attempt = 1; attempt <= 2; ++attempt)" in ota
assert "code != HTTPC_ERROR_CONNECTION_REFUSED || attempt >= 2" in ota
assert "http.end();" in ota
assert "client.stop();" in ota
assert "if (!beginGithubOtaHttp(http, client, url)) break;" in ota
assert ota.count("githubOtaGetWithRetry(http, client, ") == 2
assert 'githubOtaGetWithRetry(http, client, manifestUrl, "manifest")' in ota
assert 'githubOtaGetWithRetry(http, client, downloadUrl, "firmware")' in ota
assert "ensureDatabricksAccessToken()" not in ota
assert "addRequestAuth(http)" not in ota

# Phone/Termux can read diagnostics locally without USB serial.
for field in ("fota_last_http", "fota_tls_error", "fota_dns_github", "fota_attempt", "clock_utc_ready"):
    assert field in SOURCE

# The new diagnostics must not disclose administrative secrets.
assert "github_pat_" not in ota
assert "GITHUB_TOKEN=" not in ota

print("FOTA 2.7.9 HTTPS -1 diagnostics, bounded retry, TLS and SHA-256: PASS")
