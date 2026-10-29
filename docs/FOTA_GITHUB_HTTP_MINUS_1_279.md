# FOTA 2.7.9 — diagnóstico GitHub HTTP -1 y conexión segura

## Evidencia

El ESP32-S3 mostró firmware **2.7.7**, commit **d400e8c1**, solicitó el release **3513e2b8** (2.7.8) y recibió **GitHub HTTP -1**. El release sí existe y contiene los dos assets. En la biblioteca HTTPClient usada por ESP32, `-1` es **HTTPC_ERROR_CONNECTION_REFUSED**, un fallo de establecer conexión (TCP/DNS/TLS), no un HTTP 404 ni rechazo del SHA.

La nueva **2.7.9** no presupone la causa física. Añade:
- un reintento máximo si el código es `-1` y Wi-Fi sigue conectado; ambos intentos mantienen la verificación de certificados X.509, SNI y redirecciones HTTPS;
- límites explícitos de conexión (18 s) y de handshake TLS (25 s) y registros seriales de DNS para `github.com`, error mbedTLS, señal Wi-Fi (RSSI), memoria disponible y UTC;
- campos de diagnóstico locales en `/health`: `fota_last_http`, `fota_tls_error`, `fota_dns_github` (1=resuelve, 0=no, -1=no probado), `fota_attempt`, `clock_utc_ready`. No se incluyen tokens privados, contraseñas ni credenciales de Databricks.

**IMPORTANTE:** publicar un nuevo commit no instala código en un ESP32 que no puede conectarse por HTTPS. Si el firmware 2.7.7 falla permanentemente con `-1`, 2.7.9 podrá requerir **un USB de recuperación sin `erase_flash`** para preservar la NVS. No deshabilitar TLS con `setInsecure()`.

## Diagnóstico desde Termux (mismo Wi-Fi del dispositivo)

Ver en el ESP32 la IP local y reemplazar la variable con la IP real:

```bash
ESP_IP="192.168.1.100"
curl --connect-timeout 5 -fsS "http://$ESP_IP/health" |
  jq '{fw_version,fw_commit,wifi,clock_utc_ready,fota_last_http,fota_tls_error,fota_dns_github,fota_attempt}'
```

En 2.7.7 solo se verán `wifi` y `clock_utc_ready`; los nuevos campos aparecerán después de actualizar a 2.7.9.

En la misma red, confirmar que el teléfono puede descargar el manifiesto de 2.7.8:

```bash
curl -fLsS --retry 2 --connect-timeout 10 --max-time 45 \
  "https://github.com/wpv10barza/erp-mantto-esp32/releases/download/ota-3513e2b8/manifest.json" |
  jq '{version,source_sha,size,hardware}'
```

Esta prueba solo demuestra conectividad del teléfono; **no confirma** la conectividad TLS del ESP32. Si el ESP32 funciona con un hotspot de 2.4 GHz pero no con el router, revisar DNS, red cautiva, cortafuegos y acceso saliente TCP/443. Si los logs dan error de CA/certificado, revisar cadena TLS y hora NTP, sin `setInsecure()`.

## Comprobación de la publicación

El commit para instalar 2.7.9 debe corresponder a un release real `ota-<SHA8>`, con archivos `firmware.bin` y `manifest.json` publicados **después del CI de Firmware CD**. No confundir una PR aprobada o un commit main con una OTA disponible. `Databricks Apps` no participa en la actualización.

Para control de Databricks desde la pantalla, 2.7.9 conserva los botones `ACTIVAR / DEPLOY CLOUD` y `DESACTIVAR DATABRICKS` de la versión 2.7.8, con QR y autenticación obligatoria del profesor en GitHub Actions.
