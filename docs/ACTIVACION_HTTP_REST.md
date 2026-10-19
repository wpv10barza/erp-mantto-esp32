# Activación local HTTP REST del panel 4848S040

Abra `http://IP_DEL_ESP32/` para acceder a un botón por operación pública. Introduzca el token del dispositivo: se mantiene únicamente en memoria de la página y no se guarda en el navegador. Todos los POST requieren `X-3C-Device-Token`, incluyendo las rutas anteriores `/api/3c` y `/api/backend-health`; actualice clientes antiguos. Se reutiliza el token configurado en NVS. Sin token configurado se devuelve 503; token incorrecto: 401.

Este cambio corresponde al panel Guition ESP32-S3-4848S040, entorno PlatformIO `panel_4848s040`. No es firmware para el Waveshare AMOLED. No asigna GPIO de relé ni activa maquinaria.

| Botón / acción PowerShell | Método y ruta local | Efecto |
|---|---|---|
| Estado ESP32 / Estado | GET /health | Estado y contadores del dispositivo |
| Iniciar App Databricks / IniciarDatabricks | POST /api/databricks/start | Solicita inicio en el workspace y verifica disponibilidad |
| Probar Databricks / Databricks | POST /api/backend-health | Encola comprobación de backend |
| Verificar servicios y Sheets / Servicios | POST /api/cloud-verify | Encola diagnóstico cloud existente |
| Actualizar historial / Historial | POST /api/history | Carga historial en el panel |
| Solicitar micrófono PC / VozPC | POST /api/voice-capture | Solicita captura al agente PC existente |
| Enviar al asistente / Enviar3C | POST /api/3c | Encola propuesta con campo `text`, 1–240 bytes UTF-8 |
| Consultar firmware / ConsultarFirmware | POST /api/ota/check | Busca firmware publicado con campo `commit` de 40 hexadecimales |

La confirmación de instalación OTA permanece en la pantalla física; los ACK de voz y el sondeo se mantienen automáticos. No se añade un endpoint remoto de instalación. Una respuesta 202 confirma solamente entrada en cola: revise el resultado en el panel. Un 503 también puede indicar cola no disponible. No repita automáticamente una orden tras timeout.

## Windows PowerShell

Desde la raíz del repositorio, configure `ESP32_DEVICE_TOKEN` de forma privada en la sesión o proporcione `-DeviceToken`. No publique credenciales en Git.

```powershell
.\scripts\Activar-ESP32-REST.ps1 -DeviceUrl http://192.168.1.50 -Action Estado
.\scripts\Activar-ESP32-REST.ps1 -DeviceUrl http://192.168.1.50 -Action Databricks
.\scripts\Activar-ESP32-REST.ps1 -DeviceUrl http://192.168.1.50 -Action Servicios
.\scripts\Activar-ESP32-REST.ps1 -DeviceUrl http://192.168.1.50 -Action Historial
.\scripts\Activar-ESP32-REST.ps1 -DeviceUrl http://192.168.1.50 -Action VozPC
.\scripts\Activar-ESP32-REST.ps1 -DeviceUrl http://192.168.1.50 -Action Enviar3C -Text 'Cambia la tarea J10 a mensual'
.\scripts\Activar-ESP32-REST.ps1 -DeviceUrl http://192.168.1.50 -Action ConsultarFirmware -Commit '<SHA completo publicado>'
```

Sustituya la IP y el SHA. `VozPC` necesita el proceso de captura de Windows activo. Las propuestas requieren revisión humana en la aplicación antes de modificar Sheets.

El trayecto PowerShell/navegador → ESP32 es HTTP en LAN de confianza; no publique el puerto 80 en Internet. El token circula sin cifrar en ese tramo. ESP32 → Databricks utiliza la integración HTTPS existente y requiere Internet. Esto no convierte Databricks en un servicio local ni elimina su dependencia cloud.

## Validación en equipo

Compile `pio run -e panel_4848s040` y cargue mediante el procedimiento existente para esa placa. Compruebe GET de estado, rechazo POST sin token, entrada en cola con token y resultado en pantalla de cada operación. Verifique que una consulta OTA no instala firmware y que una propuesta 3C no se aplica sin aprobación. La prueba sobre hardware requiere acceso al ESP32 y a su red.

## Menú PowerShell equivalente al menú principal físico

```powershell
.\scripts\Menu-ESP32.ps1 -DeviceUrl http://192.168.1.50
```

Seleccione una opción y confirme en el único paso **ACTIVAR**. El menú permanece abierto hasta elegir 0. Los botones táctiles existentes no se reemplazan ni se agregan duplicados a la pantalla física. El cliente directo `Activar-ESP32-REST.ps1` sigue disponible para automatizaciones sin menú.

| Menú físico / PowerShell | Operación desde PowerShell |
|---|---|
| Conexión al backend | Solicitar comprobación Databricks |
| Google Sheets | Verificar servicios y permisos; no escribir datos |
| GitHub Actions | Abrir ejecuciones del repositorio; no dispara workflows |
| Actualizar firmware | Consultar commit publicado; instalación se confirma en ESP32 |
| Wi-Fi 2.4 GHz | Leer conectividad reportada; no cambia credenciales |
| Nube Databricks → INICIAR APP | Iniciar la App existente y comprobar el backend; no crea ni despliega otra App |
| Diagnóstico / Estado del dispositivo | Leer estado y contadores de `/health` |
| PROBAR CLOUD | Encolar verificación cloud y Sheets |
| ENVIAR 3C | Solicitar texto y enviar propuesta; aprobación web posterior |
| HISTORIAL | Actualizar historial del panel; no descarga sus registros a PowerShell |
| DICTAR PC | Solicitar captura al agente Windows ya iniciado |

**ACTIVAR** ejecuta la opción seleccionada, no todas las opciones juntas. La navegación física (INICIO, VOLVER, ANT, SIG, flechas y teclado) sigue siendo local al ESP32. No se simulan pulsaciones táctiles mediante REST. Los errores fotografiados de red/Sheets requieren diagnóstico con el equipo en su red; estos cambios no acreditan su resolución.

## Inicio de la App desde el firmware

En la pantalla: **Nube Databricks → INICIAR APP**. Abrir la sección no inicia cómputo.
PowerShell: `./scripts/Activar-ESP32-REST.ps1 -DeviceUrl http://IP_DEL_ESP32 -Action IniciarDatabricks`.
El worker llama `POST /api/2.0/apps/asistente-cloud-erp/start` al workspace HTTPS,
con OAuth M2M ya configurado. El principal necesita CAN_MANAGE en esa App; este cambio
no concede permisos. El inicio puede reanudar consumo de cómputo.
El nombre puede ajustarse con DATABRICKS_APP_NAME_VALUE en local_config.h.
Una respuesta 2xx indica solicitud aceptada, no servicio listo. Durante hasta 180 s
se verifica health cada 5 s, sin bloquear el táctil ni repetir el POST. `/health`
expone databricks_start_state, databricks_start_http y databricks_start_busy.
401/403 exige revisar credenciales/permisos; timeout significa resultado desconocido.
Los POST duplicados se rechazan mientras la activación está en cola o iniciándose.

GitHub Actions ejecuta la máquina de estados real con transporte simulado (aceptado,
OAuth/403, red, 5xx, timeout, doble pulsación y reloj desbordado), los contratos de
conexión UI/REST y las pruebas PowerShell 5.1/7, además de compilar el firmware.
Estas pruebas no arrancan la App real ni sustituyen la verificación física.

## Flash de un commit validado

Con el panel **4848S040** conectado, checkout del SHA de la rama del PR y ejecutar:
`./scripts/Flash-ESP32-3C.ps1 -ExpectedCommit (git rev-parse HEAD)`.
El script exige árbol versionado limpio, configuración local o NVS explícita,
puerto único y chip ESP32-S3 verificado antes de escribir. Si hay varios puertos,
añada `-Port COMx`. Un ESP32-S3 por sí solo no identifica el modelo de pantalla:
no usar este firmware en Waveshare AMOLED. No se borra NVS.
