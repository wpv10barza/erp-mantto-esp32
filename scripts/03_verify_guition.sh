#!/usr/bin/env bash
set -Eeuo pipefail

# BLOCK 3 — verify required files and hardware/software contracts.
REPO_DIR="${REPO_DIR:-${HOME}/project/ESP32-S3-4848S040}"
cd "${REPO_DIR}"

required=(
  README.md
  src/main.yaml
  src/secrets.yaml.example
  platformio.ini
  platformio/src/panel_4848s040/main.cpp
  src/main.cpp
  include/app_config.h
  include/command_buffer.h
  include/command_text_viewport.h
  include/virtual_keyboard.h
  include/editor_components.h
  include/panel_gpio.h
  include/touch_input.h
  include/home_menu.h
  backend/device_api.py
  contract/device-command-v1.json
  firmware/firmware-sync.json
  .github/workflows/ci.yml
)

for f in "${required[@]}"; do
  [[ -f "$f" ]] || { echo "[ERROR] missing: $f"; exit 1; }
done

grep -Fq 'github://alaltitov/esphome@1b487af0ef26ff8e7908d34e415d99cc13fc1f98' src/main.yaml
grep -Eq '^lvgl:' src/main.yaml
grep -Fq 'platform: gt911' src/main.yaml
grep -Fq 'platform: st7701s' src/main.yaml
grep -Fq 'width: 480' src/main.yaml
grep -Fq 'height: 480' src/main.yaml

FIRMWARE=platformio/src/panel_4848s040/main.cpp
grep -Fq 'kTouchAddress = 0x5D' "$FIRMWARE"
grep -Fq 'commandBuffer' "$FIRMWARE"
grep -Fq '/api/device/v1/commands' "$FIRMWARE"
grep -Fq '2500UL' include/app_config.h

# Physical Guition 86BOX display/touch parity contract.
grep -Fq 'st7701_type9_init_operations' "$FIRMWARE"
grep -Fq 'kScreenWidth, kScreenHeight, rgbPanel, 1, true' "$FIRMWARE"
grep -Fq 'touch_input::mapRaw(rawX, rawY)' "$FIRMWARE"
grep -Fq 'home_ui::hitTest(' "$FIRMWARE"
grep -Fq 'constexpr int touchSda = 19;' include/panel_gpio.h
grep -Fq 'constexpr int touchScl = 45;' include/panel_gpio.h
grep -Fq 'kRawXMax = 480' include/touch_input.h
grep -Fq 'kRawYMax = 480' include/touch_input.h

# Production Databricks Apps contract.
grep -Fq 'bool cloudEndpointConfigured()' "$FIRMWARE"
grep -Fq 'bool ensureDatabricksAccessToken()' "$FIRMWARE"
grep -Fq 'workspace + "/oidc/v1/token"' "$FIRMWARE"
grep -Fq 'grant_type=client_credentials&scope=' "$FIRMWARE"
grep -Fq 'Authorization' "$FIRMWARE"
grep -Fq 'X-3C-Device-Token' "$FIRMWARE"
grep -Fq 'DATABRICKS_WORKSPACE_URL_VALUE' include/app_config.h
grep -Fq 'DATABRICKS_CLIENT_ID_VALUE' include/app_config.h
grep -Fq 'DATABRICKS_CLIENT_SECRET_VALUE' include/app_config.h
grep -Fq '"transport": "HTTPS to Databricks Apps"' contract/device-command-v1.json
grep -Fq '"mode": "service-principal M2M"' contract/device-command-v1.json

# Local backend discovery is forbidden in the production firmware.
! grep -Fq 'ESPmDNS' "$FIRMWARE"
! grep -Fq 'MDNS.queryService' "$FIRMWARE"
! grep -Fq '3c-backend.local' "$FIRMWARE"
! grep -Fq 'backendEndpoint' "$FIRMWARE"
! grep -Fq 'WiFi.localIP()' "$FIRMWARE"
! grep -Fq 'WiFi.gatewayIP()' "$FIRMWARE"

# The documentation mirror may never drift from the compiled PlatformIO source.
cmp -s "$FIRMWARE" src/main.cpp || {
  echo '[ERROR] src/main.cpp differs from the compiled panel firmware.' >&2
  exit 1
}

! grep -Eq '192\.168\.[0-9]+\.[0-9]+' "$FIRMWARE" include/app_config.h
! grep -Fq 'github://alaltitov/esphome@dev' src/main.yaml
! grep -Fq 'lvgl/lvgl' platformio.ini

printf '\n[OK] BLOCK 3 — LVGL + GT911 + ST7701S + Databricks Apps cloud-only OAuth M2M contract valid.\n'
