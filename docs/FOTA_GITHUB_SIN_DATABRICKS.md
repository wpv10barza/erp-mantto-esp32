# ESP32-S3 FOTA directo desde GitHub — sin Databricks

Versión inicial del nuevo cliente: **2.7.5**. Repo público: https://github.com/wpv10barza/erp-mantto-esp32

## Arquitectura y consumo

```text
GitHub main → CI firmware + verificaciones → release ota-XXXXXXXX
                                           ├─ firmware.bin
                                           └─ manifest.json (SHA-256, tamaño, fuente, hardware)
ESP32-S3 4848S040 → Wi-Fi → HTTPS GitHub Releases → verifica manifiesto
                  → comprueba SHA-256 mientras descarga → ranura OTA → reinicia
```

Actualizar el firmware **no consulta a Databricks ni requiere encender su App**. Conserva las funciones 3C que siguen necesitando el backend Databricks cuando se utilicen. Las descargas públicas de GitHub pueden tener límites de uso, y GitHub Actions consume su propio cupo de CI, pero no consumen DBU de Databricks.

## Publicación y activación

Cada push a **main** inicia `Firmware CD`: compila el binario, ejecuta los contratos y, después de build exitoso, publica automáticamente el release `ota-XXXXXXXX`, usando los primeros **ocho caracteres del SHA exacto** del commit. Solo releases con un binario `firmware.bin` y su `manifest.json` se considerarán FOTA disponibles. El job `publish_github_fota` requiere permiso GitHub Actions `contents: write`. Si se bloquea por políticas del repositorio, ajustar **Settings → Actions → General → Workflow permissions** o usar un token/identidad de despliegue autorizada. No configurar tokens en el ESP32.

Consultar desde PowerShell:

```powershell
gh run list -R wpv10barza/erp-mantto-esp32 --workflow firmware-cd.yml --limit 5
gh release list -R wpv10barza/erp-mantto-esp32 --limit 10
gh release view ota-XXXXXXXX -R wpv10barza/erp-mantto-esp32
```

Reemplaza `XXXXXXXX` por el SHA del release de `main` ya publicado. **No usar un commit arbitrario que no tenga release.**

## Primer flash físico (USB)

1. Verificar que el hardware es efectivamente ESP32-S3-4848S040 con **16 MB de flash**, PSRAM y tabla `partitions_ota_16mb.csv`.
2. Si el dispositivo todavía tiene particiones antiguas, migrar por USB. El script flashea bootloader/particiones/aplicación; **no borres NVS** sin respaldo.
3. Desde Windows, después de reparar las URL locales privadas si hace falta:
   ```powershell
   cd C:\Users\ASUS\erp-mantto-esp32-deploy
   git pull --ff-only origin main
   .\scripts\Repair-Alumno-Cloud-Local.ps1 -Apply
   py .\scripts\verify_firmware_target.py
   .\scripts\Flash-ESP32-3C.ps1 -ListPorts
   .\scripts\Flash-ESP32-3C.ps1 -Port COMX -SerialSeconds 20
   ```
   Sustituye COMX por el puerto físico detectado. No ejecutes flash desde un working tree modificado. Si falta `include/local_config.h`, el panel debe tener NVS previamente aprovisionada; evita grabar un build sin credenciales en un dispositivo nuevo.
4. En pantalla: `Actualizar firmware → EDITAR SHA`, escribe los **8 primeros caracteres** de un release publicado y confirma dos veces. Si el commit mostrado coincide con el instalado, no se necesita actualizar.

## Integridad

- Cliente HTTPS con CA raíz de confianza (sin `setInsecure`), verificación de hora UTC para TLS y redirecciones HTTPS de GitHub.
- `manifest.json` define SHA de 40 caracteres, versión, tamaño, SHA-256, repositorio, hardware, tabla OTA de 16 MB y modo de persistencia NVS; se rechazan datos incompatibles.
- Binario descargado al slot OTA inactivo con `Update` y SHA-256 incremental. Si falla, se aborta sin activar la ranura.
- Releases inmutables **por política de flujo**: la automatización no sobrescribe tags existentes. Para inmutabilidad garantizada, activar **Immutable releases** en Settings → General → Releases.
- La primera migración USB y las pruebas de Wi-Fi, TLS, reinicio y recuperación ante corte eléctrico deben validarse físicamente; CI no equivale a un flash real.

**FOTA y conexión académica son cosas distintas:** el backend Databricks puede estar STOPPED y FOTA seguir funcionando si la red Wi-Fi y GitHub están disponibles.
