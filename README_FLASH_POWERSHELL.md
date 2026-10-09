# Flash completo desde PowerShell — ESP32-S3-4848S040

Este procedimiento actualiza el repositorio `wpv10barza/erp-mantto-esp32` desde `main`, ejecuta las mismas pruebas fundamentales usadas en GitHub Actions, compila el objetivo de producción `panel_4848s040` y realiza un flash limpio del panel desde **Windows PowerShell**. El backend continúa en Databricks Apps; no se necesita WSL para este flujo de flasheo.

> **Importante:** no pegues contraseñas, tokens ni secretos en GitHub. `include/local_config.h` está excluido del repositorio y es el único lugar local donde deben quedar las credenciales del dispositivo.

## 1. Requisitos en Windows

Instala Git for Windows, Python 3.12 o compatible y el controlador USB del ESP32-S3 si Windows no crea un puerto COM. Después abre **PowerShell**:

```powershell
git --version
py --version
py -m pip install --upgrade platformio==6.2.0
pio --version
```

## 2. Descargar o actualizar exactamente `main`

Primera instalación:

```powershell
cd $HOME
git clone https://github.com/wpv10barza/erp-mantto-esp32.git
cd .\erp-mantto-esp32
```

Si ya existe el repositorio:

```powershell
cd $HOME\erp-mantto-esp32
git fetch origin main --prune
git checkout main
git reset --hard origin/main
git clean -fd -e include/local_config.h
```

Verifica el commit que vas a flashear:

```powershell
$Main = (git rev-parse HEAD).Trim()
$Remote = ((git ls-remote origin refs/heads/main) -split "\s+")[0]
"LOCAL  = $Main"
"REMOTE = $Remote"
if ($Main -ne $Remote) { throw "El repositorio local no coincide con origin/main" }
```

## 3. Configuración privada del dispositivo

Crea el archivo local solo si todavía no existe:

```powershell
if (-not (Test-Path .\include\local_config.h)) {
  Copy-Item .\include\local_config.example.h .\include\local_config.h
  notepad .\include\local_config.h
}
```

Completa allí el SSID/clave Wi-Fi, URL de Databricks, credenciales OAuth del service principal y token de Device API. **No hagas `git add` de ese archivo.**

Confirma que Git lo ignora:

```powershell
git check-ignore .\include\local_config.h
```

## 4. Ejecutar pruebas locales antes del flash

```powershell
python .\tests\test_panel_state_contract.py
python .\tests\test_final_ui_contract.py
python .\tests\test_wifi_source.py
python .\tests\test_command_editor_integration.py
python .\tests\test_touch_gpio_contract.py
python .\test\command_buffer_regression.py
python .\tests\test_production_firmware.py
pio test -e native
```

Deben pasar, entre otras, las suites `test_editor_components` y `test_touch_gpio_ui`. Esas pruebas cubren centrado del teclado, zonas táctiles, separación entre botones y mapa GPIO.

## 5. Compilar el firmware de producción

```powershell
pio run -e panel_4848s040
```

Comprueba los artefactos:

```powershell
Get-Item .\.pio\build\panel_4848s040\firmware.bin
Get-Item .\.pio\build\panel_4848s040\bootloader.bin
Get-Item .\.pio\build\panel_4848s040\partitions.bin
Get-Item .\.pio\build\panel_4848s040\firmware.elf
```

## 6. Identificar el puerto COM del panel

Conecta el ESP32-S3 por USB y ejecuta:

```powershell
pio device list
[System.IO.Ports.SerialPort]::GetPortNames()
```

Anota el puerto, por ejemplo `COM5`. Sustituye `COM5` en los comandos siguientes.

## 7. Flash completo limpio

El borrado previo elimina el firmware y NVS anteriores. Úsalo cuando quieras garantizar que el panel arranca exclusivamente con la nueva versión:

```powershell
$Port = "COM5"
pio run -e panel_4848s040 -t erase --upload-port $Port
pio run -e panel_4848s040 -t upload --upload-port $Port
```

Si no quieres borrar NVS, omite la línea `-t erase` y ejecuta solo `-t upload`.

## 8. Monitor serie después del flash

```powershell
pio device monitor --port $Port --baud 115200
```

Para salir del monitor de PlatformIO usa **Ctrl+C**.

## 9. Verificación física obligatoria

Después del reinicio comprueba en el panel: menú principal, precisión de cada opción táctil, retorno **INICIO** desde **EDITAR ORDEN 3C**, teclado ABC/123 centrado, BKSP/SPACE/ENTER, flechas izquierda/derecha, DEL y LIMPIAR. Luego prueba **PROBAR CLOUD** para Databricks + Google Sheets y finalmente **ENVIAR 3C**.

GitHub Actions y las pruebas nativas validan código, geometría y contratos, pero **no sustituyen la prueba táctil física del GT911 ni el flasheo real del ESP32**.

## 10. Comprobar GitHub Actions del mismo commit

Si tienes GitHub CLI:

```powershell
gh run list --repo wpv10barza/erp-mantto-esp32 --branch main --limit 10
```

No uses como firmware final un commit cuyo workflow **Firmware CD** haya fallado. Primero corrige el build y espera un estado `success`.

## Recuperación rápida

Si el puerto no aparece: cambia cable USB, prueba otro puerto físico, mantén BOOT durante el inicio del upload si el panel lo requiere y vuelve a ejecutar `pio device list`. Si la compilación falla, no fuerces el flash: actualiza nuevamente `main`, ejecuta las pruebas y recompila.
