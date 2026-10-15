[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][uri]$DeviceUrl,
    [Parameter(Mandatory=$true)]
    [ValidateSet('Estado','Databricks','Servicios','Historial','VozPC','Enviar3C','ConsultarFirmware')]
    [string]$Action,
    [string]$Text,
    [string]$Commit,
    [string]$DeviceToken = $env:ESP32_DEVICE_TOKEN
)
$ErrorActionPreference = 'Stop'
if ($DeviceUrl.Scheme -notin @('http','https') -or $DeviceUrl.UserInfo -or $DeviceUrl.Query -or $DeviceUrl.Fragment -or $DeviceUrl.AbsolutePath -ne '/') {
    throw 'DeviceUrl debe ser el origen del ESP32, por ejemplo http://192.168.1.50'
}
$routes = @{
    Estado='/health'; Databricks='/api/backend-health'; Servicios='/api/cloud-verify'
    Historial='/api/history'; VozPC='/api/voice-capture'; Enviar3C='/api/3c'; ConsultarFirmware='/api/ota/check'
}
$request = @{
    Uri=$DeviceUrl.AbsoluteUri.TrimEnd('/') + $routes[$Action]
    Method='Post'; TimeoutSec=10; MaximumRedirection=0
}
if ($Action -eq 'Estado') { $request.Method='Get' }
else {
    if ([string]::IsNullOrWhiteSpace($DeviceToken)) { throw 'Configure ESP32_DEVICE_TOKEN o use -DeviceToken.' }
    $request.Headers=@{'X-3C-Device-Token'=$DeviceToken}
}
if ($Action -eq 'Enviar3C') {
    if ([string]::IsNullOrWhiteSpace($Text) -or [Text.Encoding]::UTF8.GetByteCount($Text.Trim()) -gt 240) {
        throw 'Text requiere de 1 a 240 bytes UTF-8.'
    }
    $request.Body=@{text=$Text.Trim()}
    $request.ContentType='application/x-www-form-urlencoded; charset=utf-8'
}
if ($Action -eq 'ConsultarFirmware') {
    if ($Commit -notmatch '^[a-fA-F0-9]{40}$') { throw 'Commit requiere 40 caracteres hexadecimales.' }
    $request.Body=@{commit=$Commit.ToLowerInvariant()}
    $request.ContentType='application/x-www-form-urlencoded; charset=utf-8'
}
# No automatic retries: a timeout does not prove the device rejected the action.
$response = Invoke-RestMethod @request
$response
if ($response.status -eq 'queued') {
    Write-Host 'Solicitud en cola. Verifique el resultado en el panel; no significa ejecucion completada.'
}
