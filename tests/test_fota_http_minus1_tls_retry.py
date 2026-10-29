"""Regression tests for GitHub FOTA HTTPS -1 diagnostics and bounded TLS retry."""
from pathlib import Path
import base64
import json
import re

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
MIRROR = (ROOT / "platformio/src/panel_4848s040/main.cpp").read_text(encoding="utf-8")
CERTS = (ROOT / "include/github_ota_ca.h").read_text(encoding="utf-8")
SYNC = json.loads((ROOT / "firmware/firmware-sync.json").read_text(encoding="utf-8"))

assert MAIN == MIRROR
assert 'kFirmwareVersion[] = "2.7.9"' in MAIN
assert SYNC["ui"]["version"] == "2.7.9"
assert "DESACTIVAR DATABRICKS" in MAIN

start = MAIN.index("bool beginGithubOtaHttp(")
end = MAIN.index("void requestPcVoiceCapture()")
ota = MAIN[start:end]

# No access token, insecure TLS, HTTP downgrade, or Databricks running dependency.
assert "client.setCACert(kGithubOtaTrustedRoots)" in ota
assert "client.setHandshakeTimeout(30)" in ota
assert "http.setConnectTimeout(15000)" in ota
assert "http.setTimeout(30000)" in ota
assert "http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS)" in ota
assert "http.setReuse(false)" in ota
assert 'http.setUserAgent("ERP-MANTTO-ESP32-FOTA/2.7.9")' in ota
assert "client.setInsecure();" not in ota
assert "ensureDatabricksAccessToken()" not in ota
assert "addRequestAuth(http)" not in ota

# Never loop unboundedly: retry exactly once and only for transport failures.
assert "int githubOtaGetWithRetry(" in ota
assert "for (int attempt = 1; attempt <= 2; ++attempt)" in ota
assert "if (result >= 0) return result;" in ota
assert "http.end();" in ota and "client.stop();" in ota
assert "vTaskDelay(pdMS_TO_TICKS(1000))" in ota
assert "const int tlsCode = client.lastError(tlsDetail, sizeof(tlsDetail));" in ota
assert 'WiFi.hostByName("github.com", githubIp)' in ota
assert "http.errorToString(result).c_str()" in ota
assert "dnsReady ? \"OK\" : \"FAIL\"" in ota
assert 'githubOtaGetWithRetry(http, client, manifestUrl, "MANIFEST")' in ota
assert 'githubOtaGetWithRetry(http, client, downloadUrl, "FIRMWARE")' in ota
assert 'code < 0 ? "GitHub DNS/TCP/TLS; ver USB"' in ota
assert 'code < 0 ? "FOTA DNS/TCP/TLS; ver USB"' in ota

# Existing release authentication and update rollback checks stay intact.
for token in (
    "fullSha.length() != 40",
    "!fullSha.startsWith(sha)",
    "validSha256(checksum)",
    "mbedtls_sha256_update_ret",
    "Update.abort()",
    "Update.end(true)",
    "esp_ota_get_next_update_partition",
    "githubOtaClockReady()",
):
    assert token in MAIN, token

# The PEM file has four full DER structures, not partial/truncated certs.
pems = re.findall(
    r"-----BEGIN CERTIFICATE-----([\s\S]*?)-----END CERTIFICATE-----", CERTS
)
assert len(pems) == 4
for pem in pems:
    der = base64.b64decode("".join(pem.split()), validate=True)
    assert der[0] == 0x30  # DER SEQUENCE
    count = der[1] & 0x7F if der[1] & 0x80 else 0
    length = int.from_bytes(der[2:2 + count], "big") if count else der[1]
    assert len(der) == 2 + count + length

print("FOTA 2.7.9 HTTPS -1 verified retry, TLS/DNS diagnostics and trust roots: PASS")
