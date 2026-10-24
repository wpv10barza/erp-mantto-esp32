# Create/complete exactly four Databricks secrets for a NEW student workspace.
# Does not copy values from the original workspace, write plaintext credentials
# to Git, Notepad, temporary JSON payloads, or any CLI argument.
# Requires a VALID student's Gemini API key and NEW Google service-account JSON.
# Windows PowerShell 5.1 and PowerShell 7.
[CmdletBinding()]
param(
  [ValidatePattern('^[A-Za-z0-9._-]{1,128}$')]
  [string]$PerfilNuevo = 'alumno-3c',
  [ValidatePattern('^[A-Za-z0-9._-]{1,128}$')]
  [string]$ScopeNuevo = 'alumno-3c',
  [string]$CorreoEsperado = 'polovaleriow@gmail.com',
  [string]$GoogleJsonPath = ''
)
$ErrorActionPreference = 'Stop'

function Assert-Exit([string]$Contexto) {
  if ($LASTEXITCODE -ne 0) { throw "Databricks fallo en $Contexto (codigo $LASTEXITCODE)." }
}

function Invoke-DbJson {
  param(
    [Parameter(Mandatory=$true)][string[]]$CliArgs,
    [Parameter(Mandatory=$true)][string]$Contexto
  )
  $response = & databricks @CliArgs
  Assert-Exit $Contexto
  $raw = @($response) -join [Environment]::NewLine
  if (-not $raw.Trim()) { throw "Databricks no devolvio JSON en $Contexto." }
  try {
    return ConvertFrom-Json -InputObject $raw -ErrorAction Stop
  } catch {
    # Do not echo responses; CLI messages may contain sensitive details.
    throw "Respuesta de Databricks no es JSON valido en $Contexto. Consulte CLI/auth y ejecute el comando con --output json. No se muestran datos privados."
  }
}

function Extract-Items([object]$Source, [string]$Property) {
  if ($null -eq $Source) { return @() }
  if ($Source -is [array]) { return @($Source) }
  if ($Source.PSObject.Properties.Name -contains $Property) {
    return @($Source.$Property | Where-Object { $null -ne $_ })
  }
  return @()
}

# A native process with redirected STDIN prevents secrets being exposed through
# process command-line arguments, normal PS history, or a temporary payload file.
function Put-SecretByStdin([string]$Key, [string]$Value) {
  if ([string]::IsNullOrWhiteSpace($Value)) { throw "Valor vacio: $Key" }
  $exe = (Get-Command databricks -CommandType Application -ErrorAction Stop).Source
  $psi = New-Object System.Diagnostics.ProcessStartInfo
  $psi.FileName = $exe
  $psi.Arguments = "secrets put-secret $ScopeNuevo $Key --profile $PerfilNuevo"
  $psi.UseShellExecute = $false
  $psi.CreateNoWindow = $true
  $psi.RedirectStandardInput = $true
  $psi.RedirectStandardOutput = $true
  $psi.RedirectStandardError = $true
  $psi.StandardInputEncoding = [System.Text.UTF8Encoding]::new($false)
  $process = [System.Diagnostics.Process]::Start($psi)
  if ($null -eq $process) { throw "No se pudo iniciar Databricks CLI para $Key." }
  try {
    $process.StandardInput.Write($Value)
    $process.StandardInput.Close()
    $null = $process.StandardOutput.ReadToEnd()
    $null = $process.StandardError.ReadToEnd()
    $process.WaitForExit()
    if ($process.ExitCode -ne 0) {
      throw "El registro de $Key fallo (exit $($process.ExitCode)). No se imprimen credenciales."
    }
  } finally {
    $process.Dispose()
  }
  Write-Host "[OK] $Key registrado. Valor oculto."
}

function New-SecretToken {
  $bytes = New-Object byte[] 48
  $rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
  try { $rng.GetBytes($bytes) } finally { $rng.Dispose() }
  return (-join ($bytes | ForEach-Object { $_.ToString('x2') }))
}

if (-not (Get-Command databricks -CommandType Application -ErrorAction SilentlyContinue)) {
  throw 'No se encontro Databricks CLI. Instale CLI y autentique alumno-3c.'
}

$user = Invoke-DbJson -CliArgs @('current-user','me','--profile',$PerfilNuevo,'--output','json') -Contexto 'usuario'
if ([string]$user.userName -ine $CorreoEsperado) {
  throw "Perfil incorrecto. Se esperaba $CorreoEsperado; no se altero ningun secreto."
}
Write-Host "[OK] Perfil $PerfilNuevo autenticado como $CorreoEsperado."

$scopesResponse = Invoke-DbJson -CliArgs @('secrets','list-scopes','--profile',$PerfilNuevo,'--output','json') -Contexto 'list-scopes'
$scopes = @(Extract-Items $scopesResponse 'scopes')
if (-not @($scopes | Where-Object { $_ -and $_.name -ieq $ScopeNuevo }).Count) {
  & databricks secrets create-scope $ScopeNuevo --profile $PerfilNuevo
  Assert-Exit 'create-scope'
  Write-Host "[OK] Scope creado: $ScopeNuevo"
} else {
  Write-Host "[OK] Scope existente: $ScopeNuevo"
}

$items = Invoke-DbJson -CliArgs @('secrets','list-secrets',$ScopeNuevo,'--profile',$PerfilNuevo,'--output','json') -Contexto 'list-secrets'
$present = @(Extract-Items $items 'secrets' | ForEach-Object { [string]$_.key })

$required = @('gemini_api_key','google_service_account_json','esp32_api_token','reviewer_token')
$missing = @($required | Where-Object { $present -notcontains $_ })
if ($missing.Count -eq 0) {
  Write-Host '[OK] Los cuatro secretos ya existen. Ninguna rotacion.'
  $required | ForEach-Object { Write-Host "[EXISTE] $_" }
  return
}

# Obtain prerequisite secrets WITHOUT printing or exporting to files.
$geminiValue = $null
$googleValue = $null
if ($missing -contains 'gemini_api_key') {
  Write-Host 'Gemini: ingrese una clave autentica creada en Google AI Studio (no se puede inventar).'
  $secureGemini = Read-Host 'Clave Gemini (entrada oculta)' -AsSecureString
  $ptr = [IntPtr]::Zero
  try {
    $ptr = [Runtime.InteropServices.Marshal]::SecureStringToBSTR($secureGemini)
    $geminiValue = [Runtime.InteropServices.Marshal]::PtrToStringBSTR($ptr)
  } finally {
    if ($ptr -ne [IntPtr]::Zero) {
      [Runtime.InteropServices.Marshal]::ZeroFreeBSTR($ptr)
    }
    $secureGemini.Dispose()
  }
  if ($geminiValue -notmatch '^[\x21-\x7e]{15,}$') {
    throw 'Clave Gemini ausente o con formato improbable. No se guardo ningun secreto.'
  }
}
if ($missing -contains 'google_service_account_json') {
  if ([string]::IsNullOrWhiteSpace($GoogleJsonPath)) {
    $GoogleJsonPath = Read-Host 'Ruta ABSOLUTA del JSON NUEVO de Google Cloud (Enter para cancelar)'
  }
  if ([string]::IsNullOrWhiteSpace($GoogleJsonPath)) {
    throw ("Falta google_service_account_json. No se realizaron cambios. " +
      "Primero cree/descargue el JSON de una cuenta de servicio autorizada; " +
      "luego ejecute: .\\scripts\\Completar-4-Secretos-Alumno-3C.ps1 " +
      '-GoogleJsonPath "C:\\ruta-privada\\google-alumno.json". ' +
      "Los otros tres secretos existentes se conservan.")
  }
  # Read-Host input commonly arrives surrounded by quotes after copy/paste.
  $GoogleJsonPath = $GoogleJsonPath.Trim().Trim('"')
  if (-not [IO.Path]::IsPathRooted($GoogleJsonPath)) {
    throw 'La ruta del JSON debe ser absoluta (ejemplo: C:\\ruta-privada\\google-alumno.json).'
  }
  if (-not (Test-Path -LiteralPath $GoogleJsonPath -PathType Leaf)) {
    throw "El archivo JSON indicado no existe o no es accesible. Ruta: $GoogleJsonPath"
  }
  if ($GoogleJsonPath -match '(?i)OneDrive|Google.Drive|Dropbox') {
    Write-Warning 'El archivo parece estar en una carpeta sincronizada. Guardelo en ubicacion privada fuera de nube.'
  }
  $googleValue = [System.IO.File]::ReadAllText(
    (Resolve-Path -LiteralPath $GoogleJsonPath).Path,
    [System.Text.UTF8Encoding]::new($false,$true)
  )
  try { $sa = ConvertFrom-Json -InputObject $googleValue } catch {
    throw 'JSON de Google invalido; no se registraron valores nuevos.'
  }
  if ($sa.type -ne 'service_account' -or
      -not $sa.client_email -or
      -not ([string]$sa.client_email).EndsWith('.gserviceaccount.com') -or
      -not ([string]$sa.private_key).StartsWith('-----BEGIN PRIVATE KEY-----')) {
    throw 'El JSON no tiene estructura de service_account o falta su clave privada.'
  }
  Write-Host ("[GOOGLE] Nueva cuenta de servicio: " + [string]$sa.client_email)
  Write-Host '[NOTA] Conceda acceso a esa identidad en Sheets; crear el secreto no otorga permisos Google.'
  $sa = $null
}

try {
  # Existing secrets are never overwritten or rotated.
  foreach ($key in $required) {
    if ($present -contains $key) {
      Write-Host "[EXISTE] $key; se conserva valor anterior."
      continue
    }
    if ($key -eq 'gemini_api_key') {
      Put-SecretByStdin $key $geminiValue
    } elseif ($key -eq 'google_service_account_json') {
      Put-SecretByStdin $key $googleValue
    } elseif ($key -eq 'esp32_api_token') {
      Put-SecretByStdin $key (New-SecretToken)
    } elseif ($key -eq 'reviewer_token') {
      Put-SecretByStdin $key (New-SecretToken)
    }
  }
} finally {
  $geminiValue = $null
  $googleValue = $null
}

$final = Invoke-DbJson -CliArgs @('secrets','list-secrets',$ScopeNuevo,'--profile',$PerfilNuevo,'--output','json') -Contexto 'verificacion final'
$keys = @(Extract-Items $final 'secrets' | ForEach-Object { [string]$_.key })
$notFound = @($required | Where-Object { $keys -notcontains $_ })
if ($notFound.Count) {
  throw ("Registro parcial, faltan nombres: " + ($notFound -join ','))
}
Write-Host "[OK] Cuatro secretos confirmados en $ScopeNuevo, perfil $PerfilNuevo."
Write-Host '[AVISO] No se copiaron credenciales del workspace original ni se crearon archivos con secretos.'
Write-Host '[SIGUIENTE] Vincular recursos de App y comprobar autenticacion real de Google Sheets/Gemini.'
