from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
script=(ROOT/"scripts/Compartir-Proyecto-3C.ps1").read_text(encoding="utf-8")
readme=(ROOT/"README_COMPARTIR_PROYECTO_3C.md").read_text(encoding="utf-8")
for phrase in (
  "polovaleriow@gmail.com",
  "CAN_USE","CAN_MANAGE",
  "databricks users list",
  "databricks users create",
  "databricks apps get-permissions",
  "databricks apps update-permissions",
  "if (-not $Aplicar)",
  "databricks apps get-permissions",
  "gh api --method PUT",
  "erp-mantto-esp32",
  "asistente-de-databricks--erp",
  "voice-embeding-in-databricks-for-esp32",
):
    assert phrase in script, f"Missing audited sharing contract: {phrase}"
assert "databricks apps set-permissions" not in script
assert "databricks account users delete" not in script
assert "github.com/wpv10barza/" not in script or "gh api --method PUT" in script
assert "GitHubUsuario" in script
assert "Restringido" in readme
assert "copia en otro account" in readme
print("Safe GitHub Databricks sharing contract: PASS")
