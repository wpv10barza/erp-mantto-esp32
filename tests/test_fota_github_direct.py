"""Direct GitHub FOTA is independent of Databricks and rejects unsigned transports."""
from pathlib import Path
import json

ROOT=Path(__file__).resolve().parents[1]
SOURCE=(ROOT/"src/main.cpp").read_text(encoding="utf-8")
MIRROR=(ROOT/"platformio/src/panel_4848s040/main.cpp").read_text(encoding="utf-8")
CA=(ROOT/"include/github_ota_ca.h").read_text(encoding="utf-8")
CI=(ROOT/".github/workflows/firmware-cd.yml").read_text(encoding="utf-8")
MANIFEST=json.loads((ROOT/"firmware/firmware-sync.json").read_text(encoding="utf-8"))

assert SOURCE==MIRROR
assert MANIFEST["ui"]["version"]=="2.7.5"
assert MANIFEST["ui"]["ota"]["independent_from_databricks"] is True
ota=SOURCE[SOURCE.index("String githubOtaAssetUrl("):SOURCE.index("void requestPcVoiceCapture()")]
for must in (
    "https://github.com/wpv10barza/erp-mantto-esp32/releases/download/",
    'githubOtaAssetUrl(sha, "manifest.json")',
    'githubOtaAssetUrl(sha, "firmware.bin")',
    "http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS)",
    "client.setCACert(kGithubOtaTrustedRoots)",
    "fullSha.length() != 40", "!fullSha.startsWith(sha)",
    "partition_table", "credential_mode", "validSha256(checksum)",
    "esp_ota_get_next_update_partition", "mbedtls_sha256_update_ret",
    "Update.begin(selected.size, U_FLASH)",
    "Update.abort()", "Update.end(true)",
    "githubOtaClockReady()",
):
    assert must in ota, must
for forbidden in ("ensureDatabricksAccessToken()", "addRequestAuth(http)",
                  "client.setInsecure(", "/api/device/v1/firmware/by-commit/"):
    assert forbidden not in ota, forbidden
assert CA.count("-----BEGIN CERTIFICATE-----") >= 3
assert "publish_github_fota:" in CI and 'gh release create "$TAG"' in CI
assert 'github.event_name == \'push\'' in CI
assert "contents: write" in CI
print("GitHub-direct verified FOTA without Databricks contract: PASS")
