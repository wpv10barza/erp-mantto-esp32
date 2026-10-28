# Conexión al backend desde ESP32-S3 — versión 2.7.8

## Resultado concreto

La opción táctil **Conexion al backend** ahora incluye:

1. Estado **Databricks alumno: CONECTADO / SIN VERIFICAR**, según respuesta reciente del Device API.
2. Entrada táctil grande **ACTIVAR / DEPLOY CLOUD**.
3. Boton tactil **ACTIVAR / DEPLOY CLOUD**: muestra un QR al workflow GitHub autenticado. El profesor elige **start** (o redeploy si necesita actualizar backend).
4. Boton tactil NUEVO **DESACTIVAR DATABRICKS**: muestra el mismo QR pero con instrucciones explicitas de elegir **stop**. Es una orden manual del profesor desde GitHub Actions, no una llamada directa desde ESP32. La ejecucion termina el compute de **esa App alumna**. NO apaga el workspace ni otros servicios.
5. No hace falta mantener el PC encendido.
6. Boton **PROBAR CLOUD** que lanza una consulta de salud del Device API tras activar la App, y **VOLVER**.

El monitor **no inicia ni detiene Databricks por sí mismo**: solamente facilita la orden iniciada por el profesor tras autenticarse. Esto es deliberado: guardar tokens de GitHub con permiso de deploy o permisos de control Databricks en NVS (no cifrada por defecto) permitiría a cualquier persona con acceso físico iniciar recursos facturables.

## Verificar si el microcontrolador físico tiene esta versión

En la pantalla `Actualizar firmware` aparece `Version: 2.7.8`. Alternativamente, con el ESP32 y el teléfono/PC en el mismo Wi-Fi:

```powershell
# Sustituye la dirección por IP LOCAL mostrada en Wi-Fi 2.4 GHz del ESP32.
Invoke-RestMethod http://192.0.2.10/health
```

El campo `fw_version` debe ser `2.7.8` y `fw_commit` corresponder al release OTA publicado. La dirección `192.0.2.10` es **solo un marcador de ejemplo**, no la IP del dispositivo.

Si no se ve el botón o aparece `2.7.5` o inferior, **falta instalar el firmware**. Un commit exitoso de GitHub y un Release publicado **no cambian automáticamente la pantalla física**. Una actualización FOTA requiere que el dispositivo ya tenga el cliente GitHub FOTA (2.7.5), Wi-Fi y particiones OTA; si no, USB con `Flash-ESP32-3C.ps1`.

## Activación guiada

1. Pulsar `Conexion al backend`, elegir `ACTIVAR / DEPLOY CLOUD` o `DESACTIVAR DATABRICKS`.
2. Escanear QR con **móvil del profesor**. Iniciar sesión en GitHub con autorización para ese repo y su GitHub environment `prod`.
3. Entrar en **Run workflow**, seleccionar **start** para encender la App existente, o **stop** para apagar COMPLETAMENTE el compute de la App alumna sin apagar el workspace. Solo usar **redeploy** cuando se necesite actualizar el backend (puede consumir compute). Ejecutar y esperar **SUCCESS** en GitHub; no confundir la pantalla del QR con la confirmacion de la accion.
4. GitHub Actions fija el host publico del alumno dentro del YAML y lee `DATABRICKS_CLIENT_ID` del entorno `prod`; requiere federacion GitHub OIDC y principal `github-actions-alumno-3c` autorizado con `CAN MANAGE`. Nunca usar el host del workspace original.
5. Solo despues de `start` exitoso, pulsar `PROBAR CLOUD` en ESP32. En fallos comprobar Wi-Fi, OAuth M2M del dispositivo, token API, permisos y recursos vinculados en Databricks.

El QR es fijo. Nunca contiene tokens ni secretos. Los alumnos pueden consultar el portal web `/acceso` si tienen `CAN_USE`; no deben disponer de permisos de despliegue.

**No ejecutar acciones START/DEPLOY sin revisar crédito y presupuesto de 14 días.** La App detenida no puede recibir órdenes desde su propio endpoint, y el simple toque del QR tampoco arranca la App.

## GitHub-only FOTA independiente

El cambio de interfaz 2.7.8 **no modifica** el transporte FOTA de 2.7.5: release `ota-<sha8>` en GitHub con `firmware.bin`, `manifest.json` y verificación TLS, tamaño, hardware, SHA-256 y slots OTA de 16MB. No se gasta crédito Databricks para actualizar firmware.

Si GitHub Actions publica un release OTA posterior, en `Actualizar firmware` se introduce el SHA corto de ese release publicado.

## Version 2.7.8 — Seguridad de DESACTIVAR

El nuevo control no guarda tokens administrativos de GitHub, Databricks ni credenciales de profesor en el dispositivo. El QR lleva al workflow autenticado y el modo DESACTIVAR muestra claramente `Run workflow > stop`. **STOP** detiene solo el compute de la App `asistente-cloud-erp-alumno`; no cancela suscripciones, credenciales, workspace, Google Sheets ni otros recursos. Verifique en GitHub Actions que el job `stop` termino con `SUCCESS` y que el estado de compute es `STOPPED` (o consulte `status`). El firmware no debe mostrar `STOPPED` solo porque se pulso el boton.

**OTA**: introducir el commit corto del release 2.7.8 publicado cuando exista y contenga `firmware.bin` y `manifest.json`. GitHub FOTA por HTTPS no depende de que Databricks este encendido. Nunca usar `setInsecure()` ni borrar NVS para esta actualizacion.
