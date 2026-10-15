#requires -Version 5.1
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$client = Join-Path $root 'scripts/Activar-ESP32-REST.ps1'
$menu = Join-Path $root 'scripts/Menu-ESP32.ps1'
foreach ($file in @($client,$menu)) {
    $tokens=$null; $errors=$null
    $null=[System.Management.Automation.Language.Parser]::ParseFile($file,[ref]$tokens,[ref]$errors)
    if ($errors.Count) { throw ($errors | Out-String) }
}
function Assert($condition,$message) { if (-not $condition) { throw $message } }
$global:requests = New-Object System.Collections.ArrayList
$global:failRequest = $false
function global:Invoke-RestMethod {
    param($Uri,$Method,$TimeoutSec,$MaximumRedirection,$Headers,$Body,$ContentType)
    $null=$global:requests.Add(@{Uri="$Uri";Method=$Method;Headers=$Headers;Body=$Body;Timeout=$TimeoutSec;Redirects=$MaximumRedirection})
    if ($global:failRequest) { throw 'Simulated timeout' }
    return [pscustomobject]@{status='queued'}
}
$routes=@{Estado='/health';Databricks='/api/backend-health';Servicios='/api/cloud-verify';Historial='/api/history';VozPC='/api/voice-capture';Enviar3C='/api/3c';ConsultarFirmware='/api/ota/check'}
foreach ($action in $routes.Keys) {
    $argsForClient=@{DeviceUrl='http://192.0.2.1';Action=$action;DeviceToken='test-only';Text='Orden de prueba';Commit=('a'*40)}
    $null=& $client @argsForClient
    $r=$global:requests[$global:requests.Count-1]
    Assert ($r.Uri -eq ('http://192.0.2.1'+$routes[$action])) "Wrong route: $action"
    Assert ($r.Timeout -eq 10 -and $r.Redirects -eq 0) 'Timeout or redirects changed'
    if ($action -eq 'Estado') {
        Assert ($r.Method -eq 'Get' -and -not $r.Headers) 'Status must be unauthenticated GET'
    } else {
        Assert ($r.Method -eq 'Post' -and $r.Headers['X-3C-Device-Token'] -eq 'test-only') 'POST must authenticate'
    }
    if ($action -eq 'Enviar3C') { Assert ($r.Body.text -eq 'Orden de prueba') 'Command body lost' }
    if ($action -eq 'ConsultarFirmware') { Assert ($r.Body.commit -eq ('a'*40)) 'Commit body lost' }
}
foreach ($bad in @(
    @{Action='Databricks';DeviceToken=''},
    @{Action='Enviar3C';Text=''},
    @{Action='Enviar3C';Text=('x'*241)},
    @{Action='Enviar3C';Text=([string][char]0x00e1)*121},
    @{Action='ConsultarFirmware';Commit='abc'},
    @{Action='Estado';DeviceUrl='http://192.0.2.1/path'}
)) {
    $argsForClient=@{DeviceUrl='http://192.0.2.1';DeviceToken='test-only'}
    foreach ($key in $bad.Keys) { $argsForClient[$key]=$bad[$key] }
    $before=$global:requests.Count; $threw=$false
    try { $null=& $client @argsForClient } catch { $threw=$true }
    Assert ($threw -and $global:requests.Count -eq $before) 'Invalid input reached transport'
}
$global:failRequest=$true
$before=$global:requests.Count; $threw=$false
try { $null=& $client -DeviceUrl http://192.0.2.1 -Action Databricks -DeviceToken test-only } catch { $threw=$true }
Assert ($threw -and $global:requests.Count -eq $before+1) 'Timeout was retried or hidden'
$global:failRequest=$false
function global:Read-Host { param($Prompt) if ($global:answers.Count -eq 0) { throw 'Unexpected prompt' }; return $global:answers.Dequeue() }
function Run-Menu($answers) {
    $global:answers=New-Object 'System.Collections.Generic.Queue[string]'
    foreach ($answer in $answers) { $global:answers.Enqueue($answer) }
    & $menu -DeviceUrl http://192.0.2.1 -DeviceToken test-only
}
$before=$global:requests.Count
Run-Menu @('1','N','0')
Assert ($global:requests.Count -eq $before) 'Cancelled selection executed'
Run-Menu @('1','S','0')
Assert ($global:requests.Count -eq $before+1) 'Confirmed selection not executed exactly once'
Assert ($global:requests[$before].Uri -eq 'http://192.0.2.1/api/backend-health') 'Menu dispatch mismatch'
Write-Host 'PASS: PowerShell parser, routes, auth, validation, timeout and menu consent'
