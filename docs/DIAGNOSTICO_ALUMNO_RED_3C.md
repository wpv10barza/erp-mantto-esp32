# Diagnóstico ESP32 3C — App independiente de la alumna

Esta reparación corrige **solo los valores predeterminados públicos** de la compilación del firmware ESP32-S3-4848S040:

- App: `https://asistente-cloud-erp-alumno-7474657268940105.aws.databricksapps.com`
- Workspace OAuth: `https://dbc-77f7771e-2520.cloud.databricks.com`
- Versión de firmware: `2.7.3`

No altera la App original, sus permisos ni credenciales de ningún usuario. No copia ni publica tokens.

## Por qué la pantalla mostraba «Flujo: error de red»

En la versión anterior `setTransportError(...)` mostraba **el mismo mensaje** para un rechazo HTTP 401/403/404 y para un fallo de red negativo. No podemos asegurar con una foto qué código fue realmente devuelto. Ahora los códigos se diferencian en la pantalla: 401 autenticación, 403 permisos, 404 ruta, 5xx servidor y código negativo red/TLS/DNS. La línea serie `[ERROR] transport phase=POST|POLL code=...` no divulga el cuerpo de la respuesta.

**Importante:** `include/local_config.h` es privado y tiene precedencia sobre `include/app_config.h`. Un `local_config.h` existente puede conservar las URLs antiguas incluso después de un `git pull`; modificar GitHub por sí solo **no** altera el ESP32 ya grabado.

## Comprobar URLs privadas sin exponer secretos (PowerShell)

~~~powershell
cd C:\Users\ASUS\erp-mantto-esp32-deploy
git status --short
git log -1 --oneline

$ArchivoPrivado = ".\include\local_config.h"
if (Test-Path $ArchivoPrivado) {
    Select-String -Path $ArchivoPrivado -Pattern '^\s*#define\s+(ASSISTANT_BASE_URL_VALUE|DATABRICKS_WORKSPACE_URL_VALUE)\s+"https://[^"]+"' |
        ForEach-Object { $_.Matches[0].Value }
} else {
    Write-Host "No hay local_config.h: la compilacion toma las URLs predeterminadas de app_config.h"
}
~~~

No imprima `DATABRICKS_CLIENT_SECRET_VALUE`, `ESP32_API_TOKEN_VALUE`, `WIFI_PASSWORD_VALUE` ni el contenido completo de `local_config.h`. Corrija **solo** las dos URLs públicas si fueran antiguas; preserve las credenciales privadas fuera de Git.

## Diagnóstico del ESP32 en ejecución

Identifique primero el puerto USB de datos que Windows detecta:

~~~powershell
.\scripts\Flash-ESP32-3C.ps1 -ListPorts
py -m platformio device list
~~~

Con el puerto real (ejemplo **COM7**, no asumir COM5):

~~~powershell
py -m platformio device monitor --port COM7 --baud 115200
~~~

El firmware v2.7.3 imprimirá `CLOUD TARGET app=... oauth_host=...`, que revela únicamente hosts públicos. En cualquier firmware que ya tenga logs de transporte, observe `[ERROR] transport phase=POST code=...` o `POLL`. Comparta esas líneas, **no tokens, cuerpo de la respuesta ni archivos privados**.

### Interpretación del código

- **401**: revisar OAuth M2M y el `X-3C-Device-Token` que la nueva App verifica; puede provenir de la capa App o de la Device API.
- **403**: principal de servicio sin autorización suficiente sobre la App o endpoint.
- **404**: endpoint ausente o backend no desplegado en esta App; también puede ser un command-id ya vencido en el polling.
- **5xx**: fallos de servicio, App o configuración; consultar logs en perfil `alumno-3c`.
- **negativo**: problema de DNS, TLS o transporte; comprobar Wi-Fi/Internet, hora TLS y conectividad.

El principal OAuth utilizado por la ESP32 debe estar creado/autorizado en el workspace de la alumna, y el token de dispositivo debe coincidir con `ESP32_API_TOKEN` configurado mediante un recurso secreto de la App. **El usuario Windows/Databricks `polovaleriow@gmail.com` no es automáticamente la identidad OAuth usada por el ESP32**.

## Revisar la App sin tocar el workspace original

~~~powershell
$Perfil = "alumno-3c"
$App = "asistente-cloud-erp-alumno"

databricks apps get $App --profile $Perfil --output json |
    ConvertFrom-Json |
    Select-Object name, url, compute_status, app_status, active_deployment

databricks apps logs $App --tail-lines 100 --profile $Perfil
~~~

Compruebe que el compute esté `ACTIVE`, la aplicación ejecutándose y que la versión de FastAPI desplegada contenga `POST /api/device/v1/commands`. No confunda la URL asignada con un despliegue que funciona.

## Siguientes pasos

1. Corrija las dos URLs públicas del archivo **local privado** (si tiene override), y verifique que los secretos OAuth y `ESP32_API_TOKEN` correspondan a la **alumna**. No copie credenciales del workspace original.
2. Antes de grabar, identifique el puerto real y compruebe que el panel tiene **16 MB** y la partición OTA compatible.
3. Solo después de pruebas de CI y configuración adecuada, instale firmware mediante `./scripts/Flash-ESP32-3C.ps1 -Port COMx`. No use `erase` salvo recuperación expresamente necesaria.
4. Los builds de GitHub Actions usan archivos sin secretos; una OTA posterior solo funcionará si el ESP32 conserva credenciales correctas en NVS y el backend de la alumna autoriza ese principal.


## Reparacion automatizada local en Windows y CI verificada

El fichero privado include/local_config.h es ignorado por Git y prevalece
sobre los defaults del repositorio. Las dos lineas antiguas capturadas en
PowerShell no cambian por ejecutar git pull.

Tras integrar la PR a main, ejecute:

~~~powershell
cd C:\Users\ASUS\erp-mantto-esp32-deploy
git status --short
git pull --ff-only origin main

# Solo lee las dos URLs publicas, no muestra tokens.
.\scripts\Repair-Alumno-Cloud-Local.ps1

# Corrige los overrides publicos y guarda respaldo privado.
.\scripts\Repair-Alumno-Cloud-Local.ps1 -Apply

# Rechaza cualquier URL que no apunte al workspace de la alumna.
py .\scripts\verify_firmware_target.py
~~~

Este reparador no cambia ni fabrica OAuth Client ID, Client Secret, Wi-Fi
SSID o password ni ESP32_API_TOKEN. Esas credenciales deben corresponder al
workspace de la alumna y ser autorizadas expresamente. La copia de respaldo
queda bajo %LOCALAPPDATA%\Proyecto3C-Alumno\Privado\Backups, fuera del Git.

### Protecciones y preflight antes de publicar OTA

- CI verifica URLs, script local y rechazo de versiones con destino viejo.
- Firmware CD compila el panel, verifica que el firmware.bin real incluya
  ambos destinos correctos y no contenga los hostnames originales. Incluye
  manifest.json con version corta, source_sha de 40 y SHA-256 del binario.
- Solo workflow_dispatch desde main permite publicar; use
  publish_to_databricks=true y release_version=2.7.3 con entorno prod.
- GitHub OIDC debe estar autorizado sobre el workspace de la alumna, con
  permisos al UC Volume; la App debe tener recurso OTA de lectura vinculado.
  Antes de modificar latest.json, el job comprueba que la App
  asistente-cloud-erp-alumno esta ACTIVE/RUNNING y vinculada al repositorio
  backend esperado.
- No sobrescribir releases: estable/latest.json se actualiza al final.
  La carpeta de version ya publicada produce error en vez de reemplazar.

### Limite fisico imprescindible

Si el ESP32 instalado actualmente apunta al servidor original y no puede
autenticarse/obtener OTA desde ese servidor, un release publicado SOLO en el
workspace de la alumna no sera visible para ese firmware. Lo mismo ocurre
si el dispositivo no tiene tabla de particiones OTA duales de 16 MB.
Necesita instalar una vez por USB desde Windows con su puerto COM real.

GitHub Actions construye/publica firmware; no abre el puerto USB de un PC
externo ni cambia por si solo la configuracion instalada en el ESP32.


### Auditoria de repositorio backend y permisos de publicacion

Existe otra discrepancia independiente: el workflow de GitHub Actions
del repositorio wpv10barza/asistente-de-databricks--erp (archivo
.github/workflows/deploy-databricks.yml) comprueba una App llamada
asistente-databricks-erp, NO asistente-cloud-erp-alumno. Ese workflow
no demuestra que la App de la alumna tenga backend desplegado; no lo
ejecute asumiendo que apunta al workspace alumno.

En el perfil correcto, si ya existe la App ACTIVE y sus recursos
secretos/volumen estan debidamente vinculados, use la ruta explicita:

~~~powershell
databricks apps get asistente-cloud-erp-alumno --profile alumno-3c --output json
databricks apps deploy asistente-cloud-erp-alumno --git-branch main --profile alumno-3c --timeout 20m
databricks apps list-deployments asistente-cloud-erp-alumno --profile alumno-3c
~~~

No publique OTA mientras la App no aparezca RUNNING con la Device API
adecuada. En GitHub, el entorno prod del repositorio firmware necesita
las variables DATABRICKS_HOST (workspace alumna),
DATABRICKS_CLIENT_ID (principal OIDC de GitHub Actions) y
OTA_VOLUME_PATH (UC Volume valido); son distintas de las credenciales
M2M que utiliza el ESP32. La ausencia de cualquiera impide
la publicacion. No agregue secretos en texto plano a estas variables.

La App FastAPI debe tener acceso de lectura al recurso
ota_firmware_volume configurado en app.yaml y ESP32_API_TOKEN
vinculado al mismo valor esperado por la credencial individual o
provisionada en NVS. No utilice ALLOW_INSECURE_DEVICE_API para
eludir el fallo de autorizacion.
