# Activación local HTTP REST del panel 4848S040

Abra `http://IP_DEL_ESP32/` para acceder a un botón por operación pública. Introduzca el token del dispositivo: se mantiene únicamente en memoria de la página y no se guarda en el navegador. Todos los POST requieren `X-3C-Device-Token`, incluyendo las rutas anteriores `/api/3c` y `/api/backend-health`; actualice clientes antiguos. Se reutiliza el token configurado en NVS. Sin token configurado se devuelve 503; token incorrecto: 401.

Este cambio corresponde al panel Guition ESP32-S3-4848S040, entorno PlatformIO `panel_4848s040`. No es firmware para el Waveshare AMOLED. No asigna GPIO de relé ni activa maquinaria.

| Botón / acción PowerShell | Método y ruta local | Efecto |
|---|---|---|
| Estado ESP32 / Estado | GET /health | Estado y contadores del dispositivo |
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
| Nube Databricks | Comprobar conexión; no inicia ni despliega una App detenida |
| Diagnóstico / Estado del dispositivo | Leer estado y contadores de `/health` |
| PROBAR CLOUD | Encolar verificación cloud y Sheets |
| ENVIAR 3C | Solicitar texto y enviar propuesta; aprobación web posterior |
| HISTORIAL | Actualizar historial del panel; no descarga sus registros a PowerShell |
| DICTAR PC | Solicitar captura al agente Windows ya iniciado |

**ACTIVAR** ejecuta la opción seleccionada, no todas las opciones juntas. La navegación física (INICIO, VOLVER, ANT, SIG, flechas y teclado) sigue siendo local al ESP32. No se simulan pulsaciones táctiles mediante REST. Los errores fotografiados de red/Sheets requieren diagnóstico con el equipo en su red; estos cambios no acreditan su resolución.
