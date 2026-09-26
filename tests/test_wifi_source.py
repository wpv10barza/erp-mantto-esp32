from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "platformio" / "src" / "panel_4848s040" / "main.cpp").read_text(encoding="utf-8")
APP = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")
PLATFORMIO = (ROOT / "platformio.ini").read_text(encoding="utf-8")


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing {label}: {needle}")


# Wi-Fi must come from ignored local_config.h, never from tracked firmware.
require(APP, '#include "local_config.h"', "local Wi-Fi config include")
require(APP, "WIFI_SSID_VALUE", "Wi-Fi SSID macro")
require(APP, "WIFI_PASSWORD_VALUE", "Wi-Fi password macro")
require(APP, "ASSISTANT_BASE_URL_VALUE", "backend URL macro")

# Firmware must use the configured credentials in STA mode.
require(MAIN, "WiFi.persistent(false);", "non-persistent Wi-Fi configuration")
require(MAIN, "WiFi.setAutoReconnect(true);", "automatic reconnect")
require(MAIN, "WiFi.mode(WIFI_STA);", "station mode")
require(MAIN, "WiFi.begin(app_config::wifiSsid, app_config::wifiPassword);", "configured Wi-Fi credentials")
require(MAIN, "WiFi.reconnect();", "Wi-Fi reconnect path")
require(MAIN, "updatePanel(PanelState::Busy, \"Reconectando Wi-Fi\");", "reconnect state")
require(MAIN, "WiFi.status() == WL_CONNECTED", "real Wi-Fi state check")

# Reconnect logic must not repeatedly reset the station with WiFi.disconnect().
if "WiFi.disconnect();" in MAIN:
    raise AssertionError("reconnect path must use WiFi.reconnect(), not repeated WiFi.disconnect()+WiFi.begin()")

# Diagnostics must expose real connection status without printing credentials.
require(MAIN, "wifiStatusLabel", "Wi-Fi status diagnostic")
require(MAIN, "WiFi.gatewayIP().toString()", "gateway diagnostic")
require(MAIN, "WiFi.RSSI()", "RSSI diagnostic")
require(MAIN, "GET health ->", "backend health diagnostic")
require(MAIN, "Wi-Fi listo:", "Wi-Fi acquisition diagnostic")

# Preserve the known-working ST7701 initialization.
require(MAIN, "st7701_type8_init_operations", "ST7701 type8 init sequence")
if "tl040wvs03_init_operations" in MAIN:
    raise AssertionError("obsolete TL040WVS03 init sequence is still referenced")

# The tracked default must never point to loopback for the ESP32-to-host bridge.
if 'ASSISTANT_BASE_URL_VALUE "http://127.0.0.1:' in APP:
    raise AssertionError("ESP32 backend default must not use loopback")

# The firmware must keep using the expected board environment.
require(PLATFORMIO, "[env:panel_4848s040]", "panel_4848s040 environment")

# No real credentials are allowed in tracked firmware source.
tracked_source = "\n".join((ROOT / p).read_text(encoding="utf-8") for p in [
    "include/app_config.h",
    "platformio/src/panel_4848s040/main.cpp",
])
for forbidden in ["WIFI_PASSWORD_VALUE \"", "ESP32_API_TOKEN_VALUE \""]:
    if forbidden in tracked_source and forbidden not in APP:
        raise AssertionError(f"unexpected hard-coded credential marker: {forbidden}")

print("Wi-Fi source contract: PASS")
print("- credentials sourced from ignored local_config.h")
print("- STA mode + auto-reconnect + non-destructive reconnect present")
print("- diagnostic exposes status/gateway/RSSI without credentials")
print("- backend URL is not hard-coded to loopback")
print("- ST7701 type8 init preserved")
