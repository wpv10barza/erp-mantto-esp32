[CmdletBinding()]
param(
    [string]$BusId,
    [switch]$NoFlash,
    [switch]$StartBackend,
    [switch]$NonInteractive
)

$ErrorActionPreference = 'Stop'
$RepoUrl = 'https://github.com/wpv10barza/erp-mantto-esp32.git'
$Branch = 'main'
$StateRoot = Join-Path $env:LOCALAPPDATA 'ERP-MANTTO-ESP32-PCU'
$BootstrapRepo = Join-Path $StateRoot 'repo'
$BusIdFile = Join-Path $StateRoot 'busid.txt'
$LogFile = Join-Path $StateRoot 'pcu.log'

New-Item -ItemType Directory -Force -Path $StateRoot | Out-Null

function Write-Step([string]$Text) {
    Write-Host "`n== $Text ==" -ForegroundColor Cyan
}

function Require-Command([string]$Name, [string]$Help) {
    if (-not (Get-Command $Name -ErrorAction SilentlyContinue)) {
        throw "$Name no esta disponible. $Help"
    }
}

function Invoke-Git([string[]]$Arguments) {
    & git @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "git fallo: git $($Arguments -join ' ')"
    }
}

function Sync-BootstrapRepository {
    Write-Step 'Sincronizar repositorio ERP desde origin/main'
    if (-not (Test-Path (Join-Path $BootstrapRepo '.git'))) {
        if (Test-Path $BootstrapRepo) {
            Remove-Item -Recurse -Force $BootstrapRepo
        }
        Invoke-Git @('clone','--branch',$Branch,'--single-branch',$RepoUrl,$BootstrapRepo)
    }

    Invoke-Git @('-C',$BootstrapRepo,'remote','set-url','origin',$RepoUrl)
    Invoke-Git @('-C',$BootstrapRepo,'fetch','origin',$Branch,'--prune')
    Invoke-Git @('-C',$BootstrapRepo,'checkout','-B',$Branch,"origin/$Branch")
    Invoke-Git @('-C',$BootstrapRepo,'reset','--hard',"origin/$Branch")

    $local = (& git -C $BootstrapRepo rev-parse HEAD).Trim()
    $remoteLine = (& git ls-remote $RepoUrl "refs/heads/$Branch" | Select-Object -First 1)
    if (-not $remoteLine) { throw 'No se pudo resolver origin/main.' }
    $remote = ($remoteLine -split '\s+')[0]
    if ($local -ne $remote) {
        throw "El clon de despliegue no coincide con main. local=$local remote=$remote"
    }
    Write-Host "ERP main verificado: $local" -ForegroundColor Green
    return $local
}

function Verify-FirmwareManifest {
    Write-Step 'Verificar que el firmware local es exactamente el declarado por main'
    $manifestPath = Join-Path $BootstrapRepo 'firmware\firmware-sync.json'
    if (-not (Test-Path $manifestPath)) { throw "Falta $manifestPath" }
    $manifest = Get-Content -Raw $manifestPath | ConvertFrom-Json
    if ($manifest.authoritative_repository -ne 'wpv10barza/erp-mantto-esp32' -or $manifest.authoritative_branch -ne 'main') {
        throw 'El manifiesto no declara erp-mantto-esp32/main como fuente autoritativa.'
    }

    foreach ($property in $manifest.git_blob_sha1.PSObject.Properties) {
        $relative = $property.Name
        $expected = [string]$property.Value
        $full = Join-Path $BootstrapRepo ($relative -replace '/', '\')
        if (-not (Test-Path $full)) { throw "Falta archivo critico: $relative" }
        $actual = (& git -C $BootstrapRepo hash-object -- $relative).Trim()
        if ($actual -ne $expected) {
            throw "Firmware distinto de main: $relative esperado=$expected actual=$actual"
        }
        Write-Host "OK $relative  $actual"
    }

    Write-Host "DNS esperado: $($manifest.backend.service_type) -> $($manifest.backend.logical_host)" -ForegroundColor Green
}

function Get-UsbipdText {
    $lines = & usbipd list 2>&1
    if ($LASTEXITCODE -ne 0) { throw 'usbipd list fallo.' }
    return @($lines | ForEach-Object { [string]$_ })
}

function Select-EspBusId {
    param([string]$Requested)

    $lines = Get-UsbipdText
    Write-Step 'Dispositivos USB detectados'
    $lines | ForEach-Object { Write-Host $_ }

    if ($Requested) {
        if (-not ($lines -match "^\s*$([regex]::Escape($Requested))\s+")) {
            throw "El BUSID indicado no aparece en usbipd list: $Requested"
        }
        return $Requested
    }

    if (Test-Path $BusIdFile) {
        $saved = (Get-Content -Raw $BusIdFile).Trim()
        if ($saved -and ($lines -match "^\s*$([regex]::Escape($saved))\s+")) {
            Write-Host "Reutilizando BUSID guardado: $saved" -ForegroundColor Green
            return $saved
        }
    }

    $candidates = @()
    foreach ($line in $lines) {
        if ($line -match '^\s*(\d+-\d+(?:\.\d+)*)\s+.*(Espressif|ESP32|USB JTAG|CP210|CH340|CH343|USB Serial|Serial/JTAG)') {
            $candidates += $Matches[1]
        }
    }
    $candidates = @($candidates | Select-Object -Unique)

    if ($candidates.Count -eq 1) { return $candidates[0] }
    if ($candidates.Count -eq 0) {
        throw 'No se detecto automaticamente un ESP32. Use -BusId 1-1 (o el BUSID mostrado por usbipd list).'
    }
    if ($NonInteractive) {
        throw "Hay varios candidatos USB: $($candidates -join ', '). Use -BusId."
    }

    Write-Host "Candidatos: $($candidates -join ', ')"
    $chosen = (Read-Host 'Escriba el BUSID del ESP32').Trim()
    if ($chosen -notin $candidates) { throw "BUSID no valido: $chosen" }
    return $chosen
}

function Ensure-UsbAttached([string]$SelectedBusId) {
    Write-Step "Compartir USB $SelectedBusId con WSL"
    $bindOutput = & usbipd bind --busid $SelectedBusId 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-Host 'usbipd bind requiere privilegios. Solo se elevara ese comando; esta ventana permanece abierta.' -ForegroundColor Yellow
        $usbipdExe = (Get-Command usbipd).Source
        $proc = Start-Process -FilePath $usbipdExe -ArgumentList @('bind','--busid',$SelectedBusId) -Verb RunAs -Wait -PassThru
        if ($proc.ExitCode -ne 0) { throw "usbipd bind elevado fallo con codigo $($proc.ExitCode)." }
    } else {
        $bindOutput | ForEach-Object { Write-Host $_ }
    }

    $attachOutput = & usbipd attach --wsl --busid $SelectedBusId 2>&1
    if ($LASTEXITCODE -ne 0) {
        $text = ($attachOutput -join "`n")
        if ($text -notmatch 'already attached|ya.*adjunt') {
            throw "usbipd attach fallo:`n$text"
        }
    }
    $attachOutput | ForEach-Object { Write-Host $_ }
    Set-Content -NoNewline -Encoding ascii -Path $BusIdFile -Value $SelectedBusId
}

try {
    Start-Transcript -Path $LogFile -Append | Out-Null
    Write-Host '============================================================' -ForegroundColor DarkCyan
    Write-Host ' PCU ERP-MANTTO-ESP32 - ESP32-S3-4848S040 / WSL2' -ForegroundColor Cyan
    Write-Host ' Fuente autoritativa: wpv10barza/erp-mantto-esp32 main' -ForegroundColor Cyan
    Write-Host '============================================================' -ForegroundColor DarkCyan

    Require-Command 'git' 'Instale Git for Windows.'
    Require-Command 'wsl.exe' 'Habilite WSL2/Ubuntu.'
    Require-Command 'usbipd' 'Instale usbipd-win.'

    $mainSha = Sync-BootstrapRepository
    Verify-FirmwareManifest

    $selected = Select-EspBusId -Requested $BusId
    Ensure-UsbAttached -SelectedBusId $selected

    Write-Step 'Ejecutar despliegue dentro de Ubuntu/WSL'
    $wslBootstrap = (& wsl.exe wslpath -a $BootstrapRepo).Trim()
    if (-not $wslBootstrap) { throw 'No se pudo convertir la ruta del repositorio a WSL.' }
    $wslScript = "$wslBootstrap/scripts/pcu-deploy.sh"

    $args = @('bash', $wslScript, '--expected-main', $mainSha)
    if ($NoFlash) { $args += '--no-flash' }
    if ($StartBackend) { $args += '--start-backend' }
    & wsl.exe @args
    if ($LASTEXITCODE -ne 0) { throw "El despliegue WSL fallo con codigo $LASTEXITCODE." }

    Write-Host "`nPCU completado. BUSID=$selected main=$mainSha" -ForegroundColor Green
    Write-Host "Log: $LogFile"
}
catch {
    Write-Host "`n[ERROR] $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Log: $LogFile" -ForegroundColor Yellow
    if (-not $NonInteractive) {
        Read-Host 'Presione ENTER para cerrar'
    }
    exit 1
}
finally {
    try { Stop-Transcript | Out-Null } catch { }
}
