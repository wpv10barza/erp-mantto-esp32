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

native_test_log="${REPO_DIR}/native-test.log"

pio test -e native 2>&1 | tee "${native_test_log}"

if grep -q '\[SKIPPED\]' "${native_test_log}"; then
  echo "[ERROR] PlatformIO reported skipped native tests."
  grep '\[SKIPPED\]' "${native_test_log}"
  exit 1
fi

required_suites=(
  "native:test_backend_command_buffer"
  "native:test_command_text_viewport"
  "native:test_virtual_keyboard"
)

for suite in "${required_suites[@]}"; do
  if ! grep -Eq -- "native:${suite#native:}[[:space:]]+\[?PASSED\]?" "${native_test_log}"; then
    echo "[ERROR] Required native test suite did not report PASSED: ${suite}"
    grep -E -- "native:${suite#native:}" "${native_test_log}" || true
    exit 1
  fi
done

pio run -e panel_4848s040

[[ -f .pio/build/panel_4848s040/firmware.bin ]] || {
  echo "[ERROR] firmware.bin not generated."; exit 2;
}

printf '\n[OK] BLOCK 5 — tests + ESP32-S3 build passed.\n'
