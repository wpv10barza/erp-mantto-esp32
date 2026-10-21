from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = (ROOT / "scripts/Actualizar-3C-DesdeGit.ps1").read_text(encoding="utf-8")
README = (ROOT / "README_GIT_DATABRICKS_FLASH_3C.md").read_text(encoding="utf-8")

for expected in (
    "git -C $Path fetch --prune origin $Rama",
    "git -C $Path merge --ff-only",
    "git -C $Path rev-parse HEAD",
    "--git-commit $Commit",
    "apps get $Aplicacion",
    "scripts\\Flash-ESP32-3C.ps1",
    "-Port $Puerto",
    "RevisorVinculado",
    "review_approval_token",
    "ALLOW_SHEET_WRITE",
    'sheet_write_enabled',
):
    assert expected in SCRIPT, f"Falta contrato de despliegue: {expected}"

for forbidden in (
    "git reset --hard",
    "git clean -fd",
    "platformio run -t erase",
    "ESP32_API_TOKEN_VALUE",
    "DATABRICKS_CLIENT_SECRET_VALUE",
):
    assert forbidden not in SCRIPT, f"Patron prohibido en updater: {forbidden}"

assert "-Modo Todo -Puerto COM9 -RevisorVinculado" in README
assert "-Modo Sincronizar" in README
# PowerShell functions emit all unredirected stdout into the returned SHA.
# Suppress Git stdout before passing the exact SHA to --git-commit.
for command in (
    "git clone -- $Url $Path | Out-Null",
    "git -C $Path fetch --prune origin $Rama | Out-Null",
    "git -C $Path switch $Rama | Out-Null",
    'git -C $Path switch --track -c $Rama "origin/$Rama" | Out-Null',
    'git -C $Path merge --ff-only "origin/$Rama" | Out-Null',
):
    assert command in SCRIPT, f"Git stdout leaking into SHA: {command}"
assert '$sha = (& git -C $Path rev-parse HEAD).Trim()' in SCRIPT
assert 'return $sha' in SCRIPT

print("Git/Databricks/ESP32 versioned update contract: PASS")
