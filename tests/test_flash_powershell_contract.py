from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SCRIPT = (ROOT / "scripts" / "Flash-ESP32-3C.ps1").read_text(encoding="utf-8")
GUIDE = (ROOT / "README_FLASH_POWERSHELL.md").read_text(encoding="utf-8")
IGNORE = (ROOT / ".gitignore").read_text(encoding="utf-8")

required = {
    "detectar COM existentes": "SerialPort]::GetPortNames()",
    "inspeccionar dispositivos USB": "Get-CimInstance Win32_PnPEntity",
    "evitar puertos COM ambiguos": "$Candidates.Count -eq 1",
    "validar puerto antes de flashear": "$AvailablePorts -notcontains $SelectedPort",
    "verificar commit": "git rev-parse HEAD",
    "respetar archivos locales no versionados": "git status --porcelain --untracked-files=no",
    "evitar firmware sin configuracion inadvertida": "AllowMissingConfig",
    "compilar y flashear objetivo de produccion": "py -m platformio run -e panel_4848s040 -t upload --upload-port",
    "capturar salida de PlatformIO": "Tee-Object -FilePath $LogFile -Append",
    "verificar exit code": "if ($UploadExitCode -ne 0)",
    "capturar serie opcional": "SerialSeconds",
}
for purpose, snippet in required.items():
    if snippet not in SCRIPT:
        raise AssertionError(f"Falta {purpose}: {snippet}")

for forbidden in ("reset --hard", "git clean", " -t erase", "ALLOW_SHEET_WRITE=true"):
    if forbidden in SCRIPT:
        raise AssertionError(f"Operacion no permitida en el script: {forbidden}")

if "flash-logs/" not in IGNORE.splitlines():
    raise AssertionError("Los logs locales no deben subirse a Git")
if "Flash-ESP32-3C.ps1 -ListPorts" not in GUIDE:
    raise AssertionError("La guia debe explicar el diagnostico de puertos")
if "Flash-ESP32-3C.ps1 -SerialSeconds 20" not in GUIDE:
    raise AssertionError("La guia debe explicar el flash y log serial")

print("Windows flash + local log contract: PASS")
