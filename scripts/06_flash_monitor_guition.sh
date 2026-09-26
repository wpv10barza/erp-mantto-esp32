#!/usr/bin/env bash
set -Eeuo pipefail

# BLOCK 6 — flash and monitor physical ESP32-S3.
REPO_DIR="${REPO_DIR:-${HOME}/project/ESP32-S3-4848S040}"
VENV_DIR="${REPO_DIR}/.venv"
PIO_ENV="${PIO_ENV:-panel_4848s040}"
PORT="${PORT:-}"
BAUD="${BAUD:-115200}"
MONITOR="${MONITOR:-1}"

cd "${REPO_DIR}"
source "${VENV_DIR}/bin/activate"

echo "============================================================"
echo " ESP32-S3-4848S040 | FLASH + MONITOR"
echo "============================================================"

# Never allow a legacy PC serial node such as /dev/ttyS0 to be
# selected automatically. ESP32 USB serial on WSL should normally
# appear as /dev/ttyACM* or /dev/ttyUSB*.
if [[ "${PORT}" == /dev/ttyS* ]]; then
  echo "[ERROR] PORT=${PORT} is a legacy PC serial device."
  echo "[ERROR] Do not use /dev/ttyS0 for the ESP32."
  echo "[INFO] Expected WSL USB devices: /dev/ttyACM0 or /dev/ttyUSB0."
  echo
  echo "[INFO] Check:"
  echo "  ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true"
  echo "  pio device list || true"
  echo "  lsusb || true"
  echo
  echo "[INFO] From Windows PowerShell (Administrator):"
  echo "  usbipd list"
  exit 20
fi

if [[ -z "${PORT}" ]]; then
  PORT="$(find /dev -maxdepth 1 -type c \( -name 'ttyACM*' -o -name 'ttyUSB*' \) -print 2>/dev/null | sort | head -n 1 || true)"
fi

if [[ -z "${PORT}" || ! -e "${PORT}" ]]; then
  echo "[ERROR] No ESP32 USB serial device was found in WSL."
  echo
  echo "[INFO] WSL devices:"
  ls -l /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || true
  echo
  echo "[INFO] PlatformIO devices:"
  pio device list || true
  echo
  echo "[INFO] USB devices:"
  lsusb || true
  echo
  echo "[INFO] Windows PowerShell (Administrator):"
  echo "  usbipd list"
  echo
  echo "[INFO] Expected result: /dev/ttyACM0 or /dev/ttyUSB0"
  exit 20
fi

echo "[OK] ESP32 serial port: ${PORT}"

if [[ ! -f ".pio/build/${PIO_ENV}/firmware.bin" ]]; then
  echo "[ERROR] Firmware not found."
  echo "[INFO] Run BLOCK 5 first:"
  echo "  bash scripts/05_platformio_guition.sh"
  exit 1
fi

echo
echo "=== FIRMWARE ==="
ls -lh ".pio/build/${PIO_ENV}/firmware.bin"

echo
echo "=== UPLOAD ==="
pio run -e "${PIO_ENV}" -t upload --upload-port "${PORT}"

printf '\n[OK] BLOCK 6 — flashed ${PIO_ENV} on ${PORT}.\n'

if [[ "${MONITOR}" == "1" ]]; then
  echo
  echo "=== SERIAL MONITOR ${BAUD} ==="
  pio device monitor --port "${PORT}" --baud "${BAUD}"
fi
