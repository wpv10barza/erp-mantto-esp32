# Sharing access without moving private keys or changing other principals.
# Default: audit only. Writes require explicit -Aplicar.
[CmdletBinding()]
param(
    [ValidatePattern('^[^@\s]+@[^@\s]+\.[^@\s]+$')]
    [string]$Correo = 'polovaleriow@gmail.com',
    [ValidateSet('CAN_USE','CAN_MANAGE')]
    [string]$Nivel = 'CAN_MANAGE',
    [string]$Perfil = 'asistente-cloud-erp',
    [string]$Aplicacion = 'asistente-cloud-erp',
    [switch]$Aplicar,
    [string]$GitHubUsuario = ''
)
$ErrorActionPreference = 'Stop'

function Assert-Exit([string]$Paso) {
    if ($LASTEXITCODE -ne 0) {
        throw ("No se pudo completar {0}; exit={1}" -f $Paso, $LASTEXITCODE)
    }
}
if (-not (Get-Command databricks -ErrorAction SilentlyContinue)) {
    throw 'Falta Databricks CLI. No se realizaron cambios.'
}
if ($GitHubUsuario -and $GitHubUsuario -notmatch '^[A-Za-z\d](?:[A-Za-z\d-]{0,37}[A-Za-z\d])?$') {
    throw 'Indique el usuario GitHub real, no un correo. No se realizaron cambios.'
}
if ($GitHubUsuario -and -not (Get-Command gh -ErrorAction SilentlyContinue)) {
    throw 'Para invitar al colaborador GitHub se necesita gh CLI autenticado.'
}

Write-Host "[DATBRICKS] Perfil: $Perfil | Aplicacion: $Aplicacion"
Write-Host "[DESTINO] $Correo | Nivel solicitado: $Nivel"
Write-Host '[SEGURIDAD] CAN_MANAGE permite cambiar y eliminar la App; no concede acceso a secretos sin permisos adicionales.'

# List/inspect before changing identity or permissions.
$usersRaw = & databricks users list --filter "userName eq '$Correo'" --profile $Perfil --output json
Assert-Exit 'consulta de usuarios'
$users = $usersRaw | ConvertFrom-Json
$existing = @($users.Resources | Where-Object { $_ -and $_.userName -ieq $Correo })
if ($existing.Count -gt 1) { throw 'Mas de una identidad coincidente; detener.' }

$permsRaw = & databricks apps get-permissions $Aplicacion --profile $Perfil --output json
Assert-Exit 'consulta de permisos App'
$perms = $permsRaw | ConvertFrom-Json
$grants = @($perms.access_control_list | Where-Object { $_ -and $_.user_name -ieq $Correo })
Write-Host ("[ESTADO] Miembro Databricks existente: {0}" -f ($existing.Count -eq 1))
if ($grants.Count -eq 0) { Write-Host '[ESTADO] Sin permiso directo a la App.' }
foreach ($grant in $grants) {
    $names = @($grant.all_permissions | ForEach-Object { $_.permission_level }) -join ','
    Write-Host ("[ESTADO] App actual: {0}" -f $names)
}

if (-not $Aplicar) {
    Write-Host '[AUDITORIA] No se modifico ningun permiso. Anada -Aplicar para invitar y asignar el nivel indicado.'
} else {
    if ($existing.Count -eq 0) {
        Write-Host '[IDENTIDAD] Dando de alta la cuenta en este workspace; requiere administrador.'
        $null = & databricks users create --user-name $Correo --profile $Perfil --output json
        Assert-Exit 'alta del usuario Databricks'
    }

    # PATCH / update, NEVER set-permissions. Preserve all other principals.
    $payload = @{ access_control_list = @(@{
        user_name = $Correo
        permission_level = $Nivel
    }) } | ConvertTo-Json -Compress -Depth 6
    $tempFile = Join-Path ([IO.Path]::GetTempPath()) ("3c-app-permission-" + [guid]::NewGuid().ToString("N") + ".json")
    try {
        [IO.File]::WriteAllText($tempFile, $payload, [Text.UTF8Encoding]::new($false))
        $null = & databricks apps update-permissions $Aplicacion --json "@$tempFile" --profile $Perfil --output json
        Assert-Exit 'actualizacion no destructiva de permisos de App'
    } finally {
        if (Test-Path -LiteralPath $tempFile) { Remove-Item -LiteralPath $tempFile -Force }
    }

    $checked = & databricks apps get-permissions $Aplicacion --profile $Perfil --output json | ConvertFrom-Json
    Assert-Exit 'lectura posterior de permisos'
    $matching = @($checked.access_control_list | Where-Object { $_ -and $_.user_name -ieq $Correo })
    $levels = @($matching | ForEach-Object { $_.all_permissions } | ForEach-Object { $_.permission_level })
    if ($levels -notcontains $Nivel) {
        throw "No se verifico el permiso $Nivel para ${Correo}: revise la UI de Databricks."
    }
    Write-Host "[OK] Permiso $Nivel verificado en $Aplicacion para $Correo."
}

# GitHub email and username are separate identities. Never guess username from email.
if ($GitHubUsuario) {
    $null = & gh api "users/$GitHubUsuario" --jq .login
    Assert-Exit 'identidad GitHub'
    $repos = @(
      'erp-mantto-esp32',
      'asistente-de-databricks--erp',
      'voice-embeding-in-databricks-for-esp32'
    )
    foreach ($repo in $repos) {
        if ($Aplicar) {
            $null = & gh api --method PUT "repos/wpv10barza/$repo/collaborators/$GitHubUsuario" -f permission=push
            Assert-Exit "invitacion GitHub $repo"
            Write-Host "[INVITADO] $repo -> $GitHubUsuario; la persona debe aceptar."
        } else {
            Write-Host "[AUDITORIA] Se invitara a $GitHubUsuario a $repo con permiso push al aplicar."
        }
    }
} else {
    Write-Host '[GITHUB] Repos publicos: lectura/clone disponibles sin invitacion.'
    Write-Host '[GITHUB] Para push, vuelva a ejecutar con -GitHubUsuario usuario_real y -Aplicar.'
}
Write-Host '[SHEETS] La hoja Estrategias_20260527_2119 ya pertenece a polovaleriow@gmail.com.'
Write-Host '[ALERTA] Revise y cambie el acceso general cualquiera-con-enlace/Editor a Restringido desde Drive.'
Write-Host '[FINAL] No se distribuyeron OAuth, secretos, contrasenas ni firmware.bin privado.'
