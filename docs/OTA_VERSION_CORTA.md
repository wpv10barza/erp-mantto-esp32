# OTA con versión corta — ESP32-S3-4848S040

El panel muestra `2.7.4` y permite seleccionar un **SHA abreviado de 7 a 12 caracteres** (ejemplo: `e38bbd3`), en vez de teclear el commit completo de 40. El SHA de 40 permanece **dentro del binario y en `manifest.json`**, para relacionar firmware, código fuente y checksum SHA-256.

## Antes de intentar OTA

1. Hardware objetivo: panel **ESP32-S3-4848S040 con flash física de 16 MB**. Compruebe la capacidad de la placa.
2. La partición `partitions_ota_16mb.csv` usa dos ranuras. Si el panel está en la variante antigua sin ranuras OTA, **la primera migración exige una grabación USB** de bootloader, particiones y aplicación. Un error `COM5 no existe` no se resuelve con GitHub Actions.
3. El firmware inicial incorpora los datos privados de conexión de manera local; posteriores builds de CI **no** incorporan secretos de usuario y recuperan credenciales ya aprovisionadas desde NVS. La NVS no está cifrada por defecto.
4. Para un dispositivo nuevo, no suponga que el artefacto genérico de GitHub Actions contiene SSID, OAuth o token de dispositivo. Configure un aprovisionamiento autorizado antes de depender de OTA.

## Publicar desde GitHub Actions

Repositorio: `wpv10barza/erp-mantto-esp32` → **Actions** → **Firmware CD**.

- Al hacer push o PR, el job `firmware` ejecuta pruebas y compila el `.bin`, sin grabar la placa ni promover ningún release OTA.
- Para publicar una vez aprobado el cambio en `main`, use **Run workflow** con `publish_to_databricks=true` y `release_version=2.7.4`. La versión proporcionada se compara con la compilada. No se permite publicar un binario de versión distinta.
- Configure las variables de GitHub en el entorno `prod`: `DATABRICKS_HOST`, `DATABRICKS_CLIENT_ID` y `OTA_VOLUME_PATH` (ruta raíz de UC Volume `/Volumes/<catalog>/<schema>/<volume>`). El flujo exige explícitamente el workspace de la alumna `https://dbc-77f7771e-2520.cloud.databricks.com`; no despliega al workspace original.
- La identidad de GitHub Actions debe tener OIDC autorizado y permisos para escribir en el volumen destino. La Databricks App debe estar vinculada al volumen con permiso de lectura y `OTA_VOLUME_PATH` configurado.
- La publicación crea `stable/<version>/firmware.bin` y `manifest.json` y, **solo al final**, promociona `stable/latest.json`. Una versión ya existente falla en lugar de sobrescribirse. Las carpetas parcialmente publicadas requieren revisión administrativa antes de reintentar.

El resumen de GitHub Actions muestra la **versión corta** y los primeros 8 caracteres del commit. El manifiesto conserva `source_sha` (40), `size` y `sha256`, como protección de integridad.

## Actualizar desde la pantalla

1. En el panel que **ya tenga particiones OTA**, entre a **Actualizar firmware** → **EDITAR SHA**.
2. Escriba entre **7 y 12 caracteres hexadecimales** del commit publicado y pulse **ENTER**.
3. El panel consulta `GET /api/device/v1/firmware/by-commit/{sha_corto}`. Databricks devuelve un release solo si ese prefijo identifica **un único firmware publicado para este hardware**. No construye commits arbitrarios ni selecciona la última versión al fallar la búsqueda.
4. Revise la versión indicada. Pulse **INSTALAR OTA**, luego **CONFIRMAR**. El firmware verifica tamaño y SHA-256; requiere la ranura OTA inactiva.
5. Compruebe visualización, tacto, versión, Wi-Fi, autenticación y conectividad del Asistente 3C después del reinicio.

Si el SHA corto es ambiguo, pruebe con 9–12 caracteres. Si persiste, publique una versión sin ambigüedad y revise el backend. Una nueva versión de backend de Databricks **no implica** actualizar el firmware físico.

## Límites deliberados

- No se ha efectuado ningún flash USB ni OTA físico mediante estos cambios de Git.
- El nombre de versión corto identifica el release; el SHA-256 completo identifica los bytes exactos del archivo.
- El token compartido de dispositivo existente no equivale todavía a aprovisionamiento multiusuario; implementarlo requiere un desarrollo separado.

## Acceso académico desde el ESP32 (2.7.4)

- El desplegable **Conexion al backend** presenta disponibilidad y explica que los alumnos ingresan por el portal web SSO de Databricks. No pide cuentas o contrasenas en pantalla.
- En el navegador del mismo Wi-Fi del panel, `http://IP-DEL-ESP32/` muestra el enlace **ABRIR PORTAL ALUMNO / PROFESOR**. La ruta local `/acceso` redirige al portal remoto `https://asistente-cloud-erp-alumno-7474657268940105.aws.databricksapps.com/acceso`.
- `PROBAR CLOUD` sigue siendo un diagnostico autenticado de dispositivo; **no inicia la App**, no modifica creditos ni da permiso administrativo.
- Si la App esta detenida, el profesor autorizado la inicia desde [Alumno Cloud | Control rapido](https://github.com/wpv10barza/asistente-de-databricks--erp/actions/workflows/alumno-cloud-control.yml) mediante GitHub OIDC. No se necesita una PC encendida.
- Configurar primero usuarios y ACL de Databricks, y recurso UC Volume `ota_firmware_volume`; el enlace SSO no evita los requisitos de acceso al backend.
- Tras build `2.7.4` en `main`, publicar desde Firmware CD con `publish_to_databricks=true`, `release_version=2.7.4`; verificar `stable/2.7.4/manifest.json` y `firmware.bin` antes de teclear los primeros 8 caracteres del commit final en **EDITAR SHA**.
- **Importante:** la migracion inicial desde firmware con particiones sin OTA exige un flash USB. El paquete OTA generico no contiene secretos; requiere credenciales previamente aprovisionadas en NVS.
