#requires -Version 5.1
[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][uri]$DeviceUrl,
    [string]$DeviceToken = $env:ESP32_DEVICE_TOKEN
)
$ErrorActionPreference = 'Stop'
$client = Join-Path $PSScriptRoot 'Activar-ESP32-REST.ps1'
# The menu selects an operation. Only its execution triggers the REST request.
$items = @(
    @{Name='Conexion al backend'; Action='Databricks'},
    @{Name='Google Sheets - verificar servicios'; Action='Servicios'},
    @{Name='GitHub Actions - abrir ejecuciones'; Action='GitHub'},
    @{Name='Actualizar firmware - consultar commit'; Action='ConsultarFirmware'},
    @{Name='Wi-Fi 2.4 GHz - consultar estado'; Action='Estado'},
    @{Name='Nube Databricks - iniciar App'; Action='IniciarDatabricks'},
    @{Name='Diagnostico - estado y contadores'; Action='Estado'},
    @{Name='Estado del dispositivo'; Action='Estado'},
    @{Name='PROBAR CLOUD'; Action='Servicios'},
    @{Name='ENVIAR 3C'; Action='Enviar3C'},
    @{Name='HISTORIAL - actualizar en ESP32'; Action='Historial'},
    @{Name='DICTAR PC - solicitar captura'; Action='VozPC'}
)
while ($true) {
    Write-Host "`n=== INTERFAZ PORTATIL - MENU PRINCIPAL ===" -ForegroundColor Cyan
    for ($i=0; $i -lt $items.Count; $i++) { Write-Host ('{0,2}. {1}' -f ($i+1),$items[$i].Name) }
    Write-Host ' 0. Salir'
    $selection = Read-Host 'Seleccione la accion'
    if ($selection -eq '0') { break }
    $number = 0
    if (-not [int]::TryParse($selection,[ref]$number) -or $number -lt 1 -or $number -gt $items.Count) {
        Write-Warning 'Seleccione una opcion de la lista.'
        continue
    }
    $item = $items[$number-1]
    $arguments = @{DeviceUrl=$DeviceUrl; DeviceToken=$DeviceToken; Action=$item.Action}
    if ($item.Action -eq 'Enviar3C') { $arguments.Text = Read-Host 'Escriba la orden 3C' }
    if ($item.Action -eq 'ConsultarFirmware') { $arguments.Commit = Read-Host 'Commit publicado (40 caracteres hexadecimales)' }
    if ((Read-Host "ACTIVAR: $($item.Name)? [S/N]") -notmatch '^[sS]$') { continue }
    try {
        if ($item.Action -eq 'GitHub') {
            Start-Process 'https://github.com/wpv10barza/erp-mantto-esp32/actions'
            Write-Host 'Pagina de ejecuciones abierta. No se ha disparado ningun workflow.'
        } else {
            & $client @arguments | Format-List | Out-Host
        }
    } catch {
        Write-Warning $_.Exception.Message
        Write-Warning 'No se reintentara automaticamente. Revise el panel antes de repetir.'
    }
}
