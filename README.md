# ERP Mantto ESP32 — ESP32-S3-4848S040

Firmware para el panel **Guition ESP32-S3-4848S040**, con pantalla **480 × 480**, controlador **ST7701S**, táctil **GT911** y comunicación con el sistema Asistente 3C.

El repositorio conserva dos objetivos funcionales separados:

- **ESPHome + LVGL** para la interfaz gráfica del panel.
- **PlatformIO** para el firmware 3C con editor de órdenes, teclado virtual, command buffer y cliente HTTP del dispositivo.

La separación de objetivos es deliberada: la configuración LVGL pertenece al objetivo ESPHome y la interfaz 3C documentada en este README pertenece al objetivo PlatformIO. No se introduce una dependencia independiente <code>lvgl/lvgl</code> en PlatformIO.

## 3.1. Enfoque del sistema

El sistema se concibe como una solución embebida de interacción y comunicación controlada para el Asistente 3C. El panel ESP32-S3-4848S040 constituye el punto de interacción local y concentra la visualización, la entrada táctil, la edición de instrucciones y la comunicación con la API del dispositivo. El procesamiento posterior se mantiene fuera de la autoridad directa del panel: la orden se transporta al servicio correspondiente, atraviesa el flujo de interpretación y control establecido y permanece sujeta a revisión humana antes de cualquier aplicación autorizada.

La organización del diseño adopta una separación de responsabilidades entre interacción, transporte, interpretación, validación, revisión, persistencia y reporte. Esta separación permite que una respuesta HTTP aceptada o una interpretación válida no se confundan con la aplicación final del cambio. En consecuencia, el capítulo distingue las condiciones que debe satisfacer cada subsistema, las decisiones de diseño adoptadas para cumplirlas, su implementación documentada y los mecanismos de verificación disponibles.

## 3.2. Diseño del sistema

El diseño se desarrolla a partir de condiciones iniciales agrupadas por subsistema. Cada condición se formula como un requisito previo que delimita lo que el sistema debe proporcionar; a continuación se establece la decisión de diseño que responde a la condición, se identifica la implementación documentada y se indica el nivel de evidencia que permite verificarla. Esta separación evita presentar como funcionamiento físico una característica que solamente está declarada, configurada, compilada o probada en software.

### 3.2.1. Diseño electrónico

El subsistema electrónico debe proporcionar una superficie de interacción visual y táctil compatible con la interfaz del Asistente 3C, conservando la geometría del panel y una integración coherente entre pantalla, controlador táctil, memoria, comunicación y alimentación. Esta condición es necesaria para que la información presentada al usuario y las coordenadas recibidas por el subsistema táctil correspondan al mismo espacio físico de interacción.

La solución de diseño utiliza el panel Guition ESP32-S3-4848S040 con una resolución de 480 × 480, pantalla RGB basada en ST7701S y controlador táctil GT911. El objetivo electrónico debe conservar una configuración de memoria y de placa compatible con los dos objetivos de construcción del proyecto. En la configuración ya documentada se registran 16 MB de memoria Flash, PSRAM OPI y el entorno PlatformIO <code>panel_4848s040</code>; estos datos se utilizan como parámetros del diseño y no como demostración de funcionamiento físico.

![Figura 13. Mapa técnico del panel ESP32-S3-4848S040](docs/images/esp32-s3-4848s040/fig13_panel_base.png)

**Figura 13. Mapa técnico del panel ESP32-S3-4848S040 y sus condiciones de integración.**

La Figura 13 permite relacionar la geometría del panel con los componentes que deben mantenerse coordinados. Su función es documental: representa las condiciones de integración declaradas para el diseño y no sustituye una prueba eléctrica o funcional sobre el dispositivo.

La distribución lógica del subsistema debe preservar la relación entre procesamiento, visualización, tacto, memoria, retroiluminación y comunicación. El diseño del repositorio organiza estos elementos alrededor del ESP32-S3 y diferencia el objetivo ESPHome del objetivo PlatformIO, sin asumir que ambas configuraciones representan una única pila de software en ejecución simultánea.

![Figura 14. Mapa lógico de subsistemas y periféricos documentados del ESP32-S3-4848S040](docs/images/esp32-s3-4848s040/fig14_subsystems.png)

**Figura 14. Mapa lógico de subsistemas y periféricos documentados del ESP32-S3-4848S040.**

La interfaz táctil debe mantener coherencia entre la geometría de la pantalla, las coordenadas recibidas y la lógica de interacción. El sistema utiliza GT911 como controlador táctil; adicionalmente, el README fuente advierte una consideración de uso de puertos en la que GPIO19 participa en I²C del GT911 y GPIO20 forma parte del bus RGB, por lo que esta condición debe revisarse durante la validación física y no interpretarse como una prueba de funcionamiento o de fallo.

![Figura 15. Mapa técnico de buses, señales y asignaciones GPIO del panel ESP32-S3-4848S040](docs/images/esp32-s3-4848s040/fig15_gpio_map.png)

**Figura 15. Mapa técnico de buses, señales y asignaciones GPIO del panel ESP32-S3-4848S040.**

La Figura 15 conserva la evidencia documental de las relaciones de buses y señales ya integradas en el repositorio. La coincidencia entre el mapa y la configuración constituye evidencia de consistencia documental; la continuidad eléctrica de las señales debe comprobarse mediante una validación física.

| Elemento | Configuración documentada |
|---|---|
| Panel | Guition ESP32-S3-4848S040 |
| Resolución | 480 × 480 |
| Display | ST7701S |
| Touch | GT911 por I²C |
| Flash | 16 MB |
| PSRAM | OPI |
| Entorno de firmware 3C | PlatformIO <code>panel_4848s040</code> |
| Objetivo de interfaz | ESPHome + LVGL |
| Consideración física | validación específica sobre el panel conectado |

La configuración de ESPHome y la de PlatformIO se mantienen como objetivos de construcción independientes para el mismo hardware. La primera se orienta a la interfaz ESPHome + LVGL y la segunda al firmware 3C con editor, teclado virtual, <code>commandBuffer</code> y cliente HTTP. Esta separación es una decisión de diseño orientada a evitar que una pila de interfaz reemplace a la otra.

La verificación de este subsistema se limita al nivel que realmente ejecuta cada procedimiento. Los scripts de verificación y las compilaciones demuestran consistencia de configuración y construcción; la validación física requiere un ESP32-S3 real conectado y un proceso específico de carga, arranque y comprobación de pantalla, táctil, memoria, comunicación y alimentación.

### 3.2.2. Diseño de software

El subsistema de software debe permitir que una instrucción sea introducida, editada, transportada y consultada de forma controlada, sin convertir la interacción del usuario en una escritura directa sobre una fuente maestra. Para ello se requiere una arquitectura que mantenga separados el transporte HTTP, el procesamiento de la orden, la validación, la revisión humana, la persistencia y el reporte del estado.

La decisión de diseño utiliza dos objetivos de firmware relacionados con el mismo panel. ESPHome + LVGL se reserva para la interfaz gráfica, mientras que PlatformIO mantiene el firmware de interacción 3C. La independencia entre ambos objetivos permite conservar la interfaz documentada y, al mismo tiempo, mantener un cliente de dispositivo específico para el flujo de comandos.

En la implementación del firmware 3C, el editor trabaja con un <code>commandBuffer</code> de tamaño fijo y soporta cursor, inserción, borrado, desplazamiento horizontal del texto, teclado virtual <code>ABC/123</code>, letras, números, símbolos, espacio, backspace y enter. La entrada editada se utiliza para construir la solicitud enviada mediante el cliente del dispositivo. De este modo, la condición funcional de disponer de una orden editable se materializa en un componente de interacción concreto y verificable por software.

La comunicación con el Asistente 3C se realiza mediante la Device API versionada. El diseño contempla <code>GET /api/device/v1/health</code>, <code>POST /api/device/v1/commands</code> y <code>GET /api/device/v1/commands/{command_id}</code>. El ciclo de una orden comienza con su creación, pasa al estado <code>pending_confirmation</code>, queda sujeto a confirmación o rechazo humano y finaliza como <code>applied</code> o <code>rejected</code>. El dispositivo consulta el estado mediante *polling* cada 2.5 s y trata los fallos de transporte o protocolo como error, sin convertirlos en una aplicación automática.

El diseño también establece una frontera entre transporte e interpretación. El panel transmite la instrucción y el contexto mínimo necesario para identificar la solicitud; la interpretación y las comprobaciones posteriores pertenecen al flujo controlado del Asistente 3C. La revisión humana permanece entre la propuesta y la aplicación final, por lo que la recepción de una respuesta o la generación de una propuesta no equivale a persistencia autorizada.

La verificación del subsistema se realiza en varios niveles. El Block 3 comprueba la presencia de LVGL, la geometría 480 × 480, ST7701S, GT911, el buffer de comandos, el teclado virtual, la ruta de la API y el *polling* de 2.5 s. El Block 4 valida y compila la configuración ESPHome; el Block 5 ejecuta las pruebas nativas y construye el objetivo <code>panel_4848s040</code>. Las pruebas Python, C++ y E2E permiten verificar contratos, editor, estados y comunicación de software. GitHub Actions automatiza estas comprobaciones, pero no sustituye la validación física del panel.

La relación entre construcción y evidencia se mantiene explícita: la documentación demuestra el diseño declarado; la implementación demuestra la presencia de código y configuración; las pruebas automatizadas demuestran los casos ejecutados; GitHub Actions demuestra las validaciones que el workflow realmente corre; y la validación física demuestra el comportamiento del dispositivo conectado. La Figura 16 conserva el flujo documental de compilación, carga, monitorización y validación.

### 3.2.3. Diseño del agente de IA: Modelo de inteligencia artificial y procesamiento controlado

El agente de inteligencia artificial forma parte del sistema de aplicación del Asistente 3C y se considera una capa de interpretación semántica. Su función queda subordinada al contrato de procesamiento del sistema y no sustituye la validación de reglas ni la revisión humana. En el contexto de este repositorio de firmware, la IA se documenta por su frontera de integración con el dispositivo y no como un componente autónomo de persistencia.

**A. Función del modelo**

El modelo debe transformar una instrucción expresada en lenguaje natural en una propuesta estructurada que pueda continuar hacia las etapas deterministas del sistema. La función del modelo termina en la interpretación de la intención; no incluye la autorización de una modificación física o de una escritura sobre una fuente maestra.

**B. Entrada contextual y** ***grounding*** **con información real**

La interpretación debe operar sobre información real disponible en el flujo de la aplicación, evitando que la salida del modelo introduzca entidades, estados o valores sin correspondencia con el contexto recibido. La orden originada en el panel constituye una entrada controlada y la información necesaria para resolverla debe provenir de las fuentes y contratos que el backend haya puesto a disposición del proceso.

**C. Contrato de salida estructurada**

La salida del modelo debe ajustarse a una representación estructurada y verificable antes de pasar a la ejecución. Esta frontera se relaciona con el contrato de la aplicación y con la Device API: el resultado de la interpretación se convierte en una propuesta que todavía debe ser comprobada y sometida al flujo de revisión.

**D. Validación determinista posterior al modelo**

Después de la interpretación, las condiciones que puedan expresarse como reglas deben resolverse de forma determinista. La validación debe comprobar consistencia de la propuesta, correspondencia con la estructura autorizada y cumplimiento de las restricciones antes de permitir que la orden avance a una etapa de persistencia autorizada.

**E. Límites de autoridad del agente de IA**

El agente no tiene autoridad directa para modificar la fuente maestra. La persistencia queda fuera del alcance del modelo y depende del flujo controlado de la aplicación. En el lado del dispositivo, además, la escritura directa queda fuera del contrato de la Device API; el ESP32 únicamente inicia y consulta el ciclo de la orden.

**F. Integración con revisión humana**

La revisión humana constituye una condición explícita del flujo de aplicación. La orden permanece en <code>pending_confirmation</code> hasta que el sistema de revisión determine su confirmación o rechazo. Por tanto, una respuesta correcta del modelo no es suficiente para considerar aplicada la operación.

**G. Secuencia completa de procesamiento de una instrucción**

La secuencia completa se interpreta de la siguiente manera:

1. Introducción o edición de la instrucción en el panel.
2. Envío de la orden mediante la Device API.
3. Recepción y procesamiento controlado en el Asistente 3C.
4. Interpretación semántica y conformación de la propuesta.
5. Validación de las condiciones y reglas deterministas aplicables.
6. Registro de la orden en estado <code>pending_confirmation</code>.
7. Revisión humana mediante confirmación o rechazo.
8. Consulta del estado por *polling* desde el dispositivo.
9. Cierre del ciclo en <code>applied</code>, <code>rejected</code> o error de protocolo/transporte.

La secuencia mantiene separadas la interpretación, la validación, la revisión y la persistencia. De esta forma, una instrucción de usuario puede atravesar todas las etapas sin que el componente probabilístico adquiera por sí mismo autoridad para aplicar cambios.

## Interfaz ESPHome + LVGL

El archivo [src/main.yaml](src/main.yaml) configura el objetivo ESPHome con LVGL, traducciones, fuentes, widgets, OTA, Wi-Fi, GT911 y ST7701S.

La configuración fija el componente externo de <code>i18n</code> al commit:

<code>alaltitov/esphome@1b487af0ef26ff8e7908d34e415d99cc13fc1f98</code>

La referencia está fijada para reproducibilidad. El proyecto no debe sustituirla por <code>@dev</code> de manera arbitraria.

Las siguientes capturas son activos documentales existentes en <code>doc/images/</code>. Sirven para describir la interfaz; no constituyen evidencia de una prueba física.

### Pantalla principal

![Pantalla principal de la interfaz ESPHome/LVGL](doc/images/home.png)

### Configuración

![Pantalla de configuración](doc/images/settings.png)

### Carga e inicio

![Pantalla de carga](doc/images/loading.png)

## Editor 3C y teclado virtual

El entorno PlatformIO <code>panel_4848s040</code> contiene el editor editable de órdenes.

La fuente de texto en tiempo de ejecución es <code>commandBuffer</code>, con capacidad de **240 caracteres**. La implementación conserva:

- cursor y navegación;
- inserción de caracteres;
- borrado hacia atrás y hacia delante;
- viewport horizontal para texto largo;
- modo <code>ABC/123</code>;
- letras, números y símbolos;
- espacio, backspace y enter;
- cancelación;
- cambio de modo del teclado.

El toque sobre el campo permite aproximar el cursor al carácter seleccionado. Al confirmar con Enter, el texto actual del <code>commandBuffer</code> es el que consume <code>send3CCommand()</code>. El comando por defecto únicamente inicializa el buffer y no reemplaza el contenido editado durante la ejecución.

La interfaz del panel mantiene las zonas de interacción **PROBAR WSL** y **ENVIAR 3C**. La primera comprueba el endpoint; la segunda abre la edición de la orden.

## API del dispositivo

El contrato versionado se encuentra en [contract/device-command-v1.json](contract/device-command-v1.json).

| Operación | Endpoint | Función |
|---|---|---|
| Health | <code>GET /api/device/v1/health</code> | comprobar disponibilidad |
| Crear orden | <code>POST /api/device/v1/commands</code> | enviar una propuesta |
| Estado | <code>GET /api/device/v1/commands/{command_id}</code> | consultar el estado |

El firmware genera una petición con la estructura:

~~~json
{
  "device_id": "panel-4848s040-3c-01",
  "request_id": "panel-4848s040-3c-01-XXXXXXXX-...",
  "text": "Cambia la tarea J10 a mensual"
}
~~~

Cuando hay token configurado, el cliente añade el encabezado <code>X-3C-Device-Token</code>.

El contrato define el protocolo <code>1.0</code>, el transporte HTTP sobre LAN confiable, longitudes máximas para <code>device_id</code>, <code>request_id</code> y <code>text</code>, códigos aceptados de creación/estado, <code>pending_confirmation</code>, autenticación del estado, polling y confirmación humana obligatoria. La escritura directa en una hoja queda explícitamente fuera de este contrato.

### Flujo de estados

~~~text
POST /api/device/v1/commands
          │
          ▼
pending_confirmation
          │
          ├──────────────► applied
          │
          └──────────────► rejected

GET /api/device/v1/commands/{command_id}
          ▲
          │
       polling
       cada 2500 ms

fallo de transporte/protocolo ─────► ERROR
~~~

El firmware consulta el estado cada **2500 ms**. Los estados terminales <code>applied</code> y <code>rejected</code> cierran el ciclo de la orden. Los estados <code>error</code>, <code>failed</code> y <code>fallido</code>, o un estado desconocido, se tratan como error de protocolo.

El ESP32 no ejecuta por sí mismo la confirmación o el rechazo. La aprobación humana pertenece al Monitor web.

### Contrato frente al servicio local de prueba

[backend/device_api.py](backend/device_api.py) es un servicio local de prueba y conserva una implementación simplificada del API. El contrato normativo del dispositivo y la prueba E2E contra el Monitor real utilizan <code>device_id</code>, <code>request_id</code> y <code>text</code>.

Por esa razón, <code>backend/device_api.py</code> se documenta como **harness de prueba**, no como el backend productivo.

La prueba [tests/test_device_api_e2e.py](tests/test_device_api_e2e.py) verifica contra el Monitor real la autenticación, el alta de la orden, la idempotencia por <code>request_id</code>, el estado <code>pending_confirmation</code>, el polling autenticado y el resultado terminal.

## Versiones y dependencias

| Componente | Versión o referencia |
|---|---|
| ESPHome | 2026.8.2 |
| PlatformIO CLI | 6.2.0 |
| plataforma PlatformIO ESP32 | espressif32 6.8.1 |
| GFX Library for Arduino | 1.5.9 |
| Framework PlatformIO | Arduino |
| Framework ESPHome | ESP-IDF |
| i18n externo | commit <code>1b487af0ef26ff8e7908d34e415d99cc13fc1f98</code> |

Las versiones se aseguran desde [scripts/02_tools_guition.sh](scripts/02_tools_guition.sh). La configuración PlatformIO también fija la plataforma y la dependencia GFX.

Las credenciales personales deben permanecer fuera del control de versiones. El patrón disponible es [include/local_config.example.h](include/local_config.example.h); el archivo real <code>include/local_config.h</code> debe mantenerse ignorado por Git.

## Seis bloques de ejecución

El fuente original define seis bloques para WSL/Ubuntu. Como este repositorio se ejecuta sobre el **destino** <code>erp-mantto-esp32</code>, se debe establecer <code>REPO_DIR</code> y utilizar la URL del destino en el Bloque 1.

![Figura 16. Flujo técnico de compilación, carga, monitorización y validación del firmware](docs/images/esp32-s3-4848s040/fig16_validation_flow.png)

**Figura 16. Flujo técnico de compilación, carga, monitorización y validación del firmware.**

La figura resume los seis bloques reproducibles del proyecto: actualización, herramientas, verificación, ESPHome, PlatformIO y carga/monitorización. También separa la evidencia obtenida en GitHub Actions —código, pruebas, contratos y compilación— de la validación física ejecutada mediante `.github/workflows/physical-validation.yml` en un runner `self-hosted` conectado al panel. La ejecución sobre un runner `ubuntu-latest` no demuestra por sí sola la conexión ni el funcionamiento del ESP32-S3, ST7701S, GT911, USB o alimentación reales.

### Block 1 — clone/update

Para un checkout existente del destino:

~~~bash
export REPO_DIR="$PWD"
REPO_URL=https://github.com/wpv10barza/erp-mantto-esp32.git   bash scripts/01_clone_guition.sh
~~~

Este bloque no instala Python ni compila. El script valida que el remoto sea el esperado.

### Block 2 — tools

Crea o utiliza <code>.venv</code> y asegura:

- ESPHome <code>2026.8.2</code>;
- PlatformIO <code>6.2.0</code>.

~~~bash
export REPO_DIR="$PWD"
bash scripts/02_tools_guition.sh
~~~

### Block 3 — verify

Comprueba archivos y contratos críticos: LVGL, 480 × 480, ST7701S, GT911, componente ESPHome fijado, <code>commandBuffer</code>, teclado virtual, ruta API y polling de 2.5 s.

~~~bash
export REPO_DIR="$PWD"
bash scripts/03_verify_guition.sh
~~~

### Block 4 — ESPHome

Valida y compila [src/main.yaml](src/main.yaml).

Para validación con secretos ficticios:

~~~bash
BUILD_MODE=validate bash scripts/04_esphome_guition.sh
~~~

Para un entorno local con secretos reales:

~~~bash
BUILD_MODE=real bash scripts/04_esphome_guition.sh
~~~

### Block 5 — PlatformIO

Ejecuta las pruebas Python y nativas, y construye:

~~~text
panel_4848s040
~~~

~~~bash
bash scripts/05_platformio_guition.sh
~~~

El resultado esperado incluye <code>.pio/build/panel_4848s040/firmware.bin</code>.

### Block 6 — dispositivo físico

Carga el firmware y abre el monitor serie:

~~~bash
PORT=/dev/ttyACM0 bash scripts/06_flash_monitor_guition.sh
~~~

El script acepta o autodetecta <code>/dev/ttyACM*</code> y <code>/dev/ttyUSB*</code>. Rechaza <code>/dev/ttyS*</code>; un nodo como <code>/dev/ttyS0</code> no debe utilizarse como sustituto del USB serial del ESP32.

Si WSL no muestra el dispositivo, comprobar:

~~~bash
ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true
pio device list
lsusb
~~~

La nota de recuperación histórica del fuente indica que una interrupción durante la sustitución de PlatformIO 6.1.19 se reanuda en el **Block 2**.

## Pruebas y evidencia

El repositorio conserva pruebas Python, C++ y E2E:

- [test/command_buffer_regression.py](test/command_buffer_regression.py)
- [test/test_backend_command_buffer/test_main.cpp](test/test_backend_command_buffer/test_main.cpp)
- [test/test_command_text_viewport/test_main.cpp](test/test_command_text_viewport/test_main.cpp)
- [test/test_virtual_keyboard/test_main.cpp](test/test_virtual_keyboard/test_main.cpp)
- [tests/test_command_editor_integration.py](tests/test_command_editor_integration.py)
- [tests/test_device_api_e2e.py](tests/test_device_api_e2e.py)
- [tests/test_panel_state_contract.py](tests/test_panel_state_contract.py)
- [tests/test_wifi_source.py](tests/test_wifi_source.py)
- [e2e/monitor_ui.spec.mjs](e2e/monitor_ui.spec.mjs)

### Qué demuestra cada nivel

| Nivel de evidencia | Demuestra | No demuestra |
|---|---|---|
| Documentación | diseño declarado, contratos y procedimiento | funcionamiento físico |
| Implementación | presencia de código, configuración y dependencias | ejecución exitosa sin compilar/probar |
| Prueba automatizada | resultados de los tests ejecutados | presencia de hardware físico |
| GitHub Actions | validación automática, compilación y E2E software | USB, pantalla, touch o alimentación reales |
| Validación física | comportamiento del ESP32 conectado al hardware | no sustituye las pruebas de software |

## GitHub Actions

### CI

[.github/workflows/ci.yml](.github/workflows/ci.yml) ejecuta sobre <code>ubuntu-latest</code>, usa Python 3.12 y comprueba:

1. sintaxis de los seis scripts;
2. verificación del proyecto;
3. pruebas nativas PlatformIO;
4. contratos Python.

### Firmware CD

[.github/workflows/firmware-cd.yml](.github/workflows/firmware-cd.yml) amplía la validación con:

- ESPHome validate/compile;
- pruebas del Monitor real de <code>wpv10barza/asistente-3c</code>;
- lint y pruebas Node;
- health del Monitor;
- Device API E2E;
- Browser UI E2E con Playwright;
- pruebas nativas;
- compilación de <code>panel_4848s040</code>;
- empaquetado de firmware y manifiesto SHA-256.

El manifiesto distingue explícitamente la evidencia de compilación de la validación física.

### Validación física

[.github/workflows/physical-validation.yml](.github/workflows/physical-validation.yml) es manual y requiere un runner:

~~~text
self-hosted, linux, x64, esp32
~~~

El workflow compila, carga el firmware en el puerto indicado, captura el arranque serie y publica <code>physical-boot.log</code>.

Las verificaciones físicas previstas incluyen:

- ST7701S y renderizado 480 × 480;
- respuesta táctil GT911;
- asociación Wi-Fi;
- PSRAM;
- estabilidad serie por USB-C;
- audio/GPIO cuando el montaje lo incorpora;
- estabilidad de alimentación durante arranque y actividad de red.

Una ejecución de CI en GitHub-hosted runners **no demuestra** por sí sola que un ESP32-S3 real, el ST7701S, el GT911, el USB o la alimentación estén funcionando.

## Inventario de imágenes y ubicación documental

El árbol fuente conserva **84 archivos de imagen** entre `doc/images/` y `src/assets/images/`. Esta consolidación añade cuatro mapas técnicos específicos del ESP32-S3-4848S040 bajo `docs/images/esp32-s3-4848s040/` para documentar condiciones iniciales, subsistemas, buses/GPIO y validación.

Los cuatro mapas se integran en el punto del README donde se explica cada contenido:

- `fig13_panel_base.png` → condiciones iniciales del panel;
- `fig14_subsystems.png` → distribución lógica de subsistemas y periféricos;
- `fig15_gpio_map.png` → buses, señales y GPIO;
- `fig16_validation_flow.png` → compilación, carga, monitorización y validación.

Estas figuras son **elaboración documental** a partir del README vigente y del código del repositorio. No son fotografías de laboratorio ni sustituyen la validación física del panel.

## Estructura del repositorio

La importación conserva la estructura funcional completa:

~~~text
.github/
  ISSUE_TEMPLATE/
  workflows/
    ci.yml
    firmware-cd.yml
    physical-validation.yml
backend/
contract/
doc/
  images/
docs/
  images/
    esp32-s3-4848s040/
      fig13_panel_base.png
      fig14_subsystems.png
      fig15_gpio_map.png
      fig16_validation_flow.png
e2e/
include/
platformio/
  src/
    panel_4848s040/
scripts/
src/
  assets/
  common/
  translations/
  widgets/
test/
tests/
gen_he.py
platformio.ini
LICENSE
README.md
.gitignore
~~~

Los componentes principales son:

- <code>src/</code>: ESPHome + LVGL + ST7701S + GT911.
- <code>platformio/src/panel_4848s040/</code>: firmware 3C, editor, teclado y cliente API.
- <code>include/</code>: command buffer, viewport y teclado.
- <code>backend/</code>: servicio local de prueba.
- <code>contract/</code>: contrato HTTP.
- <code>e2e/</code>, <code>test/</code> y <code>tests/</code>: validación de software.
- <code>scripts/</code>: seis bloques reproducibles.
- <code>.github/workflows/</code>: CI, CD y validación física.

## Documentación complementaria

- [docs/ESP32-FIRMWARE-BACKEND-README.md](docs/ESP32-FIRMWARE-BACKEND-README.md)
- [docs/api-e2e.md](docs/api-e2e.md)
- [docs/physical-validation.md](docs/physical-validation.md)
- [docs/validation-matrix.md](docs/validation-matrix.md)

La documentación académica que utilice esta información debe conservar la lógica **condición → diseño → implementación → evidencia → limitación**, sin crear una numeración paralela dentro de este README.

## Consolidación y comparación

Antes de modificar el destino se leyeron y compararon ambos README completos.

El **README fuente** de <code>wpv10barza/ESP32-S3-4848S040</code>, rama <code>main</code>, tiene 21 108 bytes y blob <code>0c33d6c53a2e0f52a200546396b94cf31b5c7b03</code> en el commit de referencia.

El **README destino preexistente** fue leído antes de la consolidación. Aportaba una descripción más breve de la importación y de la separación entre software y validación física, además de una sección académica extensa con numeración propia. Esa información útil se integró aquí en una sola estructura, mientras que los apartados duplicados, marcadores que no pertenecen al árbol de este repositorio y afirmaciones visuales no respaldadas fueron eliminados o corregidos.

La comparación del árbol funcional produjo:

| Elemento | Fuente | Destino antes de la consolidación | Resultado |
|---|---:|---:|---|
| Archivos fuente | 189 | 189 equivalentes ya importados | 189 conservados |
| Archivos faltantes | 0 después de la importación | 0 | sin faltantes |
| README | diferente | presente | reemplazado por uno integrado |
| Workflows temporales de importación | no existen | presentes | retirados |
| Activos funcionales | presentes | presentes | conservados |

No se hizo una concatenación literal ni una sustitución ciega del README.

## Procedencia

La versión consolidada utiliza como referencia:

- **Fuente:** <code>wpv10barza/ESP32-S3-4848S040</code>
- **Rama:** <code>main</code>
- **Commit fuente:** [4ef0e9a6bd55a3a935d14ae8d84aa8d290114b53](https://github.com/wpv10barza/ESP32-S3-4848S040/commit/4ef0e9a6bd55a3a935d14ae8d84aa8d290114b53)
- **README fuente:** blob <code>0c33d6c53a2e0f52a200546396b94cf31b5c7b03</code>
- **Árbol fuente:** <code>f75e091d38c6672ed16ffcdc7ea78a7b80c7ec52</code>
- **Destino:** <code>wpv10barza/erp-mantto-esp32</code>
- **Rama destino:** <code>main</code>

El firmware, GPIO, ST7701S, GT911, PSRAM, flash, API, estados, temporización, dependencias y pruebas se mantienen según el snapshot fuente utilizado. La consolidación realizada sobre este repositorio es documental y no rediseña la arquitectura funcional.

### Mapas técnicos del panel incorporados desde main

Los cuatro mapas técnicos de las Figuras 13–16 se derivan del `README.md` de `main` de `wpv10barza/ESP32-S3-4848S040` y de los archivos `platformio/src/panel_4848s040/main.cpp`, `platformio.ini`, `src/main.yaml`, `scripts/03_verify_guition.sh`, `scripts/04_esphome_guition.sh`, `scripts/05_platformio_guition.sh`, `scripts/06_flash_monitor_guition.sh`, `.github/workflows/ci.yml`, `.github/workflows/firmware-cd.yml` y `.github/workflows/physical-validation.yml`. La fuente de la rama `main` utilizada para esta actualización corresponde al commit `4ef0e9a6bd55a3a935d14ae8d84aa8d290114b53`.

## Consolidación documental de la fuente técnica

Esta versión del README se consolidó después de leer y comparar directamente el `README.md` de `wpv10barza/ESP32-S3-4848S040` en la rama `main` antes de modificar el repositorio destino.

**Referencia de sincronización**

- Repositorio fuente: `wpv10barza/ESP32-S3-4848S040`
- Rama fuente: `main`
- Commit fuente utilizado: `4ef0e9a6bd55a3a935d14ae8d84aa8d290114b53`
- Árbol fuente: `f75e091d38c6672ed16ffcdc7ea78a7b80c7ec52`
- Blob del README fuente: `0c33d6c53a2e0f52a200546396b94cf31b5c7b03`

La comparación realizada antes de esta edición mostró que el repositorio destino ya contenía el mismo árbol funcional del firmware: ambos repositorios resolvían al árbol `f75e091d38c6672ed16ffcdc7ea78a7b80c7ec52`, con 189 archivos en total y los mismos blobs para la estructura, contratos, pruebas, scripts, PlatformIO, ESPHome y workflows. Por ello, **no fue necesario reemplazar ni eliminar archivos de firmware** en esta consolidación; el destino ya contenía el snapshot completo de la fuente.

### Inventario de imágenes

El README fuente no contiene referencias Markdown a imágenes. El árbol del proyecto sí conserva:

- 16 imágenes de documentación bajo `doc/images/`;
- los activos de interfaz bajo `src/assets/images/`;
- las fuentes bajo `src/assets/fonts/`.

Las 16 imágenes de `doc/images/` ya están presentes en el destino y coinciden con los mismos SHA del repositorio fuente, por lo que no fue necesario copiarlas nuevamente ni introducir duplicados. Ninguna imagen se presenta aquí como evidencia de funcionamiento físico del panel.

### Niveles de evidencia

La documentación distingue entre:

**Documentación:** lo declarado por el README y los archivos de configuración.

**Implementación:** lo presente en el código y en el árbol del repositorio.

**Pruebas automatizadas:** pruebas Python/C++ y ejecuciones definidas por los workflows.

**GitHub Actions:** validaciones de código, contratos, compilación y pruebas que el workflow realmente ejecuta.

**Validación física:** requiere un ESP32-S3 real conectado al hardware; la CI en la nube no demuestra por sí sola el funcionamiento eléctrico del ST7701S o del GT911.

Esta consolidación conserva la arquitectura, GPIO, contratos, estados, temporización y dependencias documentadas por la fuente técnica y no constituye un rediseño del firmware.
