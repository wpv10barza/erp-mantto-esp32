# FOTA de GitHub: error de Wi-Fi y hora HTTPS (2.7.7)

## Qué significa

El firmware anterior mostraba **FOTA requiere Wi-Fi y hora HTTPS** si fallaba alguna de estas condiciones:

1. El ESP32-S3 no estaba asociado a una red Wi-Fi con Internet.
2. El reloj UTC no había sido sincronizado por NTP. Esperaba únicamente unos 9 segundos (30 intentos de 300 ms), lo que podía ser insuficiente.

La versión **2.7.7** corrige el diagnóstico y comienza la sincronización UTC en segundo plano al conectarse al Wi-Fi; si todavía no hay hora, espera hasta ~30 segundos en el worker de red. La pantalla táctil no se bloquea. **No desactiva HTTPS ni la comprobación X.509 o SHA-256**, y FOTA no utiliza Databricks.

## Diagnóstico rápido sin modificar Databricks

1. En el monitor ESP32: **Wi-Fi 2.4 GHz**. Verifica **Estado: Conectado** e **IP local** (no «Sin IP»). Comprueba que el SSID corresponda a una red de **2.4 GHz** con acceso a Internet, no a una red cautiva.
2. Si el SSID está vacío o no hay conexión, revisa `include/local_config.h` (archivo ignorado de git) y credenciales NVS `ota3c`. El build de GitHub **no contiene credenciales privadas** por diseño.
3. Si el monitor está en la misma red que el PC/móvil, consulta `http://IP-ESP32/health`, reemplazando por su dirección local. Desde 2.7.7 informa `wifi` y `clock_utc_ready`.
4. Si `wifi=true` pero `clock_utc_ready=false`, revisa DNS, salida NTP (UDP 123), hora del router, y que los servidores `pool.ntp.org`, `time.google.com` o `time.cloudflare.com` sean accesibles. La hora UTC es indispensable para certificar HTTPS.
5. Si ambos campos están en `true` pero no descarga GitHub, registra el código `GitHub HTTP` o el error TLS por USB a 115200 baudios. Una red con portal cautivo, inspección TLS o redirecciones bloqueadas puede fallar, aunque el Wi-Fi esté conectado.

## Logs sin revelar claves

Con puerto USB conectado, abre un monitor serial a 115200 y observa mensajes con prefijos `FOTA WIFI ERROR:`, `FOTA NTP:`, `FOTA NTP ERROR:` y `GITHUB FOTA LOOKUP HTTP=`. No pegues logs que expongan datos privados o contraseñas.

## Instalación puente por USB

Si el monitor actual utiliza firmware 2.7.5/2.7.6, **esta corrección no puede alterar el código ya instalado**. Si FOTA sigue bloqueada, cargar 2.7.7 una vez por USB. El script oficial no borra NVS:

```powershell
Set-PSReadLineOption -HistorySaveStyle SaveNothing
cd C:\Users\ASUS\erp-mantto-esp32-deploy
git status --short
git pull --ff-only origin main
.\scripts\Repair-Alumno-Cloud-Local.ps1 -Apply
py .\scripts\verify_firmware_target.py
.\scripts\Flash-ESP32-3C.ps1 -ListPorts
.\scripts\Flash-ESP32-3C.ps1 -Port COM6 -SerialSeconds 30
```

Usar el puerto COM real. Revisar `git status --short` antes de actualizar, porque el script requiere árbol versionado limpio y rama `main`. No ejecutar `erase_flash`. Si falta `include/local_config.h`, no usar `-AllowMissingConfig` salvo que la NVS ya tenga credenciales válidas.

## Releases FOTA

GitHub Actions publica `ota-<SHA8>` con `firmware.bin` y `manifest.json` después de un build verificado en `main`. El commit solo será instalable por FOTA cuando el release exista y contenga ambos archivos. El ESP32 valida full SHA, hardware, tamaño y checksum SHA-256 antes de arrancar la nueva partición.

**Nota:** conexión FOTA (GitHub) y operación Cloud (Databricks) son rutas distintas. Una App Databricks detenida no impide FOTA. No hay que configurar el Application ID OIDC para actualizar desde GitHub.
