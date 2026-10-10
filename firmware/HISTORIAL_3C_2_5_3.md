# Historial 3C y revisión antes de enviar (2.5.3)

Al pulsar ENVIAR 3C se abre el editor original. ENTER ya no envía:
presenta la pantalla **REVISAR ORDEN 3C** con EDITAR, INICIO y ENVIAR.
Solamente un toque explícito en ENVIAR ejecuta POST 3C.

El menú inicial incluye **Historial 3C**. Se muestran los comandos
recientes del dispositivo, una entrada por página, con estado
PENDIENTE WEB / APLICADO / RECHAZADO y las celdas y valores nuevos
devueltos por el backend **solo después** de confirmar su aplicación.
Opciones: ANTERIOR, SIGUIENTE, ACTUALIZAR.

El backend debe desplegar primero el endpoint:
GET /api/device/v1/commands/history?device_id=...&offset=0
que exige token de dispositivo. No existe escritura directa desde la
pantalla y la aprobación humana sigue siendo obligatoria.

**Alcance importante:** el almacenamiento de comandos del backend es
solo temporal (15 minutos, máximo 50 entradas) y se pierde al
reiniciar Databricks. Esto es un historial reciente de estados y
resultados, no un registro de auditoría persistente ni evidencia
definitiva de cambios antiguos.

Preparar la versión 2.5.3 con credenciales locales existentes:

```powershell
.\scripts\prepare_publish_ota_2_5_3.ps1
```

Tras validar y con volumen UC de acceso restringido:

```powershell
.\scripts\prepare_publish_ota_2_5_3.ps1 -Publish -AcknowledgeEmbeddedSecrets
```

No incluye binario privado ni implica que haya ocurrido un flash físico.
