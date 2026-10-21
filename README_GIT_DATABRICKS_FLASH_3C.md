# Desde Git: Databricks Apps + firmware ESP32-S3 4848S040

## Orden verificado: primero Databricks, luego pull y flash

Siempre desde el archivo versionado del repositorio firmware (no use un
.ps1 descargado aparte), una vez creado y vinculado el secreto de revisor:

    cd C:\Users\ASUS\erp-mantto-esp32
    git status --short
    git pull --ff-only origin main

    # 1. Actualiza backend Git a su rama protegida y despliega en Databricks
    .\scripts\Actualizar-3C-DesdeGit.ps1 -Modo Backend -RevisorVinculado

    # 2. Solo si el backend responde HTTP 200:
    #    Git pull del firmware main y flash con el procedimiento ya usado
    .\scripts\Flash-ESP32-3C.ps1 -ListPorts
    .\scripts\Actualizar-3C-DesdeGit.ps1 -Modo Firmware -Puerto COM9

Este orden NO obtiene firmware desde Databricks: la fuente es GitHub.
El comando Backend comprueba /api/health y que el modo Sheets esta
habilitado. El comando Firmware reutiliza Flash-ESP32-3C.ps1;
no borra NVS y escribe un registro de flash. El dispositivo debe
ser revisado fisicamente para confirmar el commit instalado y la
edicion aprobada de Google Sheets. Si Databricks devuelve HTTP 503,
detenga el proceso antes de probar una orden real.


La fuente unica son los repositorios GitHub de la cuenta wpv10barza. NO se usa
ningun archivo PowerShell descargado aparte.

Repositorio firmware: https://github.com/wpv10barza/erp-mantto-esp32 (main).
Repositorio backend: https://github.com/wpv10barza/asistente-de-databricks--erp
(rama feature/voice-frequency-review-sheets-secure-20261019, pendiente de
vincular el secreto revisor).
Repositorio cliente de voz: https://github.com/wpv10barza/voice-embeding-in-databricks-for-esp32 (main).

## Sincronizar los dos repositorios en PowerShell

    cd C:\Users\ASUS\erp-mantto-esp32
    git status --short
    git pull --ff-only origin main
    .\scripts\Actualizar-3C-DesdeGit.ps1 -Modo Sincronizar

El script clona el backend al directorio hermano
C:\Users\ASUS\asistente-de-databricks--erp si falta. Si hay cambios
locales versionados, se detiene: no usa reset --hard ni git clean.
No borra NVS ni muestra secretos o la configuracion privada.

## Activar la escritura protegida en Databricks

Antes de ejecutar el despliegue, cree y vincule en Databricks Apps un
recurso de tipo Secret con resource key review_approval_token, permiso Can read.
Su valor debe ser una clave de revisor independiente de ESP32_API_TOKEN,
de credenciales Wi-Fi y del OAuth de Databricks.

La rama del backend habilita ALLOW_SHEET_WRITE=true, pero aprobar/aplicar
requiere esa clave de revisor. Sin el recurso vinculado, no despliegue.
Revise los permisos de la app y rote los secretos compartidos previamente.

    cd C:\Users\ASUS\erp-mantto-esp32
    .\scripts\Actualizar-3C-DesdeGit.ps1 -Modo Backend -RevisorVinculado

Este comando despliega desde el commit Git completo obtenido por git
rev-parse HEAD, mediante databricks apps deploy --git-commit y comprueba
/api/health. Health por si solo NO confirma una escritura.

Pagina de revision, tras desplegar:
https://asistente-cloud-erp-7474651957738908.aws.databricksapps.com/review-3c

El usuario dicta y pulsa ENTER en el ESP32. Para una orden clara como
'cambiar la tarea 102497 a seis meses', el backend busca exactamente
un TareaId, propone L=6 y M=Mes, sin escribir automaticamente.
El revisor inspecciona celda, antes/despues, pulsa Aprobar y,
en otra accion, Aplicar. La app relee Sheets y solo entonces indica
APLICADO/VERIFICADO. Una orden ambigua no crea propuesta automatica.

## Flashear desde Git en Windows

La placa debe estar conectada por USB. Detecte el puerto COM real:

    cd C:\Users\ASUS\erp-mantto-esp32
    .\scripts\Flash-ESP32-3C.ps1 -ListPorts
    .\scripts\Actualizar-3C-DesdeGit.ps1 -Modo Firmware -Puerto COM9

O, una vez vinculado el recurso de revisor, despliegue y flashee en orden:

    .\scripts\Actualizar-3C-DesdeGit.ps1 -Modo Todo -Puerto COM9 -RevisorVinculado

El script se detiene si Databricks no responde. La carga USB invoca
scripts/Flash-ESP32-3C.ps1 que verifica COM, trabaja en main limpio,
conserva NVS y genera log de flash. Compruebe despues por serial la
linea FIRMWARE INSTALLED git_commit= y la lectura real de Sheets.

## Actualizar la voz Windows desde su repositorio

    cd C:\Users\ASUS\erp-mantto-esp32\voice-embeding-in-databricks-for-esp32
    git pull --ff-only origin main

Si el cliente no esta en esa ruta, haga git pull dentro de su
checkout real, sin mover ni borrar sus datos locales.

## Volver a un commit anterior de Databricks

Databricks permite desplegar un commit de backend anterior:
databricks apps deploy asistente-cloud-erp --git-commit SHA_40_CARACTERES
--profile asistente-cloud-erp --timeout 10m.

No use esta instruccion para el firmware: un commit Git no es un
binario flasheable. Primero compilar y subir USB/OTA verificado.
