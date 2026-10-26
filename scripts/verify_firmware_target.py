"""Fail closed if production ESP32 targets the wrong Databricks workspace.

Only reads public #define URL values. Never emits credential values.
Invoked before PlatformIO compile and again after building the OTA binary.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = {
    "ASSISTANT_BASE_URL_VALUE": "https://asistente-cloud-erp-alumno-7474657268940105.aws.databricksapps.com",
    "DATABRICKS_WORKSPACE_URL_VALUE": "https://dbc-77f7771e-2520.cloud.databricks.com",
}
LEGACY = (
    b"asistente-cloud-erp-7474651957738908.aws.databricksapps.com",
    b"dbc-a1aca8aa-28bd.cloud.databricks.com",
)


class TargetConfigurationError(ValueError):
    pass


def public_define(source: str, macro: str) -> str | None:
    matches = re.findall(
        rf'(?m)^[ \t]*#[ \t]*define[ \t]+{re.escape(macro)}[ \t]+"(https://[^"\r\n]+)"',
        source,
    )
    if len(matches) > 1:
        raise TargetConfigurationError(f"{macro}: definiciones duplicadas")
    return matches[0] if matches else None


def verify_configuration(root: Path = ROOT) -> None:
    base = (root / "include/app_config.h").read_text(encoding="utf-8-sig")
    private = root / "include/local_config.h"
    overrides = private.read_text(encoding="utf-8-sig") if private.is_file() else ""
    for macro, expected in EXPECTED.items():
        selected = public_define(overrides, macro)
        if selected is None:
            selected = public_define(base, macro)
        if selected != expected:
            # Never echo the contents of the private configuration.
            raise TargetConfigurationError(
                f"{macro} no apunta al workspace alumno-3c. "
                "Ejecute scripts/Repair-Alumno-Cloud-Local.ps1 -Apply en Windows."
            )


def verify_binary(data: bytes) -> None:
    for name, expected in EXPECTED.items():
        if expected.encode("ascii") not in data:
            raise TargetConfigurationError(
                f"Firmware compilado no contiene destino esperado para {name}"
            )
    if any(host in data for host in LEGACY):
        raise TargetConfigurationError(
            "Firmware compilado contiene referencia al workspace original"
        )


if __name__ == "__main__":
    try:
        verify_configuration()
        if len(sys.argv) == 3 and sys.argv[1] == "--binary":
            verify_binary(Path(sys.argv[2]).read_bytes())
        elif len(sys.argv) != 1:
            raise TargetConfigurationError("Uso: verify_firmware_target.py [--binary firmware.bin]")
    except (TargetConfigurationError, OSError) as error:
        print(f"[BLOQUEADO] Target alumno-3c: {error}", file=sys.stderr)
        sys.exit(2)
    print("[OK] Target de firmware: workspace ALUMNO, sin tokens expuestos")
