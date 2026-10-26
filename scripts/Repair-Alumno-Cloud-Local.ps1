[CmdletBinding()]
param(
    [switch]$Apply
)

# Only edits the two PUBLIC hostname macros in the ignored local_config.h.
# Never prints private token, password or service-principal values.
# Default is read-only. Use -Apply to create a secure local backup and fix.
Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$Config = Join-Path $Root "include\local_config.h"
$Expected = [ordered]@{
    ASSISTANT_BASE_URL_VALUE = "https://asistente-cloud-erp-alumno-7474657268940105.aws.databricksapps.com"
    DATABRICKS_WORKSPACE_URL_VALUE = "https://dbc-77f7771e-2520.cloud.databricks.com"
}

if (-not (Test-Path -LiteralPath $Config -PathType Leaf)) {
    throw "No existe include/local_config.h. No se generan credenciales ni se modifica nada."
}

# Ensure that a copy of the credential-bearing file cannot be accidentally committed.
& git -C $Root check-ignore --quiet -- "include/local_config.h"
if ($LASTEXITCODE -ne 0) {
    throw "local_config.h no esta excluido por .gitignore; se bloquea la modificacion."
}

$Before = [System.IO.File]::ReadAllText($Config)
$After = $Before
$Changed = @()
foreach ($Macro in $Expected.Keys) {
    $Pattern = '(?m)^(?<prefix>[ \t]*#[ \t]*define[ \t]+' +
        [regex]::Escape([string]$Macro) +
        '[ \t]+")(?<url>https://[^"\r\n]+)(?<suffix>"[^\r\n]*)$'
    $MatchesFound = @([regex]::Matches($After, $Pattern))
    if ($MatchesFound.Count -gt 1) {
        throw "Definicion duplicada de $Macro en la configuracion privada."
    }
    if ($MatchesFound.Count -eq 0) {
        Write-Host "[DEFAULT] $Macro no esta definido localmente; se usa app_config.h"
        continue
    }
    $Current = $MatchesFound[0].Groups["url"].Value
    if ($Current -eq $Expected[$Macro]) {
        Write-Host "[OK] $Macro usa el destino de la alumna"
        continue
    }
    Write-Host "[CORREGIR] $Macro apunta a un destino distinto del workspace alumno-3c"
    $Corrected = $MatchesFound[0].Groups["prefix"].Value +
        $Expected[$Macro] +
        $MatchesFound[0].Groups["suffix"].Value
    $After = $After.Remove($MatchesFound[0].Index, $MatchesFound[0].Length).
        Insert($MatchesFound[0].Index, $Corrected)
    $Changed += [string]$Macro
}

if ($Changed.Count -eq 0) {
    Write-Host "[OK] Ninguna URL privada necesita cambios."
    return
}
if (-not $Apply) {
    Write-Host "[SOLO LECTURA] Ejecute este script con -Apply para corregir las URLs."
    return
}

$User = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
$BackupFolder = Join-Path $env:LOCALAPPDATA "Proyecto3C-Alumno\Privado\Backups"
New-Item -Path $BackupFolder -ItemType Directory -Force | Out-Null
& icacls.exe $BackupFolder /inheritance:r /grant:r "$($User):(OI)(CI)F" | Out-Null
if ($LASTEXITCODE -ne 0) { throw "No se pudo proteger el directorio privado de respaldo." }
$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$Backup = Join-Path $BackupFolder ("local_config-{0}.h" -f $Stamp)
Copy-Item -LiteralPath $Config -Destination $Backup -ErrorAction Stop
& icacls.exe $Backup /inheritance:r /grant:r "$($User):F" | Out-Null
if ($LASTEXITCODE -ne 0) {
    Remove-Item -LiteralPath $Backup -Force -ErrorAction SilentlyContinue
    throw "No se pudo proteger la copia de seguridad privada."
}

# Atomic-ish replace: prepare next bytes in same directory, then replace.
$Temp = Join-Path (Split-Path $Config -Parent) ("local_config.tmp.{0}" -f $Stamp)
try {
    [System.IO.File]::WriteAllText(
        $Temp, $After, [System.Text.UTF8Encoding]::new($false))
    Move-Item -LiteralPath $Temp -Destination $Config -Force
} finally {
    Remove-Item -LiteralPath $Temp -Force -ErrorAction SilentlyContinue
}
Write-Host "[OK] Se corrigieron SOLO las dos URLs publicas en local_config.h."
Write-Host "[OK] Respaldo local privado protegido: $Backup"
Write-Host "[AVISO] Verifique las credenciales OAuth y token del dispositivo del WORKSPACE ALUMNO."
Write-Host "[AVISO] El ESP32 instalado sigue con su firmware anterior hasta grabar por USB/OTA."
