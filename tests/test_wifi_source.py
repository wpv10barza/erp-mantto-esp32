from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "platformio" / "src" / "panel_4848s040" / "main.cpp").read_text(encoding="utf-8")
APP = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")
PLATFORMIO = (ROOT / "platformio.ini").read_text(encoding="utf-8")


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing {label}: {needle}")


require(APP, '#include "local_config.h"', "local config include")
require(APP, "WIFI_SSID_VALUE", "Internet Wi-Fi SSID macro")
require(APP, "WIFI_PASSWORD_VALUE", "Internet Wi-Fi password macro")
require(APP, "ASSISTANT_BASE_URL_VALUE", "Databricks App URL macro")

# ESP32-S3 hardware still uses Wi-Fi as its Internet transport. It does not need
# the same Wi-Fi/LAN as a PC and never discovers a local backend.
require(MAIN, "WiFi.persistent(false);", "non-persistent Wi-Fi configuration")
require(MAIN, "WiFi.setAutoReconnect(true);", "automatic reconnect")
require(MAIN, "WiFi.mode(WIFI_STA);", "station mode")
require(MAIN, "WiFi.begin(app_config::wifiSsid, app_config::wifiPassword);", "configured Internet transport")
require(MAIN, "WiFi.reconnect();", "network reconnect path")
require(MAIN, '"Reconectando Internet"', "Internet reconnect state")
require(MAIN, '"CLOUD HTTPS"', "cloud status display")

# Do not expose or depend on local network addressing.
for forbidden in [
    "WiFi.localIP()", "WiFi.gatewayIP()", "192.168.", "3c-backend.local",
    "MDNS.queryService", "MDNS.queryHost", "PROBAR WSL", "WSL DISPONIBLE",
]:
    if forbidden in MAIN or forbidden in APP:
        raise AssertionError(f"local-network dependency remains: {forbidden}")

require(MAIN, "st7701_type9_init_operations", "ST7701 type9 init sequence")
require(MAIN, "kScreenWidth, kScreenHeight, rgbPanel, 1, true", "rotation/RGB display contract")
require(MAIN, "ensureDatabricksAccessToken()", "Databricks OAuth M2M")
require(PLATFORMIO, "[env:panel_4848s040]", "panel_4848s040 environment")

print("Network/cloud source contract: PASS")
print("- Wi-Fi is only the ESP32 Internet transport")
print("- no same-LAN, local IP, mDNS or WSL backend dependency")
print("- Databricks Apps is the production endpoint")
print("- ST7701 type9 Guition init preserved")
