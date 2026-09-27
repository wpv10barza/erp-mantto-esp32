# Match capítulo–figura para Capítulo 3

Criterio aplicado: máximo una figura visible por subcapítulo; se conserva únicamente material del paquete que corresponda de forma directa al contenido del subcapítulo. No se fuerza una figura cuando el paquete no contiene evidencia visual compatible.

## Selección

| Subcapítulo | Estado | Figura del paquete | Motivo |
|---|---|---|---|
| 3.2.1 Diseño electrónico | SELECCIONADA | `ESP32-4848S040回路図2.jpg` | Es un esquema del ESP32-4848S040 y representa directamente la integración electrónica del panel, buses y periféricos tratada en el subcapítulo. |
| 3.2.2 Diseño de software | SIN FIGURA DEL PAQUETE | — | El paquete contiene fotografías/comerciales, esquemas electrónicos y material de pantallas; no contiene una arquitectura de software, Device API, editor, PlatformIO/ESPHome o flujo equivalente que corresponda de forma directa. |
| 3.2.3 Diseño del agente de IA | SIN FIGURA DEL PAQUETE | — | No se encontró en el paquete una figura del modelo, grounding, validación determinista, revisión humana o flujo del agente de IA. |

## Material útil no insertado como figura principal

- `ESP32_4848S040-1.jpg`: fotografía del panel ESP32-4848S040. Es evidencia visual de hardware, pero se descarta como figura principal de 3.2.1 para evitar duplicidad dentro del mismo subcapítulo.
- `ESP32-4848S040回路図1.jpg`: esquema relacionado con el mismo panel, descartado porque 3.2.1 ya tiene una figura seleccionada.
- `GPIO拡張.jpg`: material de expansión GPIO, demasiado específico para representar por sí solo el diseño electrónico completo.

## Material descartado

Se descartan como figura académica principal las imágenes promocionales/comerciales de Temu y del módulo genérico ESP32-S3-WROOM-1U-N16R8, así como los diagramas de AMOLED/QSPI de Waveshare cuando no corresponden al panel ESP32-S3-4848S040 utilizado en el repositorio. También se descartan miniaturas, imágenes auxiliares y comparaciones que no demuestran el diseño concreto descrito en el Capítulo 3.

## Regla de integración

La figura seleccionada debe conservar su nombre de archivo original y almacenarse bajo `docs/images/esp32-s3-4848s040/condiciones-iniciales/`. Las demás imágenes pueden mantenerse fuera del README; no deben aparecer juntas ni sustituir una figura ya elegida para el mismo subcapítulo.
