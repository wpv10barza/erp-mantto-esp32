# Flash completo desde PowerShell — ESP32-S3-4848S040

Este procedimiento actualiza el repositorio `wpv10barza/erp-mantto-esp32` desde `main`, ejecuta las mismas pruebas fundamentales usadas en GitHub Actions, compila el objetivo de producción `panel_4848s040` y permite cargar el panel desde **Windows PowerShell**. El backend continúa en Databricks Apps; no se necesita WSL para este flujo de flasheo.

> **Importante:** no pegues contraseñas, tokens ni secretos en GitHub. `include/local_config.h` está excluido del repositorio y es el único lugar local donde deben quedar las credenciales del dispositivo.

## Colab/Drive y PowerShell local

- [Abrir cuaderno en Google Drive](https://drive.google.com/file/d/1_cFNIaC0fz8UhKJcpWbtqLTcQTjqGyeN/view?usp=drivesdk).
- [Abrir cuaderno en Google Colab](https://colab.research.google.com/drive/1_cFNIaC0fz8UhKJcpWbtqLTcQTjqGyeN).

El cuaderno contiene una guía Markdown para consultar desde Colab/Drive. Los comandos PowerShell se ejecutan en Windows, donde está conectado el ESP32; el runtime remoto de Colab no accede al puerto COM de tu equipo. Esta guía versionada es la referencia actual si el cuaderno conserva instrucciones anteriores.

## 1. Requisitos en Windows

Instala Git for Windows, Python 3.12 o compatible y el controlador USB del ESP32-S3 si Windows no crea un puerto COM. Después abre **PowerShell**:

```powershell
git --version
py --version
py -m pip install --upgrade platformio==6.2.0
py -m platformio --version
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
git status --short
# Si aparecen cambios locales, consérvalos antes de continuar.
if (git status --porcelain) { throw "Hay cambios locales; guárdalos antes de actualizar." }
git fetch origin main --prune
if ($LASTEXITCODE -ne 0) { throw "Falló git fetch." }
git checkout main
if ($LASTEXITCODE -ne 0) { throw "No se pudo cambiar a main." }
git merge --ff-only origin/main
if ($LASTEXITCODE -ne 0) { throw "main tiene cambios divergentes; revisa el historial." }
```

Verifica el commit que vas a flashear:

```powershell
$Main = (git rev-parse HEAD).Trim()
$Remote = ((git ls-remote origin refs/heads/main) -split "\s+")[0]
"LOCAL  = $Main"
"REMOTE = $Remote"
if ($Main -ne $Remote) { throw "El repositorio local no coincide con origin/main" }
```

La actualización conserva los cambios mediante avance rápido y no borra archivos locales. No es necesario ejecutar `git reset --hard` ni `git clean` para seguir esta guía.

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

Usa el mismo intérprete para PlatformIO y las pruebas. Este bloque se detiene ante el primer fallo:

```powershell
$ContractTests = @(
  ".\tests\test_panel_state_contract.py",
  ".\tests\test_final_ui_contract.py",
  ".\tests\test_wifi_source.py",
  ".\tests\test_command_editor_integration.py",
  ".\tests\test_touch_gpio_contract.py",
  ".\test\command_buffer_regression.py",
  ".\tests\test_production_firmware.py",
  ".\tests\test_firmware_main_sync.py",
  ".\tests\test_backend_discovery.py"
)
foreach ($ContractTest in $ContractTests) {
  py $ContractTest
  if ($LASTEXITCODE -ne 0) { throw "Falló $ContractTest" }
}
```

La regresión del buffer está en **`test/command_buffer_regression.py`**, no en `tests/test_command_buffer_regression.py`.

### Pruebas nativas: GCC del equipo anfitrión

El objetivo `native` necesita **gcc y g++ de Windows en PATH**. El compilador Xtensa instalado para el ESP32 no sustituye ese compilador anfitrión.

```powershell
Get-Command gcc -ErrorAction Stop
Get-Command g++ -ErrorAction Stop
gcc --version
g++ --version
py -m platformio test -e native
if ($LASTEXITCODE -ne 0) { throw "Fallaron las pruebas nativas." }
```

Si aparece `"gcc" no se reconoce` o `"g++" no se reconoce`, instala un toolchain GCC/G++ para Windows y abre una nueva terminal con su directorio `bin` en PATH. Repite las cinco suites: `test_backend_command_buffer`, `test_command_text_viewport`, `test_virtual_keyboard`, `test_editor_components` y `test_touch_gpio_ui`. Hasta que pasen, el resultado local es **ERRORED**, no PASS. También puedes consultar la ejecución CI del mismo commit en Ubuntu, dejando explícito que esa validación ocurrió en GitHub Actions.

## 5. Compilar el firmware de producción

```powershell
py -m platformio run -e panel_4848s040
if ($LASTEXITCODE -ne 0) { throw "Falló la compilación del firmware." }
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
py -m platformio device list
 # Opcional si System.IO.Ports está disponible en tu PowerShell:
[System.IO.Ports.SerialPort]::GetPortNames()
```

Anota el puerto, por ejemplo `COM5`. Sustituye `COM5` en los comandos siguientes.

## 7. Cargar el firmware

Cierra el monitor serie antes de cargar. El registro compartido muestra `COM9` para un firmware ejecutado desde `firmware-demo`; detecta de nuevo el puerto del panel actual. Sustituye el ejemplo `COM5` por el puerto identificado:

```powershell
$DevicePort = "COM5"
py -m platformio run -e panel_4848s040 -t upload --upload-port $DevicePort
if ($LASTEXITCODE -ne 0) { throw "Falló la carga; no la registres como validación física." }
```

El borrado completo es una recuperación opcional: elimina también NVS. Ejecútalo únicamente cuando necesites eliminar esa configuración, y vuelve a cargar después:

```powershell
py -m platformio run -e panel_4848s040 -t erase --upload-port $DevicePort
if ($LASTEXITCODE -ne 0) { throw "Falló el borrado." }
py -m platformio run -e panel_4848s040 -t upload --upload-port $DevicePort
if ($LASTEXITCODE -ne 0) { throw "Falló la carga después del borrado." }
```

## 8. Monitor serie después del flash

```powershell
py -m platformio device monitor --port $DevicePort --baud 115200
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

## 11. Evidencia revisada el 9 de octubre de 2026

| Evidencia | Resultado | Alcance |
|---|---|---|
| Checkout local del registro | `9a7d32b` | Sincronizado con main en ese momento |
| Seis contratos Python ejecutados | PASS | Estados, UI, Wi-Fi, editor, touch/GPIO y producción |
| Regresión del buffer | No ejecutada: ruta incorrecta | Repetir con `py .\test\command_buffer_regression.py` |
| Cinco suites nativas en Windows | ERRORED: faltan gcc/g++ | Instalar toolchain anfitrión y repetir |
| Compilación `panel_4848s040` | SUCCESS, 61.06 s | RAM 47 236 B; Flash 1 016 577 B |
| Serie desde `firmware-demo`, COM9 | health HTTP 200; DATABRICKS LISTO | Conectividad del firmware que estaba cargado |
| Upload de la compilación actual | No aparece en el TXT | Pendiente de documentar |
| [CI del commit 9a7d32b](https://github.com/wpv10barza/erp-mantto-esp32/actions/runs/37959355032) | success | Pruebas en runner Ubuntu |
| [Firmware CD del commit 9a7d32b](https://github.com/wpv10barza/erp-mantto-esp32/actions/runs/37959355023) | success | E2E software y build; no flasheo físico |

La respuesta health anuncia `protocol_version: "1.0"`, mientras el contrato del repositorio declara `1.1`. Registra ambas versiones; HTTP 200 no demuestra por sí solo paridad completa del protocolo, escritura en Google Sheets, precisión táctil ni aplicación de una orden.

## Recuperación rápida

Si el puerto no aparece: cambia cable USB, prueba otro puerto físico, mantén BOOT durante el inicio del upload si el panel lo requiere y vuelve a ejecutar `pio device list`. Si la compilación falla, no fuerces el flash: actualiza nuevamente `main`, ejecuta las pruebas y recompila.
