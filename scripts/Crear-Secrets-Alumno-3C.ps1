# Crear DOS tokens nuevos en Databricks del alumno y guardar notepad.txt
# fuera de Git/OneDrive. No muestra valores en terminal.
[CmdletBinding()]
param(
  [string]$PerfilNuevo = 'alumno-3c',
  [string]$ScopeNuevo = 'alumno-3c',
  [string]$CorreoEsperado = 'polovaleriow@gmail.com'
)
$ErrorActionPreference = 'Stop'
function Assert-Exit([string]$Paso) {
  if ($LASTEXITCODE -ne 0) { throw "Fallo en $Paso (exit $LASTEXITCODE)." }
}
function New-RandomHexToken {
  $bytes = New-Object byte[] 32
  $rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
  try { $rng.GetBytes($bytes) } finally { $rng.Dispose() }
  return (-join ($bytes | ForEach-Object { $_.ToString('x2') }))
}
if (-not (Get-Command databricks -ErrorAction SilentlyContinue)) {
  throw 'Falta Databricks CLI. Primero instale CLI y ejecute databricks auth login.'
}
$idJson = & databricks current-user me --profile $PerfilNuevo --output json
Assert-Exit 'validar identidad'
$identidad = $idJson | ConvertFrom-Json
if ([string]$identidad.userName -ine $CorreoEsperado) {
  throw "Perfil erroneo: $($identidad.userName); se esperaba $CorreoEsperado."
}

$scopesJson = & databricks secrets list-scopes --profile $PerfilNuevo --output json
Assert-Exit 'listar scopes'
$scopes = $scopesJson | ConvertFrom-Json
# Databricks puede devolver una lista JSON directa o { "scopes": [...] }.
$listaScopes = if ($scopes -is [array]) { @($scopes) } else { @($scopes.scopes) }
$found = @($listaScopes | Where-Object { $_ -and $_.name -ieq $ScopeNuevo })
if ($found.Count -eq 0) {
  & databricks secrets create-scope $ScopeNuevo --profile $PerfilNuevo
  Assert-Exit 'crear scope'
}
$secretsJson = & databricks secrets list-secrets $ScopeNuevo --profile $PerfilNuevo --output json
Assert-Exit 'listar secrets'
$secrets = $secretsJson | ConvertFrom-Json
$listaSecretos = if ($secrets -is [array]) { @($secrets) } else { @($secrets.secrets) }
$existing = @($listaSecretos | Where-Object {
  $_ -and $_.key -in @('esp32_api_token', 'reviewer_token')
})
if ($existing.Count -gt 0) {
  throw 'Ya existe uno o ambos tokens; se detuvo para evitar rotacion accidental.'
}
$privateDir = Join-Path $env:LOCALAPPDATA 'Proyecto3C-Alumno\Credenciales'
$notePath = Join-Path $privateDir 'notepad.txt'
if (Test-Path -LiteralPath $notePath) {
  throw "Ya existe $notePath. No se reemplazo."
}
New-Item -ItemType Directory -Force -Path $privateDir | Out-Null
$windowsUser = [System.Security.Principal.WindowsIdentity]::GetCurrent().Name
& icacls.exe $privateDir /inheritance:r /grant:r "$($windowsUser):(OI)(CI)F" | Out-Null
Assert-Exit 'proteger carpeta NTFS'

$esp32Token = New-RandomHexToken
$reviewToken = New-RandomHexToken
if ($esp32Token -eq $reviewToken) { throw 'Colision inesperada de tokens.' }
$note = @(
  'PROYECTO 3C - CREDENCIALES PRIVADAS DEL ALUMNO',
  "Usuario: $CorreoEsperado", "Perfil: $PerfilNuevo", "Scope: $ScopeNuevo", '',
  "esp32_api_token = $esp32Token",
  "reviewer_token = $reviewToken",
  'gemini_api_key = PENDIENTE: obtener de Google AI Studio',
  'google_service_account_json = PENDIENTE: nueva identidad de servicio',
  '', 'NO SUBIR ESTE ARCHIVO A GITHUB, DRIVE O CORREO.'
)
[IO.File]::WriteAllText($notePath, ($note -join [Environment]::NewLine),
  ([Text.UTF8Encoding]::new($false)))
& icacls.exe $notePath /inheritance:r /grant:r "$($windowsUser):F" | Out-Null
Assert-Exit 'proteger archivo NTFS'

# El valor NO se pasa como argumento en linea de comandos.
# El JSON temporal hereda los permisos privados de la carpeta.
function Send-Secret([string]$Key, [string]$Value) {
  $jsonPath = Join-Path $privateDir ([guid]::NewGuid().ToString('N') + '.json')
  try {
    $payload = @{ scope=$ScopeNuevo; key=$Key; string_value=$Value } |
      ConvertTo-Json -Compress
    [IO.File]::WriteAllText($jsonPath, $payload, ([Text.UTF8Encoding]::new($false)))
    & databricks secrets put-secret --json "@$jsonPath" --profile $PerfilNuevo
    Assert-Exit "cargar $Key"
  } finally {
    if (Test-Path -LiteralPath $jsonPath) {
      Remove-Item -LiteralPath $jsonPath -Force
    }
  }
}
Send-Secret 'esp32_api_token' $esp32Token
Send-Secret 'reviewer_token' $reviewToken

$checkJson = & databricks secrets list-secrets $ScopeNuevo --profile $PerfilNuevo --output json
Assert-Exit 'verificar claves'
$check = $checkJson | ConvertFrom-Json
$listaFinal = if ($check -is [array]) { @($check) } else { @($check.secrets) }
$keys = @($listaFinal | ForEach-Object { $_.key })
foreach ($key in @('esp32_api_token','reviewer_token')) {
  if ($keys -notcontains $key) { throw "No se verifico $key." }
}
$esp32Token = $null
$reviewToken = $null
Write-Host '[OK] Dos tokens de 64 caracteres registrados en Databricks.'
Write-Host "[ARCHIVO] $notePath"
Write-Host '[PENDIENTE] Registrar gemini_api_key real de Google AI Studio.'
Write-Host '[AVISO] notepad.txt tiene secretos en texto plano; borrelo cuando no lo necesite.'
Start-Process -FilePath 'notepad.exe' -ArgumentList ('"' + $notePath + '"')
