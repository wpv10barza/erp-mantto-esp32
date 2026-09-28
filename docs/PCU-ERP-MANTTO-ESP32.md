# PCU ERP-MANTTO-ESP32

Fuente autoritativa de firmware: `wpv10barza/erp-mantto-esp32`, rama `main`.

## Uso recomendado en Windows

Abra PowerShell y ejecute:

```powershell
Set-ExecutionPolicy -Scope Process Bypass
.\PCU-ERP-MANTTO-ESP32.ps1
```

O use `PCU-ERP-MANTTO-ESP32.cmd` para conservar la ventana visible si ocurre un error.

La PCU:

1. clona o resincroniza un clon de despliegue dedicado contra `origin/main`;
2. verifica que HEAD local sea exactamente el SHA remoto de `main`;
3. verifica los blobs declarados en `firmware/firmware-sync.json`;
4. detecta/recuerda el BUSID del ESP32 y lo adjunta a WSL con `usbipd`;
5. dentro de WSL vuelve a sincronizar `erp-mantto-esp32/main` y `asistente-3c/main`;
6. valida `_3c._tcp` y `3c-backend.local`;
7. compila el entorno `panel_4848s040` y flashea por `/dev/ttyACM*` o `/dev/ttyUSB*`;
8. intenta recuperar la IP del panel por el puerto serie.

Las credenciales Wi-Fi y el token se guardan solo en `~/.config/erp-mantto-esp32-pcu/local_config.h` dentro de WSL. No se escriben en Git.

Para sincronizar/compilar sin flashear:

```powershell
.\PCU-ERP-MANTTO-ESP32.ps1 -NoFlash
```

Para sincronizar, flashear e iniciar también Asistente 3C:

```powershell
.\PCU-ERP-MANTTO-ESP32.ps1 -StartBackend
```

Si la detección automática del USB encuentra varios dispositivos:

```powershell
.\PCU-ERP-MANTTO-ESP32.ps1 -BusId 1-1
```

El script ya no se relanza completo como administrador. Solo eleva el comando `usbipd bind` cuando Windows lo exige, por lo que la ventana principal y el log permanecen visibles.
