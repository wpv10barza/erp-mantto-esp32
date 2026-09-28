from pathlib import Path
import json
import subprocess

ROOT = Path(__file__).resolve().parents[1]
MANIFEST_PATH = ROOT / "firmware" / "firmware-sync.json"
manifest = json.loads(MANIFEST_PATH.read_text(encoding="utf-8"))

if manifest["authoritative_repository"] != "wpv10barza/erp-mantto-esp32":
    raise AssertionError("ERP repository must be authoritative")
if manifest["authoritative_branch"] != "main":
    raise AssertionError("main must be authoritative")

for relative, expected in manifest["git_blob_sha1"].items():
    path = ROOT / relative
    if not path.is_file():
        raise AssertionError(f"missing firmware parity file: {relative}")
    actual = subprocess.check_output(
        ["git", "hash-object", str(path)], cwd=ROOT, text=True
    ).strip()
    if actual != expected:
        raise AssertionError(
            f"firmware parity mismatch for {relative}: expected {expected}, got {actual}"
        )

print("Firmware/main sync: PASS")
print("- all deployment-critical blobs match firmware/firmware-sync.json")
