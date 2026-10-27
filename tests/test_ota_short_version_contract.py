"""Static contract: human-friendly short OTA reference without dropping full provenance."""

from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[1]
CPP = (ROOT / "platformio/src/panel_4848s040/main.cpp").read_text(encoding="utf-8")
MIRROR = (ROOT / "src/main.cpp").read_text(encoding="utf-8")
WORKFLOW = (ROOT / ".github/workflows/firmware-cd.yml").read_text(encoding="utf-8")
PACKAGE = (ROOT / "scripts/package_ota_metadata.py").read_text(encoding="utf-8")
SYNC = json.loads((ROOT / "firmware/firmware-sync.json").read_text(encoding="utf-8"))

assert CPP == MIRROR
assert 'kFirmwareVersion[] = "2.7.5"' in CPP
assert "kOtaCommitCapacity = 12;" in CPP
assert "value.length() < 8 || value.length() > 12" in CPP
assert 'display->print(String(kInstalledCommit).substring(0, 8));' in CPP
assert '"EDITAR SHA"' in CPP
assert '"SHA CORTO (8-12 caracteres hex)"' in CPP
assert "fullSha.startsWith(sha)" in CPP
assert "String(kInstalledCommit).startsWith(sha)" in CPP
assert 'githubOtaAssetUrl(sha, "manifest.json")' in CPP
assert 'githubOtaAssetUrl(sha, "firmware.bin")' in CPP
assert 'client.setCACert(kGithubOtaTrustedRoots)' in CPP
assert 'http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS)' in CPP
assert 'ensureDatabricksAccessToken()' not in CPP[CPP.index('bool checkOtaCommit('):CPP.index('void requestPcVoiceCapture()')]
assert 'esp_ota_get_next_update_partition' in CPP
assert 'mbedtls_sha256_update_ret' in CPP
assert 'Update.end(true)' in CPP
assert "40 caracteres hex" not in CPP
assert 'kInstalledCommit[] = FIRMWARE_SOURCE_COMMIT' in CPP
assert 'sha256' in PACKAGE and 'source_sha' in PACKAGE
assert "publish_github_fota:" in WORKFLOW
assert "gh release create" in WORKFLOW
assert "contents: write" in WORKFLOW
assert 'already exists. Increase kFirmwareVersion' in WORKFLOW
assert 'databricks fs cp dist/firmware.bin "$VERSION_DIR/firmware.bin"' in WORKFLOW
assert '--overwrite' not in WORKFLOW.split('databricks fs cp dist/firmware.bin', 1)[1].split("\n",1)[0]
assert 'https://dbc-77f7771e-2520.cloud.databricks.com' in WORKFLOW
assert SYNC['ui']['version'] == "2.7.5"

print("GitHub FOTA v2.7.5 + GitHub Actions safety contract: PASS")
