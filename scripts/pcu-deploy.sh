#!/usr/bin/env bash
set -Eeuo pipefail

REPO_URL="https://github.com/wpv10barza/erp-mantto-esp32.git"
BACKEND_URL="https://github.com/wpv10barza/asistente-3c.git"
BRANCH="main"
STATE_ROOT="${XDG_STATE_HOME:-$HOME/.local/state}/erp-mantto-esp32-pcu"
CONFIG_ROOT="${XDG_CONFIG_HOME:-$HOME/.config}/erp-mantto-esp32-pcu"
ERP_REPO="$STATE_ROOT/erp-mantto-esp32"
BACKEND_REPO="$STATE_ROOT/asistente-3c"
VENV="$STATE_ROOT/venv"
EXPECTED_MAIN=""
NO_FLASH=0
START_BACKEND=0
PLATFORMIO_VERSION="6.2.0"
PORT_DEVICE="${PCU_PORT:-}"

usage() {
  cat <<'EOF'
Uso: pcu-deploy.sh [--expected-main SHA] [--no-flash] [--start-backend]

Sincroniza siempre wpv10barza/erp-mantto-esp32 main en un clon dedicado de WSL,
verifica el manifiesto del firmware, sincroniza Asistente 3C, compila y flashea
el ESP32-S3-4848S040. --start-backend inicia Asistente 3C despues de validarlo.
EOF
}

while (($#)); do
  case "$1" in
    --expected-main) EXPECTED_MAIN="${2:?falta SHA}"; shift 2 ;;
    --no-flash) NO_FLASH=1; shift ;;
    --start-backend) START_BACKEND=1; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Argumento desconocido: $1" >&2; usage >&2; exit 64 ;;
  esac
done

mkdir -p "$STATE_ROOT" "$CONFIG_ROOT"
chmod 700 "$STATE_ROOT" "$CONFIG_ROOT" 2>/dev/null || true

echo
echo "============================================================"
echo " PCU ERP-MANTTO-ESP32 - WSL/Ubuntu"
echo "============================================================"

need() {
  command -v "$1" >/dev/null 2>&1 || { echo "[ERROR] falta $1. $2" >&2; exit 127; }
}
need git "Instale git dentro de Ubuntu."
need python3 "Instale python3 dentro de Ubuntu."

sync_repo() {
  local url="$1" dir="$2" branch="$3"
  if [[ ! -d "$dir/.git" ]]; then
    rm -rf "$dir"
    git clone --branch "$branch" --single-branch "$url" "$dir"
  fi
  git -C "$dir" remote set-url origin "$url"
  git -C "$dir" fetch origin "$branch" --prune
  git -C "$dir" checkout -B "$branch" "origin/$branch"
  git -C "$dir" reset --hard "origin/$branch"
}

echo
echo "[1] SINCRONIZAR ERP main"
sync_repo "$REPO_URL" "$ERP_REPO" "$BRANCH"
ERP_SHA="$(git -C "$ERP_REPO" rev-parse HEAD)"
REMOTE_SHA="$(git -C "$ERP_REPO" rev-parse "origin/$BRANCH")"
[[ "$ERP_SHA" == "$REMOTE_SHA" ]] || { echo "[ERROR] local != origin/main" >&2; exit 11; }
if [[ -n "$EXPECTED_MAIN" && "$ERP_SHA" != "$EXPECTED_MAIN" ]]; then
  echo "[ERROR] main cambio entre Windows y WSL: esperado=$EXPECTED_MAIN actual=$ERP_SHA" >&2
  exit 12
fi
echo "ERP main: $ERP_SHA"

echo
echo "[2] VERIFICAR FIRMWARE Y DNS"
cd "$ERP_REPO"
python3 tests/test_firmware_main_sync.py
python3 tests/test_backend_discovery.py

echo
echo "[3] SINCRONIZAR ASISTENTE 3C"
sync_repo "$BACKEND_URL" "$BACKEND_REPO" "$BRANCH"
BACKEND_SHA="$(git -C "$BACKEND_REPO" rev-parse HEAD)"
MANIFEST_BACKEND_SHA="$(python3 - <<'PY'
import json
from pathlib import Path
m=json.loads(Path('firmware/firmware-sync.json').read_text())
print(m['backend']['validated_commit'])
PY
)"
if [[ "$BACKEND_SHA" != "$MANIFEST_BACKEND_SHA" ]]; then
  echo "[ERROR] Asistente 3C main no coincide con el commit validado por ERP." >&2
  echo "        ERP espera: $MANIFEST_BACKEND_SHA" >&2
  echo "        Backend actual: $BACKEND_SHA" >&2
  echo "        Actualice primero el manifiesto/CI del ERP antes de desplegar." >&2
  exit 13
fi
BACKEND_DISCOVERY="$BACKEND_REPO/server/backendDiscovery.ts"
grep -q 'hostname: "3c-backend.local"' "$BACKEND_DISCOVERY"
grep -q 'service: "3c"' "$BACKEND_DISCOVERY"
grep -q 'protocol: "tcp"' "$BACKEND_DISCOVERY"
grep -q 'advertise(backendDiscoveryOptions' "$BACKEND_DISCOVERY"
echo "Asistente 3C: $BACKEND_SHA"
echo "DNS: _3c._tcp -> 3c-backend.local (puerto dinamico)"

LOCAL_CONFIG="$CONFIG_ROOT/local_config.h"
if [[ ! -s "$LOCAL_CONFIG" ]]; then
  if [[ ! -t 0 ]]; then
    echo "[ERROR] falta $LOCAL_CONFIG y no hay terminal interactiva." >&2
    exit 20
  fi
  echo
echo "[4] CONFIGURACION LOCAL DEL PANEL"
  read -r -p "SSID Wi-Fi 2.4 GHz: " WIFI_SSID
  read -r -s -p "Clave Wi-Fi: " WIFI_PASSWORD; echo
  read -r -s -p "Token ESP32/3C (Enter si no usa token): " ESP_TOKEN; echo
  cat >"$LOCAL_CONFIG" <<EOF
#pragma once
#define WIFI_SSID_VALUE "$WIFI_SSID"
#define WIFI_PASSWORD_VALUE "$WIFI_PASSWORD"
#define ESP32_API_TOKEN_VALUE "$ESP_TOKEN"
#define PANEL_AUDIO_ENABLED_VALUE 1
#define PANEL_BRIGHTNESS_VALUE 180
EOF
  chmod 600 "$LOCAL_CONFIG"
fi
cp "$LOCAL_CONFIG" "$ERP_REPO/include/local_config.h"
chmod 600 "$ERP_REPO/include/local_config.h"

echo
echo "[5] PLATFORMIO"
if [[ ! -x "$VENV/bin/python" ]]; then
  python3 -m venv "$VENV" || {
    echo "[ERROR] no se pudo crear venv. Instale python3-venv." >&2
    exit 21
  }
fi
"$VENV/bin/python" -m pip -q install --upgrade pip
"$VENV/bin/python" -m pip -q install --upgrade "platformio==$PLATFORMIO_VERSION"
PIO="$VENV/bin/pio"
"$PIO" --version

detect_port() {
  local ports=()
  mapfile -t ports < <(find /dev -maxdepth 1 -type c \( -name 'ttyACM*' -o -name 'ttyUSB*' \) -print 2>/dev/null | sort)
  if [[ -n "$PORT_DEVICE" ]]; then
    [[ -e "$PORT_DEVICE" ]] || { echo "[ERROR] no existe PCU_PORT=$PORT_DEVICE" >&2; return 1; }
    printf '%s\n' "$PORT_DEVICE"
    return 0
  fi
  if ((${#ports[@]} == 1)); then
    printf '%s\n' "${ports[0]}"
  elif ((${#ports[@]} == 0)); then
    echo "[ERROR] WSL no ve /dev/ttyACM* ni /dev/ttyUSB*. Revise usbipd attach." >&2
    return 1
  else
    echo "[ERROR] hay varios puertos serie en WSL: ${ports[*]}. Defina PCU_PORT." >&2
    return 1
  fi
}

if (( NO_FLASH )); then
  echo
echo "[6] COMPILACION (sin flasheo solicitado)"
  "$PIO" run -e panel_4848s040
else
  echo
echo "[6] DETECTAR PUERTO Y COMPILAR"
  PORT_DEVICE="$(detect_port)"
  echo "Puerto WSL: $PORT_DEVICE"
  if [[ ! -r "$PORT_DEVICE" || ! -w "$PORT_DEVICE" ]]; then
    echo "Ajustando permisos temporales para $PORT_DEVICE"
    sudo chmod a+rw "$PORT_DEVICE"
  fi
  "$PIO" run -e panel_4848s040

  echo
echo "[7] FLASHEAR EL MISMO main VERIFICADO"
  "$PIO" run -e panel_4848s040 -t upload --upload-port "$PORT_DEVICE"
  echo "Flasheo completado: $ERP_SHA"

  echo
echo "[8] BUSCAR IP DEL PANEL"
  PANEL_IP=""
  set +e
  PANEL_IP="$($VENV/bin/python - "$PORT_DEVICE" <<'PY'
import re, sys, time
try:
    import serial
except Exception:
    raise SystemExit(0)
port=sys.argv[1]
deadline=time.time()+35
try:
    s=serial.Serial(port,115200,timeout=1)
except Exception:
    raise SystemExit(0)
with s:
    while time.time()<deadline:
        line=s.readline().decode('utf-8','ignore').strip()
        if line:
            print(line, file=sys.stderr)
        m=re.search(r'Wi-Fi listo: http://([0-9.]+)/', line)
        if m:
            print(m.group(1))
            raise SystemExit(0)
PY
  )"
  set -e
  if [[ -n "$PANEL_IP" ]]; then
    echo "IP ESP32: $PANEL_IP"
    command -v curl >/dev/null 2>&1 && curl --max-time 4 -fsS "http://$PANEL_IP/health" || true
    echo
  else
    echo "No se capturo la IP por serie. Pruebe: http://esp32-panel-3c.local/"
  fi
fi

if (( START_BACKEND )); then
  echo
echo "[9] INICIAR ASISTENTE 3C CON mDNS"
  need node "Instale Node.js 20+ dentro de WSL."
  need npm "Instale npm dentro de WSL."
  BACKEND_ENV="$CONFIG_ROOT/backend.env"
  if [[ ! -s "$BACKEND_ENV" ]]; then
    [[ -t 0 ]] || { echo "[ERROR] falta $BACKEND_ENV" >&2; exit 30; }
    ESP_TOKEN="$(sed -n 's/^#define ESP32_API_TOKEN_VALUE "\(.*\)"/\1/p' "$LOCAL_CONFIG" | head -n1)"
    read -r -s -p "GEMINI_API_KEY para Asistente 3C: " GEMINI_KEY; echo
    cat >"$BACKEND_ENV" <<EOF
GEMINI_API_KEY=$GEMINI_KEY
ESP32_API_TOKEN=$ESP_TOKEN
PORT=3000
ALLOW_INSECURE_DEVICE_API=false
MDNS_ENABLED=true
MDNS_SERVICE=3c
MDNS_HOST=3c-backend.local
MDNS_NAME=3C Backend
EOF
    chmod 600 "$BACKEND_ENV"
  fi
  cd "$BACKEND_REPO"
  npm ci
  set -a; source "$BACKEND_ENV"; set +a
  if [[ -f "$STATE_ROOT/backend.pid" ]] && kill -0 "$(cat "$STATE_ROOT/backend.pid")" 2>/dev/null; then
    kill "$(cat "$STATE_ROOT/backend.pid")" || true
    sleep 1
  fi
  nohup npm run dev >"$STATE_ROOT/backend.log" 2>&1 &
  echo $! >"$STATE_ROOT/backend.pid"
  sleep 3
  if command -v curl >/dev/null 2>&1; then
    curl --max-time 5 -fsS "http://127.0.0.1:3000/api/device/v1/health"
    echo
  fi
  echo "Backend iniciado. Log: $STATE_ROOT/backend.log"
fi

echo
echo "============================================================"
echo " PCU OK"
echo " ERP main:       $ERP_SHA"
echo " Asistente 3C:   $BACKEND_SHA"
echo " DNS backend:    3c-backend.local / _3c._tcp"
echo "============================================================"
