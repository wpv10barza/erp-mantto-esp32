[CmdletBinding()]
param(
    [ValidatePattern('^COM[1-9][0-9]*$')]
    [string]$Port = '',
    [switch]$ListPorts,
    [ValidatePattern('^[a-fA-F0-9]{40}$')]
    [string]$ExpectedCommit,
    [ValidateRange(0, 600)]
    [int]$SerialSeconds = 0,
    [switch]$AllowMissingConfig
)

# Windows PowerShell 5.1 / PowerShell 7. Flash only the production panel.
# This script never erases NVS, changes Git branches, or uploads secrets.
$ErrorActionPreference = 'Stop'
$RepoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$LogDirectory = Join-Path $RepoRoot 'flash-logs'
New-Item -ItemType Directory -Path $LogDirectory -Force | Out-Null
$Stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$LogFile = Join-Path $LogDirectory ("flash-{0}.log" -f $Stamp)

function Write-Log {
    param([string]$Message)
    $Line = "[{0}] {1}" -f (Get-Date -Format 'yyyy-MM-dd HH:mm:ss'), $Message
    Add-Content -LiteralPath $script:LogFile -Value $Line -Encoding UTF8
    Write-Host $Line
}

function Get-UsbSerialDevices {
    param([string[]]$AvailablePorts)
    $Found = @()
    try {
        $Devices = @(Get-CimInstance Win32_PnPEntity -ErrorAction Stop)
        foreach ($Device in $Devices) {
            $Match = [regex]::Match([string]$Device.Name, '\((COM[0-9]+)\)')
            if (-not $Match.Success) { continue }
            $DetectedPort = $Match.Groups[1].Value
            if ($AvailablePorts -notcontains $DetectedPort) { continue }
            $Found += [pscustomobject]@{
                Port = $DetectedPort
                Name = [string]$Device.Name
                PnpId = [string]$Device.PNPDeviceID
            }
        }
    }
    catch {
        Write-Log ("[WARN] No se pudo consultar Win32_PnPEntity: {0}" -f $_.Exception.Message)
    }
    return $Found
}

function Show-PortDiagnostics {
    param([string[]]$AvailablePorts, [object[]]$Devices)
    if ($AvailablePorts.Count -eq 0) {
        Write-Log '[USB] Windows no detecta ningun puerto COM.'
    }
    else {
        Write-Log ("[USB] Puertos COM: {0}" -f ($AvailablePorts -join ', '))
    }
    foreach ($Device in $Devices) {
        Write-Log ("[USB] {0} | {1} | {2}" -f $Device.Port, $Device.Name, $Device.PnpId)
    }
    Write-Log '[AYUDA] Si no aparece el ESP32: use cable USB de datos, otro conector o BOOT + RESET/EN.'
    Write-Log '[AYUDA] Diagnostico adicional: py -m platformio device list'
}

try {
    Write-Log '=== ESP32-S3-4848S040 | FLASH WINDOWS CON REGISTRO ==='
    Write-Log ("[LOG] {0}" -f $LogFile)
    $AvailablePorts = @([System.IO.Ports.SerialPort]::GetPortNames() | Sort-Object -Unique)
    $Devices = @(Get-UsbSerialDevices -AvailablePorts $AvailablePorts)
    Show-PortDiagnostics -AvailablePorts $AvailablePorts -Devices $Devices

    if ($ListPorts) {
        Write-Log '[INFO] Solo diagnostico: no se ha intentado flashear.'
        return
    }

    $SelectedPort = $Port
    if (-not $SelectedPort) {
        # Auto-selection is allowed only for one recognized ESP32/USB-UART device.
        # A lone Bluetooth/legacy COM port is never treated as an ESP32.
        $Candidates = @(
            $Devices |
                Where-Object { $_.PnpId -match 'VID_(303A|10C4|1A86|0403)' } |
                Select-Object -ExpandProperty Port -Unique
        )
        if ($Candidates.Count -eq 1) {
            $SelectedPort = [string]$Candidates[0]
            Write-Log ("[USB] Puerto USB-serie reconocido automaticamente: {0}" -f $SelectedPort)
        }
        elseif ($Candidates.Count -eq 0) {
            throw 'No se identifico un unico ESP32 por USB. Conecte el panel, ejecute -ListPorts y luego indique -Port COMx.'
        }
        else {
            throw ("Varios dispositivos USB-serie detectados ({0}). Indique -Port COMx." -f ($Candidates -join ', '))
        }
    }

    if ($AvailablePorts -notcontains $SelectedPort) {
        throw ("El puerto {0} no existe actualmente. No se ha iniciado la carga." -f $SelectedPort)
    }

    Set-Location -LiteralPath $RepoRoot
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
        throw 'No se encontro Git en PATH.'
    }
    if (-not (Get-Command py -ErrorAction SilentlyContinue)) {
        throw 'No se encontro el lanzador Python py en PATH.'
    }

    $Branch = (& git branch --show-current | Select-Object -First 1)
    if ($LASTEXITCODE -ne 0 -or ($Branch -ne 'main' -and -not $ExpectedCommit)) {
        throw 'Para una rama o commit de prueba indique -ExpectedCommit con el SHA completo validado en Actions.'
    }
    $Commit = (& git rev-parse HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $Commit -notmatch '^[a-f0-9]{40}$') {
        throw 'No se pudo verificar el commit local.'
    }
    if ($ExpectedCommit -and $Commit -ne $ExpectedCommit.ToLowerInvariant()) {
        throw 'HEAD no coincide con -ExpectedCommit. No se ha iniciado la carga.'
    }
    $TrackedChanges = @(& git status --porcelain --untracked-files=no | Where-Object { $_.Trim() })
    if ($LASTEXITCODE -ne 0 -or $TrackedChanges.Count -gt 0) {
        throw 'Existen cambios en archivos versionados. Revise git status; no se borrara ningun archivo.'
    }
    Write-Log ("[FW] Commit local: {0}" -f $Commit)

    $PrivateConfig = Join-Path $RepoRoot 'include/local_config.h'
    if (-not (Test-Path -LiteralPath $PrivateConfig -PathType Leaf)) {
        if (-not $AllowMissingConfig) {
            throw 'Falta include/local_config.h. Configure las credenciales localmente o use -AllowMissingConfig solo si ya estan preservadas en NVS.'
        }
        Write-Log '[WARN] Compilacion sin local_config.h: requiere credenciales previamente validas en NVS.'
    }
    else {
        Write-Log '[CONFIG] include/local_config.h presente; su contenido no se registrara.'
    }

    # USB VID/PID identifies an adapter, not the chip behind it.
    $PreviousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & py -c "import esptool" 2>$null
        $ProbeDependencyExit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $PreviousPreference }
    if ($ProbeDependencyExit -ne 0) {
        & py -m pip install esptool==4.8.1
        if ($LASTEXITCODE -ne 0) { throw 'No se pudo instalar esptool para verificar el chip.' }
    }
    $PreviousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        $ChipOutput = @(& py -m esptool --chip esp32s3 --port $SelectedPort chip_id 2>&1 | ForEach-Object { [string]$_ })
        $ChipExit = $LASTEXITCODE
    } finally { $ErrorActionPreference = $PreviousPreference }
    if ($ChipExit -ne 0 -or ($ChipOutput -join "`n") -notmatch 'Chip is ESP32-S3') {
        throw 'Chip no confirmado como ESP32-S3. Se bloqueo la carga; ESP32-D0WD-V3 no es compatible.'
    }
    Write-Log '[CHIP] ESP32-S3 confirmado por esptool. Use exclusivamente el panel 4848S040.'

    Write-Log ("[UPLOAD] Entorno panel_4848s040; puerto {0}; sin erase." -f $SelectedPort)
    Write-Log '[UPLOAD] PlatformIO recompila si es necesario e incluye bootloader/particiones para USB.'
    $PreviousPreference = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try {
        & py -m platformio run -e panel_4848s040 -t upload --upload-port $SelectedPort 2>&1 |
            ForEach-Object { [string]$_ } |
            Tee-Object -FilePath $LogFile -Append
        $UploadExitCode = $LASTEXITCODE
    }
    finally {
        $ErrorActionPreference = $PreviousPreference
    }
    if ($UploadExitCode -ne 0) {
        throw ("PlatformIO no pudo instalar el firmware (exit {0}). Consulte el log." -f $UploadExitCode)
    }
    Write-Log ("[OK] FLASH COMPLETADO en {0}; commit fuente {1}" -f $SelectedPort, $Commit)

    if ($SerialSeconds -gt 0) {
        Write-Log ("[SERIAL] Captura opcional de {0} segundos a 115200 baudios." -f $SerialSeconds)
        $Serial = $null
        try {
            $Serial = New-Object System.IO.Ports.SerialPort -ArgumentList @($SelectedPort, 115200)
            $Serial.Open()
            $Deadline = (Get-Date).AddSeconds($SerialSeconds)
            while ((Get-Date) -lt $Deadline) {
                $Chunk = $Serial.ReadExisting()
                if ($Chunk) {
                    $Normalized = $Chunk.Replace([string][char]13, '')
                    foreach ($Line in ($Normalized -split [string][char]10)) {
                        if ($Line.Trim()) { Write-Log ("[SERIAL] {0}" -f $Line.TrimEnd()) }
                    }
                }
                Start-Sleep -Milliseconds 150
            }
        }
        catch {
            # A USB re-enumeration can prevent the optional monitor from opening.
            # It must never turn a successful flash into a false flash failure.
            Write-Log ("[WARN] No se pudo capturar serie: {0}" -f $_.Exception.Message)
        }
        finally {
            if ($null -ne $Serial) { $Serial.Dispose() }
        }
    }
    Write-Log ("[LOG] Evidencia local: {0}" -f $LogFile)
    Write-Log '[SIGUIENTE] Verifique fisicamente pantalla, tactil y EDITAR ORDEN 3C.'
}
catch {
    Write-Log ("[ERROR] {0}" -f $_.Exception.Message)
    Write-Log ("[LOG] Evidencia local: {0}" -f $LogFile)
    throw
}
