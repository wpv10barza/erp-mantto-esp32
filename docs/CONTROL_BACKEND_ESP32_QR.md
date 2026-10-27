# Conexión al backend desde ESP32-S3 — versión 2.7.6

## Resultado concreto

La opción táctil **Conexion al backend** ahora incluye:

1. Estado **Databricks alumno: CONECTADO / SIN VERIFICAR**, según respuesta reciente del Device API.
2. Entrada táctil grande **ACTIVAR / DEPLOY CLOUD**.
3. Al tocar, un QR que enlaza al workflow GitHub autenticado `Alumno Cloud | Control rapido`. El docente elige **status**, **start**, **redeploy** o **stop** en GitHub Actions. No necesita mantener el PC encendido.
4. Botón **PROBAR CLOUD** que lanza una consulta de salud del Device API desde el ESP32, tras activar la App, y **VOLVER**.

El monitor **no inicia Databricks por sí mismo**: solamente facilita la orden iniciada por el profesor tras autenticarse. Esto es deliberado: guardar tokens de GitHub con permiso de deploy o permisos de control Databricks en NVS (no cifrada por defecto) permitiría a cualquier persona con acceso físico iniciar recursos facturables.

## Verificar si el microcontrolador físico tiene esta versión

En la pantalla `Actualizar firmware` aparece `Version: 2.7.6`. Alternativamente, con el ESP32 y el teléfono/PC en el mismo Wi-Fi:

```powershell
# Sustituye la dirección por IP LOCAL mostrada en Wi-Fi 2.4 GHz del ESP32.
Invoke-RestMethod http://192.0.2.10/health
```

El campo `fw_version` debe ser `2.7.6` y `fw_commit` corresponder al release OTA publicado. La dirección `192.0.2.10` es **solo un marcador de ejemplo**, no la IP del dispositivo.

Si no se ve el botón o aparece `2.7.5` o inferior, **falta instalar el firmware**. Un commit exitoso de GitHub y un Release publicado **no cambian automáticamente la pantalla física**. Una actualización FOTA requiere que el dispositivo ya tenga el cliente GitHub FOTA (2.7.5), Wi-Fi y particiones OTA; si no, USB con `Flash-ESP32-3C.ps1`.

## Activación guiada

1. Pulsar `Conexion al backend`, `ACTIVAR / DEPLOY CLOUD`.
2. Escanear QR con **móvil del profesor**. Iniciar sesión en GitHub con autorización para ese repo y su GitHub environment `prod`.
3. Entrar en **Run workflow**, seleccionar `start` (activa despliegue actual) o `redeploy` (actualiza código del backend desde Git main); ejecutar.
4. GitHub Actions requiere las variables `DATABRICKS_HOST` y `DATABRICKS_CLIENT_ID` del entorno `prod`, la federación OIDC y el principal de servicio con `CAN MANAGE` sobre la App `asistente-cloud-erp-alumno`. El workflow verifica que NO sea el workspace original.
5. Al aprobarse en GitHub y comenzar la App, pulsar `PROBAR CLOUD` en ESP32. En fallos comprobar Wi-Fi, OAuth M2M del dispositivo, token API, permisos y recursos vinculados en Databricks.

El QR es fijo. Nunca contiene tokens ni secretos. Los alumnos pueden consultar el portal web `/acceso` si tienen `CAN_USE`; no deben disponer de permisos de despliegue.

**No ejecutar acciones START/DEPLOY sin revisar crédito y presupuesto de 14 días.** La App detenida no puede recibir órdenes desde su propio endpoint, y el simple toque del QR tampoco arranca la App.

## GitHub-only FOTA independiente

El cambio de interfaz 2.7.6 **no modifica** el transporte FOTA de 2.7.5: release `ota-<sha8>` en GitHub con `firmware.bin`, `manifest.json` y verificación TLS, tamaño, hardware, SHA-256 y slots OTA de 16MB. No se gasta crédito Databricks para actualizar firmware.

Si GitHub Actions publica un release OTA posterior, en `Actualizar firmware` se introduce el SHA corto de ese release publicado.
