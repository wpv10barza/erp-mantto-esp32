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
  include/app_config.h
  include/command_buffer.h
  include/command_text_viewport.h
  include/virtual_keyboard.h
  backend/device_api.py
  contract/device-command-v1.json
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

grep -Fq 'kTouchAddress = 0x5D' platformio/src/panel_4848s040/main.cpp
grep -Fq 'commandBuffer' platformio/src/panel_4848s040/main.cpp
grep -Fq '/api/device/v1/commands' platformio/src/panel_4848s040/main.cpp
grep -Fq '2500UL' platformio/src/panel_4848s040/main.cpp
# Physical Guition 86BOX display/touch parity contract.
grep -Fq 'st7701_type9_init_operations' platformio/src/panel_4848s040/main.cpp
grep -Fq 'kScreenWidth, kScreenHeight, rgbPanel, 1, true' platformio/src/panel_4848s040/main.cpp
grep -Fq 'sample.x = rawX < kScreenWidth ? rawX' platformio/src/panel_4848s040/main.cpp
grep -Fq 'sample.y = rawY < kScreenHeight ? rawY' platformio/src/panel_4848s040/main.cpp

! grep -Fq 'github://alaltitov/esphome@dev' src/main.yaml
! grep -Fq 'lvgl/lvgl' platformio.ini

printf '\n[OK] BLOCK 3 — LVGL + GT911 + ST7701S + 3C contracts valid.\n'
