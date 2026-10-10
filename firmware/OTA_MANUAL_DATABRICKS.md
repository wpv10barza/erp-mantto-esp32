# OTA manual desde Databricks App

Se integró el flujo de verificación/instalación SHA-256 del repositorio
`wpv10barza/firmware-demo` en el firmware principal ESP32-4848S040.

## Primera instalación
Cambiar desde `default_16MB.csv` a `partitions_ota_16mb.csv`
requiere **un único borrado y flasheo USB COM9**. Antes de ello, respaldar
la configuración secreta `include/local_config.h` y comprobar la compilación.
No publicar como actualización OTA una imagen construida con la tabla anterior.

## Uso desde pantalla
Abrir **Actualizar firmware**. Pulsar **BUSCAR OTA** para consultar el
manifiesto sin modificar flash. Pulsar **INSTALAR** explícitamente para
comprobar de nuevo la versión, descargar el binario en la ranura inactiva,
validar tamaño y SHA-256 e iniciar reinicio. Se confirma el arranque después
de 15 segundos y se utiliza rollback del bootloader si falla la pantalla.

No hay actualización automática ni descarga desde GitHub al ESP32.

## Databricks App y Unity Catalog
La App `asistente-cloud-erp` debe enlazar:
- `ESP32_API_TOKEN` al secreto `esp32_api_token`;
- `OTA_VOLUME_PATH` a `ota_firmware_volume`;
- `OTA_CHANNEL` a `stable`.

El ESP32 envía `Authorization: Bearer <OAuth M2M>` y
`X-3C-Device-Token: <token privado>`. Ambos deben ser válidos.

Publicar tras pruebas bajo el Volume:
```
stable/
  latest.json                       # version, sha256 y size
  2.5.1/
    firmware.bin
    manifest.json                   # sha256 verificado
```
Usar **versiones estrictamente numéricas** (2.5.1, 2.5.2...);
el firmware embarcado compara contra `2.5.0`.
La URL de descarga se restringe al mismo host de Databricks App.
Publicar `latest.json` al final de cada despliegue verificado.
Un artefacto de GitHub Actions no se publica solo en Unity Catalog.

Ejemplo de manifiesto (generar el SHA-256 real, no copiar marcadores):
```json
{"version":"2.5.1","sha256":"<64-caracteres-hex-reales>","size":1234567}
```
Se requiere `firmware.bin` real, compatible con las dos ranuras de 4 MiB.

## Estado
Código fuente integrado para revisión. La verificación física por Wi-Fi y la
publicación real a Databricks **siguen pendientes**. No activar OTA en una
instalación productiva hasta que CI termine con éxito y se ensaye en el panel.
