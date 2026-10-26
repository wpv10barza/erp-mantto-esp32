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
