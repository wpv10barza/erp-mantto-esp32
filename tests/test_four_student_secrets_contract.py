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
