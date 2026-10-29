# FOTA 2.7.9 — GitHub HTTP -1: diagnóstico y recuperación segura

El release OTA 2.7.8 ya existe con firmware.bin y manifest.json; sin embargo, el ESP32 con 2.7.7 puede informar GitHub HTTP -1. Este código es un FALLO DE TRANSPORTE (DNS/TCP/TLS), NO HTTP 404 y NO una falla de Databricks.

## Qué mejora el firmware 2.7.9

- Verifica Wi-Fi y UTC sincronizada antes de HTTPS; no omite la validación X.509.
- Permite 15 s para establecer TCP, 30 s para TLS handshake y 30 s para recepción.
- Solo ante errores negativos, realiza UN reintento con una conexión TLS nueva. No reintenta respuestas HTTP 4xx/5xx.
- Registra errores de HTTPClient y mbedTLS, DNS, RSSI y heap en el serial USB a 115200 baudios.
- Conserva la lista de raíces de confianza, validación de manifiesto, full SHA, tamaño, SHA-256, rollback OTA y NVS.
- Mantiene los botones ACTIVAR / DEPLOY CLOUD y DESACTIVAR DATABRICKS con QR autenticado.

## Si el ESP32 sigue en firmware 2.7.7 / 2.7.8

1. Comprueba Wi-Fi 2.4 GHz conectado, IP real y hora UTC. Usa /health en la IP local desde móvil conectado a la misma red.
2. Comprueba en Termux que puedes descargar el manifiesto oficial desde https://github.com/wpv10barza/erp-mantto-esp32/releases/download/ota-3513e2b8/manifest.json. Esto valida el teléfono, no la placa.
3. Si la placa sigue dando HTTP -1, NO puede instalar la corrección 2.7.9 por FOTA mientras permanezca bloqueada la conexión HTTPS. Reintentar SHA no solucionará un bloqueo persistente.
4. Cuando exista release ota-<SHA8> de v2.7.9, instala ese SHA desde FOTA solo si publica firmware.bin y manifest.json; si persiste -1, usa recuperación USB con el firmware oficial y la tabla de particiones de 16MB. No ejecutar erase_flash ni borrar NVS.
5. Si persiste el problema en v2.7.9, copia solo la línea GITHUB FOTA MANIFEST ERROR attempt=... http=... tls=... dns=... wifi=... heap=... del monitor serial, sin secretos.

## Interpretación de los nuevos logs

- dns=FAIL: resolver DNS o conexión Internet del ESP32.
- dns=OK con error TLS/mBedTLS de certificado: revisar hora, CA y posible inspección TLS de la red.
- dns=OK con TLS sin código preciso: revisar TCP, Wi-Fi, redirecciones GitHub y memoria.
- GitHub HTTP 404: release/commit no encontrado, no un error DNS/TLS.

El archivo PEM del proyecto contiene cuatro certificados con estructuras DER completas; las pruebas verifican las longitudes. No se desactiva TLS ni se sustituyen certificados sin evidencia.
Databricks y GitHub FOTA son independientes. La opción DESACTIVAR detiene exclusivamente el cómputo de la App alumno mediante GitHub Actions autenticado.
