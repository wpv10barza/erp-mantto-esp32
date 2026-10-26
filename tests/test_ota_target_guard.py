"""Production firmware must never accidentally compile or release for the old workspace."""
from pathlib import Path
from tempfile import TemporaryDirectory
import sys

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from verify_firmware_target import (
    EXPECTED, LEGACY, TargetConfigurationError,
    verify_configuration, verify_binary,
)

def fixture(root: Path, override: str = "") -> None:
    (root / "include").mkdir(exist_ok=True)
    defaults = "\n".join(
        f'#define {name} "{value}"' for name, value in EXPECTED.items()
    )
    (root / "include/app_config.h").write_text(defaults, encoding="utf-8")
    if override:
        (root / "include/local_config.h").write_text(override, encoding="utf-8")

with TemporaryDirectory() as tmp:
    root = Path(tmp)
    fixture(root)
    verify_configuration(root)
    url_name = "ASSISTANT_BASE_URL_VALUE"
    old_url = LEGACY[0].decode("ascii")
    override = f'#define {url_name} "{old_url}"\n'
    fixture(root, override)
    try:
        verify_configuration(root)
    except TargetConfigurationError as exc:
        assert url_name in str(exc)
        assert old_url not in str(exc), "private overrides should not appear in errors"
    else:
        raise AssertionError("Original workspace URL was accepted in local_config.h")
    fixture(root, "#define ASSISTANT_BASE_URL_VALUE \"" +
            EXPECTED[url_name] + "\"\n")
    verify_configuration(root)
    # Duplicated keys are rejected, rather than resolving a non-deterministic value.
    fixture(root, override + override)
    try:
        verify_configuration(root)
    except TargetConfigurationError as exc:
        assert "duplicadas" in str(exc)
    else:
        raise AssertionError("Duplicate private macro must be rejected")

good_binary = b"PREFIX\n" + b"\n".join(v.encode("ascii") for v in EXPECTED.values())
verify_binary(good_binary)
for bad in [b"unsafe", good_binary + LEGACY[0], good_binary + LEGACY[1]]:
    try:
        verify_binary(bad)
    except TargetConfigurationError:
        pass
    else:
        raise AssertionError("Bad binary target accepted")

pio = (ROOT / "scripts/inject_firmware_commit.py").read_text(encoding="utf-8")
pack = (ROOT / "scripts/package_ota_metadata.py").read_text(encoding="utf-8")
assert "scripts/verify_firmware_target.py" in pio
assert "verify_binary(binary_bytes)" in pack

ps = (ROOT / "scripts/Repair-Alumno-Cloud-Local.ps1").read_text(encoding="utf-8")
for token in ("[switch]$Apply", "local_config.h", "check-ignore --quiet",
              "Proyecto3C-Alumno", "icacls.exe", "Copy-Item", "[System.IO.File]::WriteAllText"):
    assert token in ps
assert "local_config.tmp" not in ps
assert "DATABRICKS_CLIENT_SECRET_VALUE" not in ps
assert "ESP32_API_TOKEN_VALUE" not in ps

print("Target-locked ESP32 firmware + OTA artifact + private PowerShell repair: PASS")
