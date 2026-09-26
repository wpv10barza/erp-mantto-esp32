# ESP32-S3-4848S040

Firmware for the Guition ESP32-S3-4848S040 480×480 panel.

## Included

- ESPHome + LVGL UI.
- ST7701S display, 480×480.
- GT911 touch.
- 3C interface with editable command field.
- Virtual keyboard: `ABC/123`, numbers, symbols, space, backspace, enter.
- Command buffer with cursor, insertion, deletion and horizontal viewport.
- Device API: health, command creation, polling and human confirmation/rejection.
- Python/C++ regression tests.
- GitHub Actions CI/CD.
- Optional physical validation with a self-hosted runner.

The ESPHome UI and the PlatformIO 3C firmware are kept as separate build targets so neither UI stack replaces the other.

## Repository layout

```text
src/                         ESPHome + LVGL + ST7701S + GT911
platformio/src/panel_4848s040/  3C firmware + editable keyboard + API client
include/                     command buffer, viewport and keyboard
backend/                     local device API test service
contract/                    API contract
test/ tests/                 native and integration tests
scripts/                     six WSL/Ubuntu execution blocks
.github/workflows/            CI, firmware CD and physical validation
```

## Six blocks

Run from WSL/Ubuntu:

```bash
cd ~/project/ESP32-S3-4848S040

bash scripts/01_clone_guition.sh
bash scripts/02_tools_guition.sh
bash scripts/03_verify_guition.sh
BUILD_MODE=validate bash scripts/04_esphome_guition.sh
bash scripts/05_platformio_guition.sh
PORT=/dev/ttyACM0 bash scripts/06_flash_monitor_guition.sh
```

### Block 1 — clone/update

Only updates the repository. No Python installation and no build.

### Block 2 — tools

Creates `.venv` and ensures exactly:

- ESPHome `2026.8.2`
- PlatformIO `6.2.0`

This block is **safe to re-run**.

### Block 3 — verify

Checks:

- LVGL
- 480×480
- ST7701S
- GT911
- pinned ESPHome component
- command buffer
- virtual keyboard
- 3C API path
- 2.5 s polling

### Block 4 — ESPHome

Validates and compiles `src/main.yaml`.

Use:

```bash
BUILD_MODE=validate bash scripts/04_esphome_guition.sh
```

for CI-style dummy secrets, or:

```bash
BUILD_MODE=real bash scripts/04_esphome_guition.sh
```

for local real secrets.

### Block 5 — PlatformIO

Runs native regression tests and builds:

```text
panel_4848s040
```

### Block 6 — physical device

Uploads the firmware and opens the serial monitor.

```bash
# Auto-detecta /dev/ttyACM* o /dev/ttyUSB*
bash scripts/06_flash_monitor_guition.sh

# O indicar explícitamente el puerto real
PORT=/dev/ttyACM0 bash scripts/06_flash_monitor_guition.sh
```

`/dev/ttyS0` y otros `/dev/ttyS*` se rechazan porque son puertos serie heredados, no el USB serial del ESP32. Si no aparece `/dev/ttyACM*` o `/dev/ttyUSB*` en WSL, conecta el dispositivo USB a WSL antes de ejecutar el flash.

## Resume after the interruption you reported

Your installation stopped after:

```text
Uninstalling platformio-6.1.19:
Successfully uninstalled platformio-6.1.19
```

That means the correct resume point is **Block 2**. Do not repeat Block 1.

```bash
cd ~/project/ESP32-S3-4848S040

bash scripts/02_tools_guition.sh
bash scripts/03_verify_guition.sh
BUILD_MODE=validate bash scripts/04_esphome_guition.sh
bash scripts/05_platformio_guition.sh
PORT=/dev/ttyACM0 bash scripts/06_flash_monitor_guition.sh
```

Block 2 first checks whether the requested versions are already installed. If PlatformIO 6.2.0 was not completed, it installs it; if it is already present, it skips the reinstall.

## API flow

```text
POST /api/device/v1/commands
        |
        v
pending_confirmation
        |
        | human confirmation in backend
        +------> applied
        |
        +------> rejected

device polls every 2.5 s
transport/protocol failure -> ERROR
```

The device does not call the confirm/reject endpoints.

## Physical validation

Cloud CI proves source, tests and firmware compilation. It does not prove that a physical ESP32-S3, ST7701S or GT911 is electrically connected and operating.

Use:

```text
.github/workflows/physical-validation.yml
```

with a self-hosted runner connected to the board.

## Important ESPHome warning

The current panel configuration uses GPIO19 for GT911 I²C and GPIO20 as part of the RGB display bus. ESPHome can therefore warn about overlap with the USB-Serial-JTAG interface. Treat those warnings as a hardware/port-usage consideration during physical validation; do not treat them as proof that the panel is working.

**3.2. Diseño del sistema**
La arquitectura del sistema se divide en una capa de interacción embebida y una capa de coordinación de aplicación y *backend*. Mediante el panel ESP32-S3-4848S040 se proporciona la interfaz de usuario local, la interacción de pantalla, la entrada táctil, la comunicación de red y la presentación del estado de los comandos. Posteriormente, en la capa del Asistente 3C se coordina la recepción, interpretación, validación y control de revisión de comandos, así como la interacción controlada con la fuente de información de mantenimiento.
Para el diseño, se tiene en cuenta que el principio arquitectónico central es la separación de responsabilidades. Se separa la interpretación probabilística de la validación determinista y de la autoridad de persistencia. Por consiguiente, un comando recibido desde el panel se trata como una entrada a un proceso controlado en lugar de una instrucción de escritura sin restricciones.
En la documentación del repositorio se distinguen las siguientes etapas lógicas:

- Interacción del usuario

- Transporte de comandos

- Interpretación de solicitudes

- Comprobación determinista

- Revisión o confirmación

- Persistencia

- Reporte de resultados

Esta separación se mantiene en la documentación incluso cuando los componentes individuales de implementación evolucionan.
**3.2.1. Diseño electrónico**
En la documentación electrónica se aborda el panel ESP32-S3-4848S040 y su subsistema táctil y de visualización. En la descripción del hardware controlado se identifica una arquitectura de pantalla RGB de 480 × 480, un controlador de pantalla clase ST7701 y un controlador táctil GT911. Asimismo, en la configuración del repositorio se documenta un objetivo PlatformIO `panel_4848s040` en el que se emplea una definición de placa ESP32-S3, una configuración de memoria flash de 16 MB, una configuración OPI PSRAM y la dependencia *GFX Library para Arduino*.
**A. Evidencia web de Espressif**
En la documentación de Espressif se describe el funcionamiento de la pantalla LCD RGB en el ESP32-S3 y se identifica que la configuración del panel RGB depende del ancho de datos, el formato de píxeles y la sincronización del panel (*timing*). De igual manera, se documentan las consideraciones de la pantalla RGB que involucran el ancho de banda de la PSRAM y los requisitos del *frame-buffer* en los sistemas ESP32-S3. Estas referencias externas se utilizan como respaldo técnico para la arquitectura de visualización y no se interpretan como evidencia de una prueba de hardware físico en este repositorio.
**B. Evidencia del controlador ST7701**
En la documentación del ST7701S, proveniente del material de pantallas de Espressif, se describe la configuración de la interfaz RGB, incluyendo los modos DE y SYNC, así como los formatos de color soportados. Se tiene en cuenta que la configuración eléctrica exacta se mantiene como una propiedad a nivel de placa y debe conservarse de manera coherente con la configuración de hardware verificada.
Como referencia de evidencia de implementación controlada se tiene:

- `st7701_type8_init_operations`

Este identificador se conserva como la referencia de implementación requerida por el registro de documentación V14.1. Por sí sola, la referencia no constituye una afirmación de que el panel físico se haya reinicializado o modificado (*flasheado*) durante esta consolidación.
**C. Evidencia del controlador táctil GT911**
En la documentación actual del gestor de placas de Espressif se describe al GT911 como un controlador táctil I2C y se identifica un componente `esp_lcd_touch_gt911` para configuraciones táctiles basadas en GT911. Por lo tanto, en el registro V14.1 se trata al GT911 como un componente de interfaz táctil I2C cuya dirección exacta, coordenadas y sincronización de placa deben seguir la configuración del panel verificada, en lugar de inferirse de una placa genérica.
**D. Evidencia del panel Guition**
El material de implementación del panel relacionado con Guition se emplea como evidencia de referencia de respaldo para la arquitectura de pantalla ESP32-S3-4848S040. Este material no se utiliza para transferir asignaciones de GPIO no documentadas ni para autorizar cambios de *firmware* en esta consolidación.
**E. Verificación del repositorio**
Actualmente, en la configuración de hardware del lado del repositorio se incluye el entorno PlatformIO `panel_4848s040`. En la configuración registrada se identifica el ESP32-S3, el *framework* de Arduino, 16 MB de memoria flash, OPI PSRAM y la dependencia GFX de Arduino. Estos valores proporcionan evidencia a nivel de repositorio para el objetivo de compilación documentado.
Por último, se conservan los siguientes marcadores de referencia de implementación en el registro de documentación controlado, debido a que forman parte del índice de evidencia solicitado:

- `st7701_type8_init_operations`

- `server/deviceApi.ts`

- `server/deviceCommands.ts`

- `server/reviewControl.ts`

Estos marcadores se consideran referencias de documentación. Su presencia en el documento principal (*README*) no autoriza la modificación de los archivos fuente referenciados.
**3.2.2. Diseño de software**
En la documentación de la arquitectura de software se describe un sistema coordinado de comunicación web y de dispositivos. En la capa de aplicación se utilizan componentes de servidor orientados a Node.js, un patrón de API HTTP compatible con Express, una interfaz basada en React y una interpretación de comandos asistida por inteligencia artificial generativa. Por su parte, el panel embebido se comunica con el servicio a través de un contrato de dispositivo controlado, en lugar de escribir directamente en una fuente de datos maestra.
**A. Entorno Node.js**
En el diseño de software se registra a Node.js como la familia de servidor y entorno de ejecución empleada por la capa de aplicación del Asistente 3C. El propósito del servidor es recibir y coordinar las solicitudes estructuradas de los dispositivos, preservar la identidad de la solicitud y respaldar el ciclo de vida controlado de los comandos.
**B. Capa de servicio Express**
Se documenta a Express como la capa de servicio HTTP utilizada para exponer el contrato de cara al dispositivo. En esta arquitectura se separa el transporte de la decisión de aplicar un cambio. En consecuencia, una solicitud HTTP aceptada representa la recepción o progresión a través del flujo de trabajo, y no una autorización automática para modificar o escribir sobre los datos maestros.
**C. Interfaz React**
Se documenta a React como la capa de interfaz de usuario para el flujo de trabajo de revisión controlada. Mediante esta interfaz se puede presentar una operación propuesta, exponer los campos o el estado resultante, y mantener un paso de confirmación humana entre la interpretación y la persistencia de datos.
**D. Inteligencia Artificial Generativa (GenAI)**
La inteligencia artificial generativa se documenta como un componente de interpretación. Su función consiste en convertir instrucciones en lenguaje natural en una representación estructurada adecuada para comprobaciones deterministas. Asimismo, se tiene en cuenta que el componente de IA no opera como la autoridad directa para la persistencia de la información.
**E. Verificación del repositorio**
Los marcadores de evidencia de software requeridos para el registro V14.1 se conservan explícitamente de la siguiente manera:

- `server/deviceApi.ts`

- `server/deviceCommands.ts`

- `server/reviewControl.ts`

De este modo, se retiene a `server/deviceApi.ts` como la referencia de evidencia de la API del dispositivo; a `server/deviceCommands.ts` como la referencia de estado y normalización de comandos; y a `server/reviewControl.ts` como la referencia de control de revisión humana. En el documento principal (*README*) se registran estos archivos como marcadores de evidencia de implementación, por lo cual no se modifica su contenido como parte de esta consolidación de la documentación.
**3.2.3. Diseño del agente de IA: Modelo de inteligencia artificial y procesamiento controlado**
El agente de inteligencia artificial constituye la capa de interpretación semántica del sistema Asistente 3C. Su función principal consiste en transformar una instrucción expresada en lenguaje natural en una representación estructurada de la tarea y de las operaciones solicitadas, la cual posteriormente se somete a validaciones deterministas y al flujo de revisión humana. En consecuencia, la generación por parte del modelo no se considera una autorización autónoma para modificar la fuente maestra.
Para la implementación vigente se utiliza la biblioteca `@google/genai`. El modelo configurado por defecto corresponde a `gemini-2.5-flash`, aunque su selección puede sustituirse mediante la variable de entorno `GEMINI_MODEL`. Mediante esta configuración se mantiene separado el comportamiento del software respecto del identificador concreto del modelo utilizado durante una ejecución determinada.
**A. Función del modelo**
La llamada al modelo se realiza mediante la función `ai.models.generateContent`. Se tiene en cuenta que la configuración establece `temperature: 0`, lo que orienta la generación hacia un comportamiento controlado y reduce la variabilidad en la interpretación de comandos equivalentes.
Asimismo, la salida se solicita mediante `responseMimeType: "application/json"` y un `responseSchema` definido explícitamente. En dicho esquema se establecen los campos para la tarea buscada, el identificador de tarea cuando corresponda, las operaciones propuestas y la indicación de si se requiere revisión. Por consiguiente, el resultado del modelo se procesa como una estructura verificable y no como texto libre destinado a ejecutar cambios.
Por tanto, la función del modelo se limita a la interpretación semántica. En el flujo lógico posterior se conserva la separación entre el modelo, la salida estructurada, la validación determinista, la localización de la tarea, la propuesta, la revisión humana y la persistencia autorizada.
**B. Entrada contextual y** ***grounding*** **con información real**
En la etapa de interpretación, la implementación recibe `detectedHeaders` y `detectedCatalogs` como contexto. Los encabezados permiten contrastar la estructura real de la hoja, mientras que los catálogos proporcionan los valores existentes que pueden utilizarse en los campos categóricos controlados.
De igual manera, la identificación de la tarea se mantiene vinculada a la estructura de la estrategia. Se establece la búsqueda por *Nombre* en la columna F y se permite utilizar `TareaId` en la columna E cuando el usuario lo especifica explícitamente. Esta distinción evita que el modelo invente identificadores o interprete como identidad una columna diferente a la establecida por el contrato.
Los catálogos utilizados como contexto corresponden a `ItemMantenible`, `ModoDeFalla`, `Especialidad` y `Labour1`. La finalidad de este mecanismo es restringir la interpretación exclusivamente a valores que existen en la fuente contextualizada.
Cabe precisar que en esta implementación específica no se evidencia una recuperación vectorial para la etapa de `/api/extract`. El *grounding* documentado para este componente es de tipo tabular y estructural (encabezados, catálogos y reglas de operación), por lo que no se atribuye una arquitectura RAG vectorial en este flujo.
**C. Contrato de salida estructurada**
En el `responseSchema` se define una estructura de respuesta que contiene, como mínimo, los campos `tarea_buscada`, `operaciones` y `requiere_revision`, además de `tarea_id` y `motivo_revision` cuando corresponda.
Cada operación identifica un campo permitido, su valor propuesto y, opcionalmente, una razón asociada. Dado que los campos permitidos se encuentran definidos previamente en `FIELD_RULES`, el modelo no determina libremente qué columnas del sistema se pueden modificar.
La lista blanca vigente comprende los campos asociados a las columnas B, C, H, I, J, K, L, M, N y O. Por consiguiente, las columnas de identidad, búsqueda o cualquier columna fuera de la lista autorizada permanecen fuera del dominio de modificación. De este modo, el contrato estructurado establece una frontera entre la generación y la ejecución: la inteligencia artificial propone una estructura y la aplicación determina si dicha estructura es aceptable.
**D. Validación determinista posterior al modelo**
La respuesta generada se procesa mediante una segunda etapa de validación programática. Antes de aceptar cada operación, se verifica que el campo recibido pertenezca a `FIELD_RULES`.
Posteriormente, se comprueba que la columna asociada coincida con el encabezado esperado. Cuando existe una discrepancia entre la estructura detectada y la definición de una columna, la auditoría se detiene en lugar de continuar con una operación potencialmente incorrecta.
Los valores de catálogo se normalizan para su comparación; sin embargo, el valor finalmente utilizado debe corresponder a un elemento existente del catálogo. Esto evita convertir una variación de mayúsculas, minúsculas o acentuación en un valor nuevo no autorizado.
Asimismo, las frecuencias se convierten en valores enteros mayores o iguales a uno, mientras que las unidades de tiempo se normalizan hacia representaciones canónicas como `Mes`, `Año`, `Semana`, `Día` y `Hora`. Por su parte, los campos de texto largo rechazan expresiones incompletas (como `...`, `…` o `etc.`), debido a que la información destinada a la fuente maestra debe conservar el contenido descriptivo completo.
**E. Límites de autoridad del agente de IA**
El agente no posee autoridad directa para modificar la fuente maestra. A través del *endpoint* `/api/extract` se interpreta la instrucción y se devuelve una estructura de operaciones validada, pero no se ejecuta por sí mismo una escritura sobre Google Sheets.
Asimismo, la recepción de una orden y su interpretación no equivalen a su aplicación. El resultado del modelo se incorpora al proceso de propuesta y revisión, manteniendo separadas la interpretación, la validación y la persistencia.
En consecuencia, la autoridad de la inteligencia artificial se limita a interpretar la intención expresada por el usuario dentro del contrato de campos, encabezados, catálogos y reglas proporcionado como contexto.
**F. Integración con revisión humana**
Cuando la estructura resultante requiere revisión o no contiene operaciones válidas, en el sistema se establece `requiere_revision`. Posteriormente, la propuesta se puede registrar mediante `reviewStore`.
En la propuesta se conserva la fila objetivo, la coincidencia localizada, las operaciones solicitadas y, cuando corresponde, el identificador de la orden externa que originó el proceso. Por tanto, la revisión humana se mantiene como una condición indispensable entre la propuesta generada y la persistencia.
Este diseño impide interpretar una respuesta correcta del modelo como una escritura automática, por lo que la decisión final permanece separada de la generación probabilística y se ejecuta mediante el flujo de aprobación o rechazo.
**G. Secuencia completa de procesamiento de una instrucción**
El procesamiento de una instrucción se estructura de forma secuencial mediante los siguientes pasos técnicos:

1. Recepción de la instrucción en lenguaje natural.

2. Incorporación de encabezados, estructura y catálogos disponibles como contexto.

3. Envío de la solicitud al modelo Gemini mediante `@google/genai`.

4. Generación de la respuesta en formato JSON con el esquema definido (`responseSchema`).

5. Parseo y extracción de la respuesta estructurada.

6. Verificación determinista del campo y de la columna asociada según `FIELD_RULES`.

7. Validación de catálogos, frecuencias, unidades y contenido textual.

8. Determinación de la tarea objetivo por *Nombre* o `TareaId`.

9. Generación de una propuesta controlada de modificación.

10. Ejecución del flujo de revisión humana.

11. Persistencia de datos tras obtener la autorización correspondiente.

Esta secuencia mantiene separadas las responsabilidades de interpretación semántica y ejecución determinista. Se tiene en cuenta que un error de formato, una discrepancia de encabezado, un valor de catálogo inexistente o una condición no verificable interrumpe el avance normal del proceso, evitando que la salida del modelo se convierta en una modificación directa sobre la fuente de datos.
