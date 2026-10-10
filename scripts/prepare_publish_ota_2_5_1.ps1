#requires -Version 5.1
<#
.SYNOPSIS
  Build an ESP32-S3-4848S040 OTA 2.5.1 image locally and (optionally)
  publish a SHA-256-verified release to a Databricks Unity Catalog Volume.
.DESCRIPTION
  The ignored include/local_config.h is required. Its credentials are
  compiled into the binary: NEVER share it as a public GitHub artifact.
  Default is build-only. Publishing requires explicit opt-in and acknowledgment.
  The script does NOT access COM9 or trigger an OTA install on the ESP32.
.EXAMPLE
  .\scripts\prepare_publish_ota_2_5_1.ps1
.EXAMPLE
  .\scripts\prepare_publish_ota_2_5_1.ps1 -Publish -AcknowledgeEmbeddedSecrets
#>
[CmdletBinding()]
param(
    [string]$Profile = "asistente-cloud-erp",
    [string]$Volume = "workspace.default.esp32_firmware",
    [switch]$Publish,
    [switch]$AcknowledgeEmbeddedSecrets,
    [switch]$PromoteLatest
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"
$version = "2.5.1"
$envName = "panel_4848s040"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

function Invoke-Checked {
    param([string]$Executable, [string[]]$Arguments)
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$Executable falló (exit $LASTEXITCODE)."
    }
}

function Write-JsonUtf8NoBom {
    param([string]$Path, [object]$Data)
    $json = ($Data | ConvertTo-Json -Depth 8) + [Environment]::NewLine
    [System.IO.File]::WriteAllText(
        $Path, $json, ([System.Text.UTF8Encoding]::new($false))
    )
}

if ($PromoteLatest -and -not $Publish) {
    throw "-PromoteLatest requiere -Publish."
}
if ($Publish -and -not $AcknowledgeEmbeddedSecrets) {
    throw "La imagen contiene credenciales compiladas. Use -AcknowledgeEmbeddedSecrets solo si el UC Volume tiene ACL restringidas."
}
if ($Volume -notmatch '^[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+\.[A-Za-z0-9_-]+$') {
    throw "Nombre de volumen no válido. Ejemplo: workspace.default.esp32_firmware"
}

Push-Location $root
try {
    $configPath = Join-Path $root "include\local_config.h"
    if (-not (Test-Path -LiteralPath $configPath -PathType Leaf)) {
        throw "Falta include/local_config.h. No se puede generar una OTA que conserve Databricks."
    }

    $secretConfig = [System.IO.File]::ReadAllText($configPath)
    $requiredKeys = @(
        "WIFI_SSID_VALUE", "WIFI_PASSWORD_VALUE",
        "DATABRICKS_CLIENT_ID_VALUE", "DATABRICKS_CLIENT_SECRET_VALUE",
        "ESP32_API_TOKEN_VALUE"
    )
    foreach ($key in $requiredKeys) {
        $pattern = '(?m)^\s*#define\s+' + [regex]::Escape($key) + '\s+"([^"]+)"'
        $match = [regex]::Match($secretConfig, $pattern)
        if (-not $match.Success -or $match.Groups[1].Value -match '^(YOUR_|CHANGE_ME|REPLACE_|PLACEHOLDER)') {
            throw "Falta un valor real de $key en local_config.h. No se publica firmware incompleto."
        }
    }
    Remove-Variable secretConfig

    $status = & git status --porcelain --untracked-files=no
    if ($LASTEXITCODE -ne 0 -or $status) {
        throw "Hay archivos versionados modificados. Confirme o guarde los cambios antes de compilar para obtener un commit exacto."
    }
    $commitSha = (& git rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $commitSha -notmatch '^[0-9a-fA-F]{40}$') {
        throw "No se pudo determinar el SHA completo del commit compilado."
    }

    $sourceA = Join-Path $root "platformio\src\panel_4848s040\main.cpp"
    $sourceB = Join-Path $root "src\main.cpp"
    $sourceText = [System.IO.File]::ReadAllText($sourceA)
    if ($sourceText -notmatch 'kOtaVersion\[\]\s*=\s*"2\.5\.1"') {
        throw "El código fuente no declara OTA 2.5.1."
    }
    if ((Get-FileHash $sourceA -Algorithm SHA256).Hash -ne
        (Get-FileHash $sourceB -Algorithm SHA256).Hash) {
        throw "Las copias del firmware principal no coinciden."
    }

    if (-not (Get-Command pio -ErrorAction SilentlyContinue)) {
        throw "Instale PlatformIO CLI (pio) antes de compilar."
    }
    Invoke-Checked "pio" @("run", "-e", $envName)

    $binary = Join-Path $root ".pio\build\$envName\firmware.bin"
    if (-not (Test-Path -LiteralPath $binary -PathType Leaf)) {
        throw "No existe firmware.bin después de compilar."
    }
    $size = (Get-Item -LiteralPath $binary).Length
    if ($size -lt 100000 -or $size -gt 0x400000) {
        throw "Tamaño de imagen OTA fuera de rango: $size bytes."
    }
    $file = [System.IO.File]::OpenRead($binary)
    try {
        if ($file.ReadByte() -ne 0xE9) {
            throw "El archivo no parece ser una imagen ESP32 válida (magic 0xE9)."
        }
    } finally {
        $file.Dispose()
    }
    $sha256 = (Get-FileHash -LiteralPath $binary -Algorithm SHA256).Hash.ToLowerInvariant()

    $release = Join-Path $root "ota-release\$version"
    New-Item -ItemType Directory -Force -Path $release | Out-Null
    $releaseBinary = Join-Path $release "firmware.bin"
    $releaseManifest = Join-Path $release "manifest.json"
    Copy-Item -LiteralPath $binary -Destination $releaseBinary -Force

    $manifest = [ordered]@{
        version        = $version
        git_commit_sha = $commitSha.ToLowerInvariant()
        sha256         = $sha256
        size           = [int64]$size
        channel        = "stable"
        board          = "ESP32-S3-4848S040"
        partition_csv  = "partitions_ota_16mb.csv"
    }
    Write-JsonUtf8NoBom $releaseManifest $manifest
    if ((Get-FileHash $releaseBinary -Algorithm SHA256).Hash.ToLowerInvariant() -ne $sha256) {
        throw "El SHA-256 de la imagen preparada no coincide."
    }

    Write-Host "Imagen OTA compilada localmente: $version"
    Write-Host "Commit completo: $commitSha"
    Write-Host "SHA-256 binario: $sha256"
    Write-Host "Tamaño: $size bytes"
    Write-Host "Carpeta privada: $release"
    Write-Warning "La imagen incluye las credenciales privadas de local_config.h; no la suba a GitHub."

    if (-not $Publish) {
        Write-Host "No se ha publicado ni instalado firmware. Para publicar revise los permisos de UC Volume y use -Publish -AcknowledgeEmbeddedSecrets."
        return
    }
    if (-not (Get-Command databricks -ErrorAction SilentlyContinue)) {
        throw "Falta Databricks CLI."
    }

    # Do not overwrite an existing published release.
    Invoke-Checked "databricks" @("volumes", "get", $Volume, "--profile", $Profile, "--output", "json")
    $dbfs = "dbfs:/Volumes/" + ($Volume -replace '\.', '/')
    $directory = "$dbfs/stable/$version"
    Invoke-Checked "databricks" @("fs", "mkdir", $directory, "--profile", $Profile)
    Invoke-Checked "databricks" @("fs", "cp", $releaseBinary,
        "$directory/firmware.bin", "--profile", $Profile)

    # Independent download-and-hash check before making manifest discoverable.
    $verificationFile = Join-Path ([System.IO.Path]::GetTempPath()) ("ota_verify_" + [guid]::NewGuid().ToString("N") + ".bin")
    try {
        Invoke-Checked "databricks" @("fs", "cp", "$directory/firmware.bin",
            $verificationFile, "--profile", $Profile)
        $remoteSha = (Get-FileHash $verificationFile -Algorithm SHA256).Hash.ToLowerInvariant()
        if ($remoteSha -ne $sha256) {
            throw "Verificación remota SHA-256 falló. No se publica manifest.json."
        }
    } finally {
        Remove-Item -LiteralPath $verificationFile -ErrorAction SilentlyContinue
    }

    # The manifest is last: backend sees only complete, verified versions.
    Invoke-Checked "databricks" @("fs", "cp", $releaseManifest,
        "$directory/manifest.json", "--profile", $Profile)
    Write-Host "Release $version publicada y SHA-256 remoto verificado: $commitSha"

    if ($PromoteLatest) {
        $latestManifest = Join-Path $release "latest.json"
        # Latest carries the same exact version, digest and size.
        Write-JsonUtf8NoBom $latestManifest $manifest
        Invoke-Checked "databricks" @("fs", "cp", $latestManifest,
            "$dbfs/stable/latest.json", "--profile", $Profile, "--overwrite")
        Write-Host "Canal stable/latest.json actualizado: $version"
    } else {
        Write-Host "latest.json no se ha modificado. Seleccione COMMIT en el panel para buscar esta release."
    }
    Write-Host "La App debe tener un recurso UC Volume con clave ota_firmware_volume (Can read)."
    Write-Host "El dispositivo no se actualiza solo: use COMMIT -> ENTER -> INSTALAR tras comprobar la API."
}
finally {
    Pop-Location
}
