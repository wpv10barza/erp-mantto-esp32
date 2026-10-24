# Compartir el proyecto 3C con una segunda cuenta

Destino solicitado: `polovaleriow@gmail.com`.

## Estado comprobado

- Google Sheet `Estrategias_20260527_2119`: esta cuenta **ya es propietaria**.
  No se requiere permiso adicional. Se detecto un permiso peligroso:
  `anyone = writer`. Debe cambiarse **Compartir -> Acceso general ->
  Restringido**, manteniendo los permisos directos del propietario y
  las cuentas de servicio autorizadas.
- Firmware `erp-mantto-esp32`, backend
  `asistente-de-databricks--erp` y cliente
  `voice-embeding-in-databricks-for-esp32` son **publicos**:
  lectura y clone disponibles. Colaborar mediante push si requiere
  una invitacion GitHub a su **usuario de GitHub** (no inferirlo por correo).
- Databricks Apps `asistente-cloud-erp`: la App autentica usuarios.
  No se puede dar acceso anonimo. Se necesita identidad del destinatario
  en el MISMO account/workspace (o aprovisionamiento SCIM/JIT compatible)
  y permiso de App `CAN_USE` o `CAN_MANAGE`.

## Ejecutar desde la carpeta firmware de GitHub

Abrir PowerShell y actualizar el codigo, conservando los cambios locales:

~~~powershell
cd C:\Users\ASUS\erp-mantto-esp32-deploy
git pull --ff-only origin main
~~~

### 1. Auditar antes de escribir

~~~powershell
.\scripts\Compartir-Proyecto-3C.ps1
~~~

El modo predeterminado consulta el usuario y los permisos actuales sin
crear usuarios ni modificar recursos.

### 2. Compartir la administracion de la App

**Solo desde un perfil Databricks con autoridad para administrar usuarios
y con permiso CAN MANAGE sobre la App:**

~~~powershell
.\scripts\Compartir-Proyecto-3C.ps1 -Aplicar -Nivel CAN_MANAGE
~~~

El script (a) busca el usuario exacto, (b) lo crea en el workspace solo
si falta, (c) utiliza `apps update-permissions`, **no**
`set-permissions`, para conservar los permisos actuales de otros usuarios;
(d) consulta otra vez y verifica `CAN_MANAGE`.

**CAN_MANAGE permite cambiar ajustes, permisos e incluso eliminar la App.**
Si el destinatario solo debe usarla:

~~~powershell
.\scripts\Compartir-Proyecto-3C.ps1 -Aplicar -Nivel CAN_USE
~~~

No concede automaticamente lectura de otros secretos, Unity Catalog ni
acceso a credenciales de Google. El service principal de la App es
independiente del usuario que la administra.

### 3. Conceder push en GitHub

Primero compruebe el **usuario GitHub real** asociado al destinatario.
No use la direccion Gmail como si fuese un nombre de usuario:

~~~powershell
.\scripts\Compartir-Proyecto-3C.ps1 -GitHubUsuario GITHUB_USUARIO_REAL
# Cuando haya confirmado identidad y nivel:
.\scripts\Compartir-Proyecto-3C.ps1 -Aplicar -Nivel CAN_MANAGE -GitHubUsuario GITHUB_USUARIO_REAL
~~~

Se envian invitaciones separadas a los tres repositorios principales.
No se concede acceso de propietario a la cuenta GitHub y la colaboracion
queda pendiente hasta que la otra persona acepte la invitacion.

### 4. Si son dos cuentas Databricks completamente distintas

`CAN_MANAGE` en una App de un workspace NO crea una copia en otro account.
Para migrar: clonar repos Git en otro workspace, crear la App destino,
vincular recursos de nuevo, crear secretos propios, otorgar acceso
de Google Sheets a su service account con los permisos minimos necesarios,
configurar volumen OTA si procede, y desplegar un SHA probado.

**Nunca copiar** JSON de service accounts, PAT, Gemini API keys,
`reviewer_token`, credenciales OAuth ni `local_config.h` entre cuentas
por Git o correo. El firmware requiere su propia configuracion privada
y flasheo/OTA de hardware correcto.

### 5. Comprobacion final

1. Databricks: `databricks apps get-permissions asistente-cloud-erp --profile asistente-cloud-erp`.
2. Usuario nuevo: iniciar sesion con `polovaleriow@gmail.com` y abrir
   `https://asistente-cloud-erp-7474651957738908.aws.databricksapps.com`.
3. GitHub: aceptar cada invitacion; clonar las tres carpetas.
4. Google Sheets: abrir el archivo siendo propietario y confirmar
   acceso **Restringido** y roles de los service accounts.
5. Panel ESP32: flasheo a traves de PowerShell en el equipo conectado
   por USB; la App no carga firmware directamente a la placa.

Documentacion oficial:
- https://docs.databricks.com/aws/en/dev-tools/databricks-apps/permissions
- https://docs.databricks.com/aws/en/dev-tools/cli/reference/apps-commands
- https://docs.github.com/en/repositories/managing-your-repositorys-settings-and-features/repository-access-and-collaboration/inviting-collaborators-to-a-personal-repository
