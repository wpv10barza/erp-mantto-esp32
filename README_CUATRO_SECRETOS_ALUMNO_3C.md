# Cuatro secretos del workspace del alumno — PowerShell

Destino: perfil Databricks CLI alumno-3c, usuario esperado polovaleriow@gmail.com,
scope alumno-3c. No se reutilizan los secretos del Databricks original.

## Requisitos

1. Autenticar alumno:
    databricks auth login --host https://SU-NUEVO-WORKSPACE --profile alumno-3c
2. Una clave valida obtenida por el alumno en Google AI Studio.
3. Una cuenta de servicio nueva en un proyecto Google Cloud autorizado
   con API Sheets habilitada y una credencial JSON propia en una ruta PRIVADA.
4. La hoja Google Sheets debe compartirse con el email de esa identidad
   (Reader para lectura; Editor solo si se autorizan escrituras con revision).
   Antes de autorizar edicion, retirar el permiso anyone=writer actual.

## Uso desde el repositorio versionado

PowerShell:

    cd C:\Users\ASUS\erp-mantto-esp32-deploy
    git pull --ff-only origin main
    if ($LASTEXITCODE -ne 0) { throw 'Revisar Git antes de seguir.' }
    .\scripts\Completar-4-Secretos-Alumno-3C.ps1

Si ya conoce la ruta, anada:
    -GoogleJsonPath "C:\ruta\privada\service-account-alumno.json"

Introduce la clave Gemini en campo oculto y la ruta del archivo JSON si
faltan. El script usa STDIN UTF-8, sin secretos en argumentos de CLI,
archivos temporales de payload ni Notepad. No sobrescribe secretos presentes.
Genera aleatoriamente dos tokens solo si faltan.

Nombres: gemini_api_key, google_service_account_json, esp32_api_token y
reviewer_token. El volumen workspace.default.ota_firmware es independiente.

## Crear una nueva identidad Google desde PowerShell (opcional)

Requiere el Cloud SDK gcloud, un proyecto propio y permisos IAM:

    gcloud auth login
    $GoogleProyecto = Read-Host 'Project ID de Google Cloud'
    gcloud config set project $GoogleProyecto
    gcloud services enable sheets.googleapis.com --project $GoogleProyecto
    $CuentaServicio = "erp-3c-alumno@$GoogleProyecto.iam.gserviceaccount.com"
    gcloud iam service-accounts describe $CuentaServicio --project $GoogleProyecto

Solo si la identidad no existe, crearla:

    gcloud iam service-accounts create erp-3c-alumno --project $GoogleProyecto

Si el backend exige JSON y el administrador permite este riesgo, crear
la clave en una carpeta privada fuera de Git y carpetas sincronizadas:

    gcloud iam service-accounts keys create RUTA_PRIVADA_JSON --iam-account $CuentaServicio --project $GoogleProyecto

Sustituir RUTA_PRIVADA_JSON por ruta completa ya protegida.
No enviar el JSON por correo/chat ni versionarlo. Preferir federacion
de identidad y credenciales de corta vida cuando sea compatible.

## Verificacion sin exponer secretos

    databricks secrets list-secrets alumno-3c --profile alumno-3c

La presencia de los cuatro nombres no prueba aun acceso real a Gemini
ni lectura real de Google Sheets. Luego se vinculan los recursos de App
con valueFrom y se prueban los endpoints correspondientes.
No usar el script antiguo de notepad.txt con secretos en texto plano.
