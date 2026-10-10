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

## Historial de órdenes 3C y cambios aplicados en Sheets

Desde la versión `2.6.0-history-3c`, el panel 4848S040 tiene el botón
**HISTORIAL** arriba a la derecha tanto en **INICIO** como en
**EDITAR ORDEN 3C**. El menú tiene dos pestañas:

- **ORDEN 3C:** texto enviado, fecha UTC, estado real y una vista previa
  de celdas si la propuesta se vinculó a la orden.
- **SHEETS:** únicamente cambios registrados *después* de que la API de
  Google Sheets respondió a una escritura; presenta celda, valor nuevo y
  referencia de la orden o su origen web.

Use **ACTUALIZAR** para consultar la nube en segundo plano; **ANT/SIG**
para paginar y toque una entrada para ver más detalles. **VOLVER**
regresa a la pantalla desde la que se abrió el historial. Una orden
`pending_confirmation` nunca se muestra como un cambio ejecutado.

**Consultar desde PowerShell** después de flashear en el puerto
que Windows detecte (por ejemplo COM9):

```powershell
py -m platformio device monitor --port COM9 --baud 115200
```

Al pulsar ACTUALIZAR, la consola registra una línea como
`HISTORY 3C -> 2 orders, 1 Sheets changes`.
Esas cifras proceden del backend: **un HTTP 200 con
`pending_confirmation` no equivale a modificar la hoja**.

**Consultar la API desde PowerShell (sin cambiar Sheets):** se necesitan
dos credenciales distintas: Bearer OAuth de Databricks Apps y
`X-3C-Device-Token`. Ambas deben obtenerse mediante un mecanismo privado,
no guardarse en scripts versionados. Ejemplo cuando ya están disponibles
en variables de entorno del proceso:

```powershell
$Base = "https://asistente-cloud-erp-7474651957738908.aws.databricksapps.com"
$Headers = @{
  Authorization = "Bearer $env:DATABRICKS_ACCESS_TOKEN"
  "X-3C-Device-Token" = $env:ESP32_API_TOKEN
}
Invoke-RestMethod -Method GET -Uri "$Base/api/device/v1/history?limit=8" -Headers $Headers |
  ConvertTo-Json -Depth 8
```

**Persistencia importante:** en el backend, `HISTORY_LOG_PATH` debe
apuntar a una ruta real de un volumen persistente de Databricks Apps
para conservar historial entre reinicios. Sin esa configuración, el
historial solo cubre la sesión de ejecución actual. Las órdenes
anteriores a la incorporación de estos endpoints no se reconstruyen
automáticamente. El despliegue de la nueva app Databricks y el
flasheo físico son pasos independientes de subir los commits a GitHub.



## OTA con campo GitHub commit (v2.7.0-ota-commit)

**Se recuperó el campo táctil dentro de `Actualizar firmware`.**
En el panel principal, abra **Actualizar firmware** y pulse **EDITAR SHA**.
Escriba un SHA de GitHub de 7 a 40 caracteres hexadecimales con el
teclado en pantalla; **ENTER** consulta al backend Databricks de forma
asíncrona. También puede pulsar **BUSCAR OTA** para repetir la búsqueda.

El backend usa la ruta autenticada
`GET /api/device/v1/firmware/by-commit/{sha}`. No compila
automáticamente commits de GitHub ni acepta archivos desconocidos:
solo devuelve un firmware **ya publicado en un Unity Catalog Volume**,
con metadatos `source_sha`, `version`, `size` y `sha256`.
Cuando está disponible, el panel indica la versión. Para flashear,
pulse **INSTALAR OTA** y después **CONFIRMAR**. La descarga y
verificación SHA-256 se hacen en la tarea de red, no bloquean el teclado,
y se escriben en la ranura OTA inactiva. Un SHA no publicado mostrará
`Commit sin OTA publicada`; no se instalará el firmware de otro commit.

### Migración obligatoria desde la versión 2.6 mostrada en la foto

La interfaz **2.6.0-history-3c** utilizaba
`board_build.partitions = default_16MB.csv` y no tiene dos ranuras OTA.
Por tanto **no puede actualizarse directamente por aire a 2.7**:
necesita **una instalación de arranque por USB** que incluya
`bootloader.bin`, `partitions.bin` y `firmware.bin` con la nueva tabla
`partitions_ota_16mb.csv`.

1. Antes de compilar localmente, configure
   `include/local_config.h` (excluido de Git). Nunca publique contraseñas.
2. Ejecute `py -m platformio run -e panel_4848s040` y posteriormente
   `py -m platformio run -e panel_4848s040 -t upload --upload-port COM9`,
   confirmando primero el puerto conectado.
3. Al arrancar, **2.7** guarda la configuración provista en NVS (namespace
   `ota3c`) para que las siguientes imágenes OTA, incluso si se compilan
   sin `local_config.h`, puedan seguir conectadas.

**Seguridad:** NVS de Arduino no está cifrada por defecto.
En un equipo accesible a terceros, habilite las protecciones de
flash del ESP32 según su política antes de guardar credenciales.

### Requisitos de publicación

`Firmware CD` genera artefactos, **no los publica automáticamente
en un Volume de Databricks**. Para resolver un SHA debe existir
`<OTA_VOLUME_PATH>/<OTA_CHANNEL>/<version>/firmware.bin`
y el archivo `manifest.json` que vincula exactamente el
`source_sha` con el binario SHA-256. La App Databricks necesita el
Volume asignado y `OTA_VOLUME_PATH` configurado.

Un `databricks apps deploy ... --git-commit ...` solo cambia el
backend, no actualiza el ESP32. Instale una nueva versión semántica
(superior a 2.7.0) desde una imagen compilada para el hardware y
particiones OTA correctos. Verifique antes de publicar que **el
siguiente arranque no perderá las credenciales NVS**.
