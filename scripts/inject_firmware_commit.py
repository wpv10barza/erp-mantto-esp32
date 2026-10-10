"""PlatformIO pre-build: embed the *actual full Git commit* in every ESP32 image.

The generated header lives in .pio/build/, never in source control.
Refuse unknown revision so UI can always identify the installed build.
"""
import os
import re
import subprocess
from pathlib import Path

Import("env")  # SCons/PlatformIO global provided to extra_scripts

project = Path(env.subst("$PROJECT_DIR"))
commit = os.environ.get("GITHUB_SHA", "").strip().lower()
if not re.fullmatch(r"[0-9a-f]{40}", commit):
    commit = subprocess.check_output(
        ["git", "rev-parse", "HEAD"], cwd=project, text=True
    ).strip().lower()
if not re.fullmatch(r"[0-9a-f]{40}", commit):
    raise RuntimeError("OTA build requires a full 40-character Git commit")

build_dir = Path(env.subst("$BUILD_DIR"))
build_dir.mkdir(parents=True, exist_ok=True)
header = build_dir / "firmware_source_commit.generated.h"
header.write_text(
    "#pragma once\n#define FIRMWARE_SOURCE_COMMIT \"" + commit + "\"\n",
    encoding="utf-8",
)
env.Append(CPPPATH=[str(build_dir)])
print("[FW] Installed source commit: " + commit)
