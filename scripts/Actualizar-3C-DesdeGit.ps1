# Git -> Databricks Apps -> ESP32-S3. Windows PowerShell 5.1/7.
# Archivo versionado: scripts/Actualizar-3C-DesdeGit.ps1. No imprime secretos.
[CmdletBinding()]
param(
    [ValidateSet('Sincronizar', 'Backend', 'Firmware', 'Todo')]
    [string]$Modo = 'Sincronizar',
    [string]$Puerto = 'COM9',
    [string]$Perfil = 'asistente-cloud-erp',
    [string]$Aplicacion = 'asistente-cloud-erp',
    [string]$BackendRef = 'feature/voice-frequency-review-sheets-secure-20261019',
    [string]$BackendPath = '',
    [switch]$RevisorVinculado
)

$ErrorActionPreference = 'Stop'
$FirmwarePath = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
if (-not $BackendPath) {
    $BackendPath = Join-Path (Split-Path -Parent $FirmwarePath) 'asistente-de-databricks--erp'
}
$FirmwareUrl = 'https://github.com/wpv10barza/erp-mantto-esp32.git'
$BackendUrl = 'https://github.com/wpv10barza/asistente-de-databricks--erp.git'
$AppUrl = 'https://asistente-cloud-erp-7474651957738908.aws.databricksapps.com'

function Assert-Exit([string]$Operacion) {
    if ($LASTEXITCODE -ne 0) {
        throw ("Fallo: {0} (exit {1}). No se borra ni resetea Git." -f $Operacion, $LASTEXITCODE)
    }
}

function Update-GitRepository {
    param([string]$Path, [string]$Url, [string]$Rama)
    if (-not (Test-Path -LiteralPath $Path)) {
        Write-Host "[GIT] Clonando $Url"
        & git clone -- $Url $Path | Out-Null
        Assert-Exit 'git clone'
    }
    if (-not (Test-Path -LiteralPath (Join-Path $Path '.git'))) {
        throw "No es un checkout de Git: $Path"
    }
    $origin = (& git -C $Path remote get-url origin).Trim()
    Assert-Exit 'git remote get-url'
    $expected = [IO.Path]::GetFileNameWithoutExtension($Url)
    if ($origin -notmatch ('(?i)(?:github.com[/:])wpv10barza/' + [regex]::Escape($expected) + '(?:\.git)?$')) {
        throw "El origin de $Path no corresponde al repositorio esperado."
    }
    $dirty = @(& git -C $Path status --porcelain --untracked-files=no | Where-Object { $_ -and $_.Trim() })
    Assert-Exit 'git status'
    if ($dirty.Count -ne 0) {
        throw "Hay cambios locales versionados en $Path. Revisar git status; no se descartan."
    }
    & git -C $Path fetch --prune origin $Rama | Out-Null
    Assert-Exit 'git fetch'
    & git -C $Path show-ref --verify --quiet "refs/heads/$Rama"
    $branchExists = $LASTEXITCODE -eq 0
    if ($branchExists) {
        & git -C $Path switch $Rama | Out-Null
        Assert-Exit 'git switch'
    } else {
        & git -C $Path switch --track -c $Rama "origin/$Rama" | Out-Null
        Assert-Exit 'git switch --track'
    }
    & git -C $Path merge --ff-only "origin/$Rama" | Out-Null
    Assert-Exit 'git merge --ff-only'
    $sha = (& git -C $Path rev-parse HEAD).Trim()
    Assert-Exit 'git rev-parse'
    if ($sha -notmatch '^[a-f0-9]{40}$') {
        throw 'No se verifico SHA completo de 40 caracteres.'
    }
    Write-Host "[GIT] $Path"
    Write-Host "      Rama: $Rama | commit: $sha"
    return $sha
}

function Invoke-BackendDeploy([string]$Commit) {
    if (-not $RevisorVinculado) {
        throw 'Despliegue detenido: falta -RevisorVinculado. Vincule antes en Databricks Apps el secreto review_approval_token, distinto del token del ESP32.'
    }
    if (-not (Get-Command databricks -ErrorAction SilentlyContinue)) {
        throw 'Falta Databricks CLI en PATH.'
    }
    $config = Get-Content -LiteralPath (Join-Path $BackendPath 'app.yaml') -Raw
    if ($config -notmatch 'valueFrom:\s*review_approval_token' -or $config -notmatch '(?s)name:\s*ALLOW_SHEET_WRITE\s+value:\s*"true"') {
        throw 'El app.yaml no exige revisor y escritura controlada.'
    }
    $appInfo = (& databricks apps get $Aplicacion --profile $Perfil --output json | ConvertFrom-Json)
    Assert-Exit 'databricks apps get'
    if ($null -ne $appInfo.resources) {
        $keys = @($appInfo.resources | ForEach-Object { [string]$_.name; [string]$_.resource_key } | Where-Object { $_ })
        if ($keys.Count -gt 0 -and $keys -notcontains 'review_approval_token') {
            throw 'No figura review_approval_token en recursos de la App.'
        }
    }
    # Preflight: the newly bound reviewer secret must be a single-line UTF-8
    # value. Windows/PowerShell pipelines may accidentally store UTF-16LE
    # bytes (with embedded NUL), which Databricks rejects during app deploy.
    # Never print, save or log the raw secret or its base64 representation.
    $reviewResponse = & databricks secrets get-secret asistente-cloud-erp reviewer_token --profile $Perfil --output json
    Assert-Exit 'databricks secrets get-secret reviewer_token'
    try {
        $reviewPayload = $reviewResponse | ConvertFrom-Json
        if (-not $reviewPayload.value) { throw 'Secreto ausente o vacio.' }
        $reviewBytes = [Convert]::FromBase64String([string]$reviewPayload.value)
        if ($reviewBytes.Length -lt 32) {
            throw 'reviewer_token demasiado corto: usar al menos 32 caracteres ASCII aleatorios.'
        }
        if ($reviewBytes -contains [byte]0) {
            throw 'reviewer_token contiene NUL. Reemplazar solamente este secreto con databricks secrets put-secret asistente-cloud-erp reviewer_token --profile asistente-cloud-erp (entrada interactiva ASCII); verificar que NUL=False.'
        }
        $null = ([System.Text.UTF8Encoding]::new($false, $true)).GetString($reviewBytes)
        Write-Host '[SECRET] reviewer_token valido: UTF-8, sin NUL (valor oculto).'
    } catch {
        throw ('Comprobacion del secreto revisor bloqueada: ' + $_.Exception.Message)
    } finally {
        $reviewResponse = $null
        $reviewPayload = $null
        $reviewBytes = $null
    }

    # Databricks Apps requires the previous deployment to be RUNNING before
    # accepting a new Git commit. Only auto-start a safely STOPPED app.
    $computeState = [string]$appInfo.compute_status.state
    $appState = [string]$appInfo.app_status.state
    Write-Host "[DATABRICKS] Estado previo: compute=$computeState app=$appState"
    if ($computeState -eq 'STOPPED') {
        Write-Host '[DATABRICKS] App detenida: iniciando el ultimo deployment existente.'
        & databricks apps start $Aplicacion --profile $Perfil --timeout 10m
        Assert-Exit 'databricks apps start'
        $appInfo = (& databricks apps get $Aplicacion --profile $Perfil --output json | ConvertFrom-Json)
        Assert-Exit 'databricks apps get after start'
        $computeState = [string]$appInfo.compute_status.state
        $appState = [string]$appInfo.app_status.state
        Write-Host "[DATABRICKS] Estado tras start: compute=$computeState app=$appState"
    }
    if ($appState -ne 'RUNNING' -or $computeState -ne 'ACTIVE') {
        throw (("App no lista para deploy: compute={0}, app={1}. " +
               "Consulte Databricks Apps -> Logs y repare el proceso anterior; " +
               "no se intentara deploy ni flash automaticamente.") -f $computeState, $appState)
    }
    Write-Host "[DATABRICKS] Desplegando commit $Commit"
    & databricks apps deploy $Aplicacion --git-commit $Commit --profile $Perfil --timeout 10m
    Assert-Exit 'databricks apps deploy'

    # Verificar salud HTTPS con OAuth; nunca imprimir bearer.
    $auth = (& databricks auth token --profile $Perfil --output json | ConvertFrom-Json)
    Assert-Exit 'databricks auth token'
    if (-not $auth.access_token) { throw 'No se obtuvo OAuth para comprobar /api/health.' }
    $headers = @{ Authorization = "Bearer $($auth.access_token)" }
    $verified = $false
    for ($i = 1; $i -le 8; $i++) {
        try {
            $health = Invoke-RestMethod -Uri "$AppUrl/api/health" -Headers $headers -Method GET -TimeoutSec 12
            if ($health.ok -eq $true -and $health.sheet_write_enabled -eq $true) {
                $verified = $true
                break
            }
        } catch {
            Write-Host "[HEALTH] Intento $i/8: backend no disponible todavia."
        }
        if ($i -lt 8) { Start-Sleep -Seconds 5 }
    }
    if (-not $verified) {
        throw 'Backend no verifico HTTP 200 y escritura habilitada; detener antes del flash.'
    }
    Write-Host '[OK] Backend HTTP 200, modo Sheets habilitado.'
    Write-Host '[NOTA] /api/health no demuestra una escritura; usar /review-3c y lectura posterior.'
}

if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    throw 'Git for Windows no esta disponible.'
}
$needsBackend = $Modo -in @('Sincronizar', 'Backend', 'Todo')
$needsFirmware = $Modo -in @('Sincronizar', 'Firmware', 'Todo')
$backendCommit = ''
$firmwareCommit = ''
if ($needsFirmware) {
    $firmwareCommit = Update-GitRepository -Path $FirmwarePath -Url $FirmwareUrl -Rama 'main'
}
if ($needsBackend) {
    $backendCommit = Update-GitRepository -Path $BackendPath -Url $BackendUrl -Rama $BackendRef
}
if ($Modo -in @('Backend', 'Todo')) {
    Invoke-BackendDeploy -Commit $backendCommit
}
if ($Modo -in @('Firmware', 'Todo')) {
    if ($Puerto -notmatch '^COM[1-9][0-9]*$') { throw "Puerto COM invalido: $Puerto" }
    if (-not (Get-Command py -ErrorAction SilentlyContinue)) { throw 'Falta lanzador Python py.' }
    $flash = Join-Path $FirmwarePath 'scripts\Flash-ESP32-3C.ps1'
    Write-Host "[ESP32] USB $Puerto | commit $firmwareCommit"
    & $flash -Port $Puerto -SerialSeconds 20
    if (-not $?) { throw 'Flash no confirmado. Revisar flash-logs locales.' }
    Write-Host '[OK] Carga USB completada por script de firmware. Verifique FIRMWARE INSTALLED por serie.'
}
Write-Host "[FIN] Modo $Modo. Fuente: GitHub versionado; sin scripts descargados aparte."
