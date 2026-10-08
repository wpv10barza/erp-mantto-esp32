# ESP32-S3-4848S040 → Databricks Apps

Firmware del panel **ESP32-S3-4848S040 (480×480)** conectado al backend cloud `asistente-cloud-erp` desplegado en Databricks Apps.

## Arquitectura de producción

```text
ESP32-S3-4848S040
  │ Wi-Fi
  │
  ├─ OAuth 2.0 M2M (Databricks service principal)
  │    POST <workspace>/oidc/v1/token
  │    access_token en RAM, renovación antes de 1 h
  │
  └─ HTTPS + Authorization: Bearer <token>
       + X-3C-Device-Token
            │
            ▼
Databricks Apps / asistente-cloud-erp
            │
            ├─ GET  /api/device/v1/health
            ├─ POST /api/device/v1/commands
            └─ GET  /api/device/v1/commands/{command_id}
            │
            ▼
Revisión humana → Google Sheets
```

El ESP32 **no escribe directamente en Google Sheets**. La orden queda en `pending_confirmation` hasta que la revisión humana determine `applied` o `rejected`.

## Cambio respecto al firmware WSL

La URL de producción ya no se descubre mediante mDNS. `ASSISTANT_BASE_URL_VALUE` apunta a Databricks Apps y tiene prioridad. El mecanismo `_3c._tcp` se conserva únicamente como fallback de desarrollo cuando la URL cloud se deja vacía.

La interfaz del panel muestra **DATABRICKS LISTO** y el botón **PROBAR CLOUD**.

## Configuración local segura

Copiar `include/local_config.example.h` a `include/local_config.h` y completar solo los valores privados:

```cpp
#define WIFI_SSID_VALUE "..."
#define WIFI_PASSWORD_VALUE "..."
#define ASSISTANT_BASE_URL_VALUE "https://asistente-cloud-erp-7474651957738908.aws.databricksapps.com"
#define DATABRICKS_WORKSPACE_URL_VALUE "https://dbc-a1aca8aa-28bd.cloud.databricks.com"
#define DATABRICKS_CLIENT_ID_VALUE "..."
#define DATABRICKS_CLIENT_SECRET_VALUE "..."
#define ESP32_API_TOKEN_VALUE "..."
```

`include/local_config.h` está ignorado por Git y nunca debe versionarse.

### Credenciales Databricks

La cuenta de servicio de Google usada por Sheets **no sirve** para autenticar al ESP32 contra Databricks. Para el panel se requiere un **Databricks service principal** con OAuth M2M:

1. crear el service principal en Databricks;
2. asignarlo al workspace;
3. generar un OAuth secret;
4. otorgarle **CAN USE** sobre la app `asistente-cloud-erp`;
5. copiar client ID y secret solamente a `include/local_config.h`.

El firmware solicita un token en `<workspace>/oidc/v1/token` con `grant_type=client_credentials`, lo conserva solo en RAM y lo renueva antes de su expiración. Nunca imprime el token ni el client secret.

`ESP32_API_TOKEN_VALUE` es una segunda protección, propia del backend 3C, enviada en `X-3C-Device-Token`.

## Contrato del dispositivo

| Operación | Endpoint |
|---|---|
| Health | `GET /api/device/v1/health` |
| Crear orden | `POST /api/device/v1/commands` |
| Estado | `GET /api/device/v1/commands/{command_id}` |

Secuencia: Wi-Fi → OAuth M2M Databricks → health → POST orden → `pending_confirmation` → polling cada 2.5 s → `applied` / `rejected`.

## Build y evidencia

Compilar con `pio run -e panel_4848s040`.

GitHub Actions compila y prueba el contrato contra `wpv10barza/asistente-de-databricks--erp`. La CI demuestra compilación e integración de software; **no demuestra validación física**. La pantalla, touch, Wi-Fi real y comunicación real del panel deben comprobarse con el hardware.

## Seguridad

- no versionar Wi-Fi, OAuth client secret, access tokens ni `ESP32_API_TOKEN`;
- el access token Databricks vive solo en RAM;
- `ALLOW_SHEET_WRITE` pertenece al backend Databricks, no al ESP32;
- el panel nunca recibe la credencial de Google Sheets;
- si un dispositivo se pierde, rotar el OAuth secret del service principal y el token 3C.
