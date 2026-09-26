#!/usr/bin/env bash
set -Eeuo pipefail

# BLOCK 5 — native tests + ESP32-S3 firmware build.
REPO_DIR="${REPO_DIR:-${HOME}/project/ESP32-S3-4848S040}"
VENV_DIR="${REPO_DIR}/.venv"

cd "${REPO_DIR}"
source "${VENV_DIR}/bin/activate"

python tests/test_panel_state_contract.py
python tests/test_wifi_source.py
python tests/test_command_editor_integration.py
python test/command_buffer_regression.py

pio test -e native
pio run -e panel_4848s040

[[ -f .pio/build/panel_4848s040/firmware.bin ]] || {
  echo "[ERROR] firmware.bin not generated."; exit 2;
}

printf '\n[OK] BLOCK 5 — tests + ESP32-S3 build passed.\n'
