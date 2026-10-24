from pathlib import Path
src = (Path(__file__).resolve().parents[1] / "scripts/Completar-4-Secretos-Alumno-3C.ps1").read_text(encoding="utf-8")
for expected in (
    "alumno-3c", "polovaleriow@gmail.com", "ScopeNuevo",
    "current-user", "create-scope", "list-scopes", "list-secrets",
    "gemini_api_key", "google_service_account_json", "esp32_api_token",
    "reviewer_token", "Put-SecretByStdin", "New-SecretToken",
    "RedirectStandardInput", "UTF8Encoding", "RandomNumberGenerator",
    "ProcessStartInfo", "-AsSecureString", "ZeroFreeBSTR", "ConvertFrom-Json",
    "service_account", "No se copiaron credenciales", "$present -contains $key",
):
    assert expected in src, f"Missing contract: {expected}"
for dangerous in ("--string-value", "Set-Content", "Start-Process -FilePath 'notepad.exe'", "WriteAllText("):
    assert dangerous not in src, f"Credential persistence/arg risk: {dangerous}"
assert "get-secret" not in src, "Never export a previous credential"
print("Four student secrets protected-workflow contract: PASS")

assert 'function Invoke-DbJson {' in src
assert '& databricks @CliArgs' in src
assert src.count('Invoke-DbJson -CliArgs @(') == 4
assert 'Invoke-DbJson @(' not in src

# Empty/blank user input must fail with a useful message BEFORE Test-Path;
# otherwise Windows PowerShell throws ParameterArgumentValidationErrorEmptyStringNotAllowed.
start = src.index("if ($missing -contains 'google_service_account_json')")
blank_guard = src.index("if ([string]::IsNullOrWhiteSpace($GoogleJsonPath))", start)
blank_error = src.index("Falta google_service_account_json. No se realizaron cambios.", blank_guard)
path_check = src.index("Test-Path -LiteralPath $GoogleJsonPath -PathType Leaf", start)
assert start < blank_guard < blank_error < path_check
assert 'Los otros tres secretos existentes se conservan.' in src
assert '$GoogleJsonPath = $GoogleJsonPath.Trim().Trim' in src
assert '[IO.Path]::IsPathRooted($GoogleJsonPath)' in src
print("Empty Google service JSON path: safely rejected before Test-Path.")
