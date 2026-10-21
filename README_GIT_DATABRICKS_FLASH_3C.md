# Desde Git: Databricks Apps + firmware ESP32-S3 4848S040

## Error comprobado: reviewer_token contiene byte NUL

Si el deploy informa `An environment variable contains an invalid character
(NUL byte)` y una inspeccion segura del scope `asistente-cloud-erp`
determino que `reviewer_token` tiene NUL, reemplace **solo su valor**:
mantenga el secret scope, secret key y resource key
`review_approval_token` vinculados a la App.

En PowerShell (modo interactivo; NO pase la nueva clave como argumento
literal `--string-value` ni publique el valor en Git):

~~~powershell
databricks secrets put-secret asistente-cloud-erp reviewer_token --profile asistente-cloud-erp
~~~

Introduzca un token ASCII aleatorio de al menos 32 caracteres, generado
con un gestor de contrasenas. Databricks sobrescribe el valor anterior
sin necesitar borrar ni crear el recurso. Confirme despues con una
consulta que solo muestra estado, nunca el secreto:

~~~powershell
$r = databricks secrets get-secret asistente-cloud-erp reviewer_token --profile asistente-cloud-erp --output json | ConvertFrom-Json
if ($LASTEXITCODE -ne 0) { throw "Error de lectura reviewer_token" }
$b = [Convert]::FromBase64String($r.value)
[PSCustomObject]@{ NUL = ($b -contains [byte]0); Bytes = $b.Length }
~~~

Solo con `NUL=False` y al menos 32 bytes ejecutar el script desde Git:

~~~powershell
cd C:\Users\ASUS\erp-mantto-esp32-deploy
git pull --ff-only origin main
.\scripts\Actualizar-3C-DesdeGit.ps1 -Modo Backend -RevisorVinculado
~~~

El script ahora realiza la comprobacion de codificacion del token
revisor antes de iniciar y desplegar Databricks. Si la App
continua fallando, consulte `databricks apps logs` sin imprimir
ninguna credencial. Esto no cambia Google Sheets ni flashea USB.


## Error corregido 21/oct/2026: App not in RUNNING state

Si el recurso de revision ya existe pero Databricks devuelve
`Cannot deploy app ... as it is not in RUNNING state`, no vuelva
a crear recursos ni modifique credenciales. El script versionado
`scripts/Actualizar-3C-DesdeGit.ps1` ahora consulta `compute_status.state`
y `app_status.state`. Si el compute esta `STOPPED`, ejecuta
`databricks apps start asistente-cloud-erp --timeout 10m`, vuelve
a consultar ambos estados y despliega el SHA exacto unicamente cuando
`compute_status.state=ACTIVE` y `app_status.state=RUNNING`.
Si hay `CRASHED`, `ERROR` o estados de transicion, se detiene
para consultar Logs, sin forzar una escritura ni el flash USB.

Desde la copia limpia de firmware en PowerShell:

~~~powershell
cd C:\Users\ASUS\erp-mantto-esp32-deploy
git pull --ff-only origin main
.\scripts\Actualizar-3C-DesdeGit.ps1 -Modo Backend -RevisorVinculado
~~~

El switch `-RevisorVinculado` solo se utiliza si la App tiene el
resource key `review_approval_token` realmente vinculado a un
secreto de lectura. La imagen de Settings debe mostrarlo como otra
fila de Secret, conservando los recursos existentes. El error de
estado de ejecución no requiere cambiar `app.yaml` ni el branch
`feature/voice-frequency-review-sheets-secure-20261019`.


## Recuperación 21/oct/2026: main divergente + script ausente + sin COM

Si en C:\Users\ASUS\erp-mantto-esp32 aparece:
- git pull --ff-only origin main: fatal: Not possible to fast-forward
- scripts/Actualizar-3C-DesdeGit.ps1 no se reconoce
- Flash-ESP32-3C.ps1 -ListPorts: Windows no detecta ningun puerto COM

**No fuerce el pull ni flashee COM9.** La causa del script ausente es que
la actualización no se completó. Las carpetas untracked (logs/,
ota-release/, voice-embeding-in-databricks-for-esp32/) pertenecen al
checkout local y se conservan. El fallo de USB es independiente de Git.

### Recuperación sin tocar los commits y archivos de la carpeta original

Primero registre la evidencia de la rama local (sin alterar archivos):

~~~powershell
cd C:\Users\ASUS\erp-mantto-esp32
git fetch origin main
$respaldo = "backup/local-$(Get-Date -Format yyyyMMdd-HHmmss)"
git branch $respaldo HEAD
git log --left-right --cherry-pick --oneline HEAD...origin/main
~~~

Para usar **exactamente origin/main sin alterar el checkout anterior**,
clone a una nueva carpeta en el mismo equipo:

~~~powershell
cd C:\Users\ASUS
git clone --branch main https://github.com/wpv10barza/erp-mantto-esp32.git erp-mantto-esp32-deploy
if ($LASTEXITCODE -ne 0) { throw 'No se pudo clonar el firmware Git' }
cd .\erp-mantto-esp32-deploy
git rev-parse HEAD
Test-Path .\scripts\Actualizar-3C-DesdeGit.ps1
~~~

Esta carpeta **no incluye local_config.h privado**, que fue excluido
de Git correctamente. Antes del flash, si el archivo anterior existe,
cópielo de manera local, sin imprimirlo ni agregarlo a Git:

~~~powershell
$anterior = 'C:\Users\ASUS\erp-mantto-esp32\include\local_config.h'
if (Test-Path -LiteralPath $anterior) {
    Copy-Item -LiteralPath $anterior -Destination .\include\local_config.h
}
git check-ignore .\include\local_config.h
~~~

Con los secretos de Databricks ya vinculados, primero ejecute la etapa
backend (solo nube, sin puerto COM):

~~~powershell
.\scripts\Actualizar-3C-DesdeGit.ps1 -Modo Backend -RevisorVinculado
~~~

No utilice -RevisorVinculado antes de configurar efectivamente el
recurso review_approval_token en la App. Si el despliegue falla, no
continúe al firmware.

Una vez que Windows detecte realmente el ESP32 USB, reutilice el flash
habitual desde esta carpeta de Git:

~~~powershell
.\scripts\Flash-ESP32-3C.ps1 -ListPorts
py -m platformio device list
.\scripts\Actualizar-3C-DesdeGit.ps1 -Modo Firmware -Puerto COM9
~~~

Sustituya COM9 por el puerto detectado; con cero puertos, ese último
comando **no debe ejecutarse**. Pruebe cable de datos, conexión física,
administrador de dispositivos y modo BOOT/RESET/EN. No borre NVS.

Para conciliar más adelante los commits de la carpeta original,
compare primero git log --left-right; no use reset --hard ni rebase
a ciegas. La carpeta de despliegue es independiente y permite continuar
sin perder los trabajos locales.


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
