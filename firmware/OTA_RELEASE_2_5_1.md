# Release OTA 2.5.1 (ESP32-S3-4848S040)

La versión 2.5.1 incorpora la interfaz de selección por commit y el
protocolo OTA verificado. **No existe una instalación física realizada por
GitHub/ChatGPT**: el binario con credenciales solo puede compilarse en el
PC que dispone de \`include/local_config.h\` privado.

## Prerequisitos
1. Guardar todos los cambios locales y actualizar la rama
   \`feature/databricks-ota-manual\`.
2. Contar con PlatformIO (\`pio\`) y Databricks CLI autenticado bajo
   \`--profile asistente-cloud-erp\`.
3. Comprobar que \`include/local_config.h\` contiene valores reales de
   Wi-Fi, OAuth M2M y token de dispositivo.
4. Tener configurado en **la App** \`asistente-cloud-erp\` el UC Volume
   \`workspace.default.esp32_firmware\` con clave \`ota_firmware_volume\`
   y permiso **Can read**; el publicador debe tener permisos de escritura.
   El despliegue Databricks de la API OTA por commit debe estar activo.
5. No usar artefactos de GitHub Actions no provisionados en dispositivos
   que dependen de estas credenciales.

## Compilar y generar manifiesto (sin publicar)
En Windows PowerShell desde el repositorio:

\`\`\`powershell
.\scripts\prepare_publish_ota_2_5_1.ps1
\`\`\`

Se realizan comprobaciones del estado Git, versión 2.5.1,
configuración privada, build PlatformIO, imagen ESP32 (magic 0xE9),
límite de ranura 4 MiB, SHA-256 y origen por commit SHA **completo**.
El script crea:

\`\`\`text
ota-release/2.5.1/
  firmware.bin
  manifest.json
\`\`\`

\`ota-release/\` está ignorada por Git porque **firmware.bin contiene
credenciales**. No se debe adjuntar, compartir ni subir esa imagen a
artifacts públicos. La App solo debe exponer binarios a dispositivos
autorizados. Considerar rotar las credenciales expuestas anteriormente.

## Publicar una release de commit (sin alterar latest)
La primera vez se exige reconocimiento del riesgo de credenciales:

\`\`\`powershell
.\scripts\prepare_publish_ota_2_5_1.ps1 -Publish -AcknowledgeEmbeddedSecrets
\`\`\`

Esto escribe el binario en el UC Volume, lo vuelve a descargar al PC,
compara el SHA-256 remoto y **solo al final** publica el manifest.json.
No sobrescribe una versión 2.5.1 publicada anteriormente. Para promover
además a BUSCAR OTA (latest), y **solo después de ensayar la release**:

\`\`\`powershell
.\scripts\prepare_publish_ota_2_5_1.ps1 -Publish -AcknowledgeEmbeddedSecrets -PromoteLatest
\`\`\`

Si una release ya existe, no debe recompilarse/publicarse sobre esa ruta.
Crear una versión nueva o validar manualmente su integridad primero.
No permitir que otro usuario con acceso al volumen extraiga el binario.

## Instalar desde la pantalla
Usar **Actualizar firmware → COMMIT**, introducir los primeros 8 caracteres
del SHA reportado por el script (o 40 completos) y pulsar ENTER para
verificar. Si aparece \`OTA disponible 2.5.1\`, pulsar INSTALAR. El binario
se verifica con SHA-256 antes del reinicio, usando ranura OTA inactiva.

IMPORTANTE: el commit \`bbe98114\` es una revisión **2.5.0**,
no esta nueva compilación **2.5.1**; no puede identificarse falsamente
como la release nueva. Utilizar el SHA que muestre el script.
Para usar BUSCAR OTA, latest.json debe contener esta versión y su hash
verificado.

## Diagnóstico
- \`OTA_VOLUME_PATH no está configurado\`: asociar UC Volume a App
  con clave \`ota_firmware_volume\` y redesplegar.
- \`404 No existe firmware ... commit\`: release no publicada,
  prefijo incorrecto o SHA no coincide.
- \`404 Not Found\`: la ruta del backend no se desplegó.
- \`HTTP 200 GET health\` solo valida el backend, no OTA.
- \`FW 2.5.1 al dia\`: el firmware instalado no necesita esa versión.
- Nunca borrar flash/NVS para resolver un error HTTP del backend.

Para futuras versiones sin secretos en binario, implementar primero un
mecanismo seguro de provisión y persistencia local de credenciales.
