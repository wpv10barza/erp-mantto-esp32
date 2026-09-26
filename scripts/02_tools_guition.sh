#!/usr/bin/env bash
set -Eeuo pipefail

# BLOCK 2 — install exact tools; safe to re-run after interruption.
REPO_DIR="${REPO_DIR:-${HOME}/project/ESP32-S3-4848S040}"
VENV_DIR="${REPO_DIR}/.venv"
ESPHOME_VERSION="${ESPHOME_VERSION:-2026.8.2}"
PLATFORMIO_VERSION="${PLATFORMIO_VERSION:-6.2.0}"

cd "${REPO_DIR}"
[[ -d .git ]] || { echo "[ERROR] Run BLOCK 1 first."; exit 1; }

PYTHON_BIN="${PYTHON_BIN:-python3}"
"${PYTHON_BIN}" -c 'import sys; assert (3,12) <= sys.version_info < (3,15), sys.version'

if [[ ! -f "${VENV_DIR}/bin/activate" ]]; then
  "${PYTHON_BIN}" -m venv "${VENV_DIR}"
fi

source "${VENV_DIR}/bin/activate"

have_esphome=0
if command -v esphome >/dev/null 2>&1 && esphome version | grep -Fq "${ESPHOME_VERSION}"; then
  have_esphome=1
fi
(( have_esphome )) || python -m pip install "esphome==${ESPHOME_VERSION}"

have_pio=0
if command -v pio >/dev/null 2>&1 && pio --version | grep -Fq "${PLATFORMIO_VERSION}"; then
  have_pio=1
fi
(( have_pio )) || python -m pip install "platformio==${PLATFORMIO_VERSION}"

esphome version | grep -Fq "${ESPHOME_VERSION}" || { echo "[ERROR] ESPHome install failed."; exit 2; }
pio --version | grep -Fq "${PLATFORMIO_VERSION}" || { echo "[ERROR] PlatformIO install failed."; exit 2; }

printf '\n[OK] BLOCK 2 — ESPHome %s | PlatformIO %s\n' "${ESPHOME_VERSION}" "${PLATFORMIO_VERSION}"
