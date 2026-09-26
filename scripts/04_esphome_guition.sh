#!/usr/bin/env bash
set -Eeuo pipefail

# BLOCK 4 — ESPHome validate/compile.
REPO_DIR="${REPO_DIR:-${HOME}/project/ESP32-S3-4848S040}"
VENV_DIR="${REPO_DIR}/.venv"
MODE="${BUILD_MODE:-validate}"

cd "${REPO_DIR}"
source "${VENV_DIR}/bin/activate"

CONFIG="${REPO_DIR}/src/main.yaml"
TMP_DIR=""
trap '[[ -z "${TMP_DIR}" ]] || rm -rf "${TMP_DIR}"' EXIT

if [[ "${MODE}" == "real" ]]; then
  [[ -f "${REPO_DIR}/src/secrets.yaml" ]] || { echo "[ERROR] src/secrets.yaml missing."; exit 3; }
  WORK_CONFIG="${CONFIG}"
elif [[ "${MODE}" == "validate" ]]; then
  TMP_DIR="$(mktemp -d)"
  cp -a "${REPO_DIR}/src/." "${TMP_DIR}/"
  rm -f "${TMP_DIR}/secrets.yaml"
  python - <<'PY' > "${TMP_DIR}/secrets.yaml"
import base64
print('wifi_ssid: "CI_DUMMY_WIFI"')
print('wifi_password: "CI_DUMMY_PASSWORD"')
print('display_key: "' + base64.b64encode(b"0123456789abcdef0123456789abcdef").decode() + '"')
print('display_ota: "CI_DUMMY_OTA"')
PY
  WORK_CONFIG="${TMP_DIR}/main.yaml"
else
  echo "[ERROR] BUILD_MODE must be validate or real."
  exit 64
fi

esphome config "${WORK_CONFIG}"
esphome compile "${WORK_CONFIG}"

printf '\n[OK] BLOCK 4 — ESPHome ${MODE} passed.\n'
