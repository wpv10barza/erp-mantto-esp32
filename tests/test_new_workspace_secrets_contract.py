from pathlib import Path
script = (Path(__file__).resolve().parents[1] / "scripts" / "Crear-Secrets-Alumno-3C.ps1").read_text(encoding="utf-8")
for expected in (
    "alumno-3c", "polovaleriow@gmail.com", "New-RandomHexToken",
    "New-Object byte[] 24", "notepad.txt", "databricks current-user me",
    "databricks secrets list-scopes", "databricks secrets create-scope",
    "databricks secrets list-secrets", "databricks secrets put-secret",
    "esp32_api_token", "reviewer_token", "gemini_api_key",
    "--json \"@$jsonPath\"", "icacls.exe", "Start-Process",
    "No se reemplazo", "rotacion accidental",
):
    assert expected in script, f"Missing student secret safety contract: {expected}"
assert "--string-value $esp32Token" not in script
assert "--string-value $reviewToken" not in script
assert "Get-Content" not in script
assert "gemini_api_key' $esp32Token" not in script
print("New workspace student secret generator contract: PASS")
