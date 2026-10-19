# Documentación técnica del panel ESP32-S3-4848S040 y su integración con el Asistente 3C

Este documento presenta las condiciones iniciales, el diseño, la implementación, la validación y la puesta en marcha del panel táctil ESP32-S3-4848S040 integrado al Asistente 3C. La exposición sigue una jerarquía decimal compatible con la organización académica T-030 de la Universidad Ricardo Palma. Se describen los componentes y criterios de funcionamiento sin incluir, en esta versión, instrucciones ejecutables ni fragmentos de código. La documentación del README anterior se conserva íntegra en `README_ORIGINAL.md`.

## 1. Condiciones iniciales

Las condiciones iniciales delimitan el equipamiento, las interfaces físicas y las dependencias de desarrollo necesarias para estudiar el sistema. La identificación de un componente en el repositorio demuestra una decisión de diseño o una configuración registrada, pero no constituye por sí sola evidencia de una prueba satisfactoria sobre el dispositivo físico.

### 1.1. Plataforma electrónica ESP32-S3-4848S040 de 480 × 480 píxeles

El prototipo emplea el panel Guition ESP32-S3-4848S040, basado en el microcontrolador ESP32-S3 y una pantalla cuadrada de 480 × 480 píxeles. La visualización utiliza un controlador de la familia ST7701, identificado como ST7701S en la documentación del proyecto, y la interacción táctil se gestiona mediante el GT911. El objetivo de compilación de PlatformIO denominado `panel_4848s040` registra una configuración de 16 MB de memoria flash, PSRAM de tipo OPI y framework Arduino. Estas propiedades deben distinguirse de la verificación de la capacidad y del comportamiento del equipo físico concreto.

### 1.2. Conexiones principales GPIO

La asignación de entradas y salidas de propósito general (GPIO) se documenta en `include/panel_gpio.h`. Según ese mapa, la retroiluminación utiliza GPIO38; el enlace de control de la pantalla emplea GPIO39 como selección de dispositivo, GPIO48 como reloj y GPIO47 como datos de salida. La interfaz I²C del GT911 utiliza GPIO19 para datos y GPIO45 para reloj. Las señales de sincronización RGB se asignan a GPIO18, GPIO17, GPIO16 y GPIO21 para DE, VSYNC, HSYNC y PCLK, respectivamente. Las líneas de color rojo utilizan GPIO11, GPIO12, GPIO13, GPIO14 y GPIO0; las verdes, GPIO8, GPIO20, GPIO3, GPIO46, GPIO9 y GPIO10; y las azules, GPIO4, GPIO5, GPIO6, GPIO7 y GPIO15. Las líneas GPIO1, GPIO2 y GPIO40 se encuentran identificadas en el mapa para funciones auxiliares y no deben confundirse con el bus RGB o la interfaz táctil. La coincidencia de GPIO19 y GPIO20 con recursos asociados a USB en el ESP32-S3 exige comprobar las restricciones de uso durante las pruebas físicas.

### 1.3. Condiciones de software WSL y PlatformIO

El entorno de desarrollo contempla Windows con Windows Subsystem for Linux (WSL), una distribución Linux de trabajo y PlatformIO como sistema de compilación y gestión de dependencias del firmware. El repositorio define un objetivo específico para el panel y objetivos adicionales de pruebas, por lo que la selección de un entorno debe corresponder a la función que se desea evaluar. La preparación en WSL y la construcción del firmware son actividades de desarrollo, mientras que la carga mediante un puerto serie USB y la observación directa del comportamiento requieren verificar el acceso efectivo al dispositivo. La ejecución remota de pruebas o de integración continua no sustituye la comprobación eléctrica de pantalla, tacto y conectividad.

### 1.4. Configuración local `local_config.h`

El archivo `include/local_config.h` concentra los parámetros privados o variables del despliegue y se mantiene fuera del control de versiones. Su estructura se obtiene a partir del ejemplo público `include/local_config.example.h`, que identifica los campos de conectividad Wi-Fi, servicio de aplicación, espacio de trabajo Databricks, credenciales OAuth de la entidad de servicio, token de dispositivo, identificador del panel y preferencias de interfaz. Las credenciales no deben incorporarse al README, a los archivos de prueba ni al historial de Git. En la arquitectura productiva descrita en el repositorio, el panel necesita una red Wi-Fi de 2,4 GHz con salida a Internet, pero no requiere compartir una subred local con el servidor de aplicación.

## 2. Diseño del sistema

El diseño separa la interacción del operador, el procesamiento gráfico y táctil, el transporte seguro de mensajes, la interpretación de solicitudes y la autorización de los cambios. El panel actúa como terminal de interacción y seguimiento; el servicio de aplicación conserva la responsabilidad de las validaciones y de la gestión del ciclo de vida de las operaciones.

### 2.1. Diseño electrónico

El diseño electrónico integra el ESP32-S3, la pantalla RGB, las señales de configuración del ST7701S, el controlador táctil GT911, la retroiluminación y los recursos de alimentación y comunicación. La documentación de pines se mantiene centralizada para disminuir el riesgo de asignaciones incompatibles entre periféricos. La disponibilidad de memoria externa y las condiciones de sincronización de la pantalla deben ser coherentes con la resolución prevista, las exigencias de actualización gráfica y el consumo de memoria del firmware. La distribución documentada permite preparar verificaciones de continuidad funcional y de ausencia de conflictos entre recursos, sin presumir resultados de laboratorio no registrados.

### 2.2. Diseño del software

El software diferencia la interfaz embebida del panel y la lógica de aplicación del Asistente 3C. En el firmware PlatformIO se organizan la inicialización periférica, el editor de comandos, la representación de estados y el cliente de comunicación. En el servicio externo se reciben solicitudes, se interpreta su contenido, se realizan comprobaciones determinísticas y se coordina la revisión humana previa a la persistencia autorizada. El repositorio conserva, por separado, un objetivo ESPHome con LVGL; esta alternativa no implica que la interfaz PlatformIO utilice LVGL como dependencia directa. La ruta de producción descrita para el firmware conecta el panel por HTTPS con Databricks Apps y no depende de un backend local en WSL para su operación ordinaria.

### 2.3. Diseño de la interfaz táctil

La interfaz se organiza para presentar opciones de conectividad, diagnóstico, consulta del servicio y envío de órdenes al Asistente 3C. La pantalla principal prioriza las funciones operativas y reserva vistas específicas para introducir instrucciones, revisar mensajes y conocer el estado de las solicitudes. El diseño del editor debe considerar el área disponible de 480 × 480 píxeles, el tamaño legible de los controles, la retroalimentación visual y la preservación del texto del usuario durante actualizaciones de estado. La presentación de información técnica no debe confundirse con una confirmación automática de que un procedimiento externo se completó.

### 2.4. Tratamiento táctil

El tratamiento táctil comprende la adquisición de coordenadas desde el GT911, su correspondencia con la orientación de la pantalla, la identificación de la región pulsada y el control de pulsaciones consecutivas. Se consideran mecanismos de filtrado temporal, seguimiento de eventos y prevención de activaciones involuntarias. La manipulación de un campo de texto debe conservar la posición del cursor y permitir la inserción o eliminación de caracteres sin alterar comandos en edición por efectos de actualizaciones asíncronas. La calibración y la precisión de las coordenadas se comprueban sobre el panel real.

## 3. Implementación

La implementación corresponde a la integración progresiva de los módulos físicos y lógicos descritos en el diseño. Los apartados siguientes registran las responsabilidades técnicas de cada componente, sin presentar código fuente ni atribuir resultados de prueba no documentados.

### 3.1. Inicialización del hardware

La secuencia de arranque comprende la configuración del microcontrolador, la disponibilidad de memoria y las interfaces utilizadas por la pantalla, el tacto y la conectividad. El firmware debe iniciar los periféricos en un orden que preserve las dependencias eléctricas y lógicas, e informar sobre fallos cuando un recurso indispensable no responda. La activación de la retroiluminación y la habilitación de la interfaz se condicionan a la preparación de los subsistemas correspondientes. Los registros de compilación y arranque se consideran fuentes de evidencia diferentes.

### 3.2. Pantalla ST7701

El subsistema de visualización emplea el controlador ST7701S y un enlace RGB para transportar la información de píxeles, con señales de control que permiten establecer la secuencia de inicialización. La versión observada en los archivos fuente del repositorio utiliza una secuencia de inicialización de la variante ST7701 tipo 9 mediante Arduino-GFX. Esta precisión evita confundir registros anteriores que mencionan otra variante. El funcionamiento correcto exige verificar sincronización, orientación, colores, ausencia de cortes y estabilidad de la retroiluminación en una prueba física.

### 3.3. Táctil GT911

El controlador GT911 se comunica por I²C y proporciona la información necesaria para determinar interacciones sobre la superficie visible. La implementación contempla la lectura del estado táctil y de las coordenadas, así como su transformación a regiones funcionales de la interfaz. La correspondencia entre imagen y tacto debe verificarse en esquinas, bordes, teclas virtuales y áreas de confirmación. La detección de un dispositivo I²C no demuestra por sí sola la exactitud del mapeo táctil.

### 3.4. Editor de comandos

El editor permite redactar instrucciones destinadas al Asistente 3C desde el dispositivo. La entrada comprende teclado virtual, cambios de modo para caracteres y símbolos, desplazamiento del contenido, tratamiento de borrado e inserción y conservación de la posición de edición. La orden redactada se distingue del comando aceptado por el servicio: su envío inicia un proceso de recepción y comprobación, pero no autoriza de manera inmediata la modificación de una fuente maestra. El texto en proceso de edición debe permanecer protegido frente a actualizaciones periódicas del estado de red o del servicio.

### 3.5. Backend 3C

El backend recibe la identidad del dispositivo y la solicitud, coordina las etapas de interpretación y validación, y conserva la trazabilidad del resultado. En la configuración productiva vigente del repositorio, la comunicación se realiza mediante HTTPS hacia Databricks Apps, con autenticación OAuth 2.0 de máquina a máquina y un token adicional de dispositivo según el contrato de la aplicación. La aceptación de una petición no constituye autorización de persistencia; la aplicación efectiva de cambios exige la revisión humana definida en el sistema. Las integraciones con fuentes de mantenimiento o con Google Sheets se documentan como responsabilidades del servicio y no como operaciones directas del panel.

### 3.6. Estados

Los estados del panel permiten distinguir disponibilidad, conexión, envío, espera de confirmación, aplicación, rechazo y error. El contrato del dispositivo reconoce la espera de revisión mediante `pending_confirmation` y los resultados terminales `applied` y `rejected`; los fallos de transporte, autenticación o protocolo deben representarse de modo diferenciado. El firmware consulta periódicamente el estado remoto y actualiza la interfaz sin intervenir en la decisión humana. El estado mostrado se vincula a una operación identificada y no debe interpretarse como comprobación universal de integridad de los sistemas externos.

## 4. Validación y pruebas

La validación distingue las verificaciones estáticas del repositorio, las pruebas de componentes y la evaluación física del equipo. Las primeras comprenden coherencia del mapa GPIO, correspondencia entre contratos de software, integridad del editor, tratamiento de estados y construcción del objetivo PlatformIO. Las pruebas de integración deben revisar conectividad Wi-Fi, autenticación, disponibilidad de la API, envío y consulta de solicitudes, ciclo de revisión humana y coherencia entre estado reportado y resultado del servicio. Las pruebas físicas deben incluir alimentación, calidad de visualización, sensibilidad y precisión táctil, respuesta del teclado virtual, recuperación de red y estabilidad operativa del panel.

Cada comprobación debe registrar su contexto, versión de firmware, identificador del dispositivo, procedimiento, resultado observado y evidencia disponible. Una compilación satisfactoria, una ejecución de integración continua o una respuesta HTTP correcta acreditan únicamente los aspectos evaluados y no deben presentarse como validación completa de la interacción táctil ni de la persistencia final. Cuando una comprobación no se ha ejecutado, su estado debe mantenerse expresamente como pendiente.

## 5. Puesta en marcha

La puesta en marcha se realiza sobre una versión identificada del firmware y una configuración local válida que no exponga credenciales. Antes de la carga se revisan las dependencias de PlatformIO, la correspondencia con el objetivo de placa y la disponibilidad de la interfaz de programación adecuada. Tras el arranque físico se comprueban el inicio de la pantalla, el registro táctil, la conexión a Internet, la autenticación con el servicio de aplicación y la presentación de mensajes de disponibilidad o error. La operación de prueba continúa con una instrucción controlada y el seguimiento de su estado hasta identificar un resultado terminal o una incidencia trazable.

La aceptación de la puesta en marcha requiere que la revisión humana y la persistencia autorizada se verifiquen en la fuente correspondiente cuando formen parte del alcance de la prueba. La publicación de una versión o la actualización remota del firmware no sustituyen las comprobaciones posteriores al despliegue. Los resultados se incorporarán al registro de validación con sus evidencias y limitaciones; esta edición documental no afirma que se haya ejecutado una nueva carga ni una nueva prueba física.

## Control desde Windows PowerShell

Use `scripts/Menu-ESP32.ps1 -DeviceUrl http://IP_DEL_ESP32` para seleccionar las mismas funciones del menú principal y ejecutarlas mediante ACTIVAR. Consulte [el procedimiento HTTP REST](docs/ACTIVACION_HTTP_REST.md) para autenticación, correspondencias y límites.
