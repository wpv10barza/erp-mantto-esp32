# Verifica secretos YA REGISTRADOS; nunca crea scopes ni rota tokens.
# Windows PowerShell 5.1 y 7; CLI Databricks actual.
# Uso:
#   .\scripts\Verificar-Secretos-Alumno-3C.ps1
#   .\scripts\Verificar-Secretos-Alumno-3C.ps1 -RegistrarGemini
[CmdletBinding()]
param(
    [string]$PerfilNuevo = "alumno-3c",
    [string]$ScopeNuevo = "alumno-3c",
    [string]$CorreoEsperado = "polovaleriow@gmail.com",
    [switch]$RegistrarGemini,
    [switch]$NoAbrirNotepad
)
$ErrorActionPreference = "Stop"

function Consultar-JSON {
    param([string[]]$Argumentos, [string]$Descripcion)
    $respuesta = & databricks @Argumentos
    if ($LASTEXITCODE -ne 0) {
        throw "Databricks: fallo consultando $Descripcion."
    }
    $cadena = @($respuesta) -join [Environment]::NewLine
    if (-not $cadena.Trim()) { throw "Databricks: respuesta vacia para $Descripcion." }
    return (ConvertFrom-Json -InputObject $cadena)
}
function Extraer-Lista {
    param([object]$Respuesta, [string]$Campo)
    if ($null -eq $Respuesta) { return @() }
    if ($Respuesta -is [array]) { return @($Respuesta) }
    if ($Respuesta.PSObject.Properties.Name -contains $Campo) {
        return @($Respuesta.$Campo | Where-Object { $null -ne $_ })
    }
    return @()
}

if (-not (Get-Command databricks -ErrorAction SilentlyContinue)) {
    throw "El CLI Databricks no esta instalado o no figura en PATH."
}

$usuario = Consultar-JSON @("current-user","me","--profile",$PerfilNuevo,"--output","json") "identidad"
if ([string]$usuario.userName -ine $CorreoEsperado) {
    throw "La cuenta autenticada no corresponde a $CorreoEsperado."
}
Write-Host "[OK] Cuenta alumno verificada."

# list-scopes devuelve un objeto con .scopes o un array, segun formato/version.
$scopesRespuesta = Consultar-JSON @("secrets","list-scopes","--profile",$PerfilNuevo,"--output","json") "scopes"
$scopes = @(Extraer-Lista $scopesRespuesta "scopes")
if (@($scopes | Where-Object { $_ -and [string]$_.name -ieq $ScopeNuevo }).Count -ne 1) {
    throw "No se encontro el scope $ScopeNuevo. Se detiene sin crearlo ni modificarlo."
}
Write-Host "[OK] Scope $ScopeNuevo ya existe."

$clavesRespuesta = Consultar-JSON @("secrets","list-secrets",$ScopeNuevo,"--profile",$PerfilNuevo,"--output","json") "secretos"
$claves = @(Extraer-Lista $clavesRespuesta "secrets" | ForEach-Object { [string]$_.key })
foreach ($nombre in @("esp32_api_token", "reviewer_token")) {
    if ($claves -notcontains $nombre) {
        throw "Falta $nombre. Detenido: no se generara ni reemplazara."
    }
    Write-Host "[OK] Secreto existente: $nombre"
}

$carpeta = Join-Path $env:LOCALAPPDATA "Credenciales_3C_Alumno"
$rutaTXT = Join-Path $carpeta "notepad.txt"

# Comparar en memoria la copia local con Databricks: no mostrar ningun valor.
if (Test-Path -LiteralPath $rutaTXT -PathType Leaf) {
    $locales = @{}
    foreach ($linea in (Get-Content -LiteralPath $rutaTXT -Encoding UTF8)) {
        if ($linea -match '^(esp32_api_token|reviewer_token)=([a-fA-F0-9]{64})\s*$') {
            $locales[$matches[1]] = $matches[2]
        }
    }
    foreach ($nombre in @("esp32_api_token", "reviewer_token")) {
        if (-not $locales.ContainsKey($nombre)) {
            throw "En notepad.txt falta $nombre con 64 caracteres. No se sobrescribe."
        }
        $remoto = Consultar-JSON @("secrets","get-secret",$ScopeNuevo,$nombre,"--profile",$PerfilNuevo,"--output","json") "lectura de $nombre"
        $base64Local = [Convert]::ToBase64String(
            [Text.Encoding]::UTF8.GetBytes([string]$locales[$nombre])
        )
        if (-not $remoto.value -or $base64Local -cne [string]$remoto.value) {
            throw "$nombre no coincide con notepad.txt. No se cambiaron los secretos."
        }
        Write-Host "[OK] $nombre coincide con notepad.txt; valor oculto."
        $remoto = $null
        $base64Local = $null
    }
} else {
    Write-Warning "No existe notepad.txt en la carpeta local. No se recrea ni se rotan tokens."
}

# Gemini no es una clave generada: se obtiene en Google AI Studio.
if ($claves -contains "gemini_api_key") {
    Write-Host "[OK] gemini_api_key ya esta registrado; no se sobrescribe."
} elseif ($RegistrarGemini) {
    Write-Host "[GEMINI] El CLI solicitara interactivamente la clave real de Google."
    Write-Host "[SEGURIDAD] No pegue la clave en este chat ni en un comando visible."
    & databricks secrets put-secret $ScopeNuevo gemini_api_key --profile $PerfilNuevo
    if ($LASTEXITCODE -ne 0) {
        throw "No se pudo registrar Gemini. Los tokens anteriores no se modificaron."
    }
    Write-Host "[OK] Se solicito el registro de Gemini."
} else {
    Write-Host "[PENDIENTE] gemini_api_key. Use -RegistrarGemini cuando tenga la clave real."
}

$final = Consultar-JSON @("secrets","list-secrets",$ScopeNuevo,"--profile",$PerfilNuevo,"--output","json") "inventario final"
$presentes = @(Extraer-Lista $final "secrets" | ForEach-Object { [string]$_.key })
if ($RegistrarGemini -and $presentes -notcontains "gemini_api_key") {
    throw "No se verifico gemini_api_key despues de registrarla."
}
Write-Host ("[FINAL] Secretos presentes: " + (($presentes | Sort-Object) -join ", "))
Write-Host "[OK] No se crearon scopes ni se rotaron tokens."
if ((Test-Path -LiteralPath $rutaTXT -PathType Leaf) -and -not $NoAbrirNotepad) {
    Write-Warning "notepad.txt contiene secretos en texto claro. Guardelo en un gestor cifrado."
    & notepad.exe $rutaTXT
}
