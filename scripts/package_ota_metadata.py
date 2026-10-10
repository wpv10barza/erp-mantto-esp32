"""Package primary ESP32 HMI OTA metadata after a verified PlatformIO build.

Writes descriptors only. Does not upload to Databricks or flash hardware.
"""
from __future__ import annotations

import hashlib
import json
import os
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DIST = ROOT / "dist"
MAIN = ROOT / "platformio" / "src" / "panel_4848s040" / "main.cpp"
PATTERN = r'kFirmwareVersion\[\]\s*=\s*"([0-9]+\.[0-9]+\.[0-9]+)(?:-[^"]+)?"'
source = MAIN.read_text(encoding="utf-8")
match = re.search(PATTERN, source)
if not match:
    raise SystemExit("Cannot locate semantic firmware version")
version = match.group(1)
commit = os.environ.get("GITHUB_SHA", "").lower()
if not re.fullmatch(r"[0-9a-f]{40}", commit):
    raise SystemExit("GITHUB_SHA must be the exact 40-character source SHA")
binary = DIST / "firmware.bin"
if not binary.is_file():
    raise SystemExit("Missing compiled dist/firmware.bin")
size = binary.stat().st_size
if not (65536 <= size <= 0x400000):
    raise SystemExit(f"Invalid 4 MiB OTA slot binary size {size}")
sha = hashlib.file_digest(binary.open("rb"), "sha256").hexdigest()
manifest = {
    "version": version,
    "sha256": sha,
    "size": size,
    "channel": "stable",
    "source_sha": commit,
    "source_repository": "wpv10barza/erp-mantto-esp32",
    "hardware": "ESP32-S3-4848S040",
    "partition_table": "partitions_ota_16mb.csv",
    "credential_mode": "ota3c-nvs",
    "validation": "compiled_not_physically_flashed",
}
(DIST / "latest.json").write_text(
    json.dumps({key: manifest[key] for key in ("version", "sha256", "size", "channel")},
               indent=2) + "\n", encoding="utf-8"
)
(DIST / "manifest.json").write_text(
    json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
)
print(f"OTA metadata packaged commit={commit[:12]} version={version} size={size}")
