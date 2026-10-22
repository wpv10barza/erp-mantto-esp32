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

# The App may be stopped even after the reviewer secret is correctly bound.
# Auto-start only STOPPED compute and reject unhealthy states before deploying.
assert "$appInfo.compute_status.state" in SCRIPT
assert "$appInfo.app_status.state" in SCRIPT
assert "if ($computeState -eq 'STOPPED')" in SCRIPT
assert "databricks apps start $Aplicacion --profile $Perfil --timeout 10m" in SCRIPT
assert "if ($appState -ne 'RUNNING' -or $computeState -ne 'ACTIVE')" in SCRIPT
assert SCRIPT.index("databricks apps start") < SCRIPT.index("databricks apps deploy")
assert "Assert-Exit 'databricks apps start'" in SCRIPT
# Fail-closed: the App must inject the SAME secret that was validated
# in the Databricks secret scope. Merely listing the resource name is not enough.
for expected in (
    "$reviewResources = @($appInfo.resources | Where-Object { $_.name -eq 'review_approval_token' })",
    "if ($reviewResources.Count -ne 1)",
    "$binding = $reviewResources[0].secret",
    "[string]$binding.scope -ne 'asistente-cloud-erp'",
    "[string]$binding.key -ne 'reviewer_token'",
    "[string]$binding.permission -ne 'READ'",
    "Vinculacion incorrecta: review_approval_token",
):
    assert expected in SCRIPT, f"Falta guardia de vinculacion App: {expected}"
assert SCRIPT.index("$binding = $reviewResources[0].secret") < SCRIPT.index("databricks secrets get-secret")
assert SCRIPT.index("[string]$binding.permission -ne 'READ'") < SCRIPT.index("databricks apps deploy")
assert "if ($null -ne $appInfo.resources)" not in SCRIPT, "No saltarse controles sin resources"

# Reject the actual Oct-21 Databricks deployment blocker BEFORE attempting deploy.
assert "databricks secrets get-secret asistente-cloud-erp reviewer_token" in SCRIPT
assert "FromBase64String" in SCRIPT
assert "$reviewBytes -contains [byte]0" in SCRIPT
assert "UTF8Encoding" in SCRIPT
assert "reviewer_token contiene NUL" in SCRIPT
assert "reviewer_token valido: UTF-8, sin NUL" in SCRIPT
assert SCRIPT.index("databricks secrets get-secret") < SCRIPT.index("databricks apps deploy")

print("Git/Databricks/ESP32 versioned update contract: PASS")
