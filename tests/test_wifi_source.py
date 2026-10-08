from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "platformio" / "src" / "panel_4848s040" / "main.cpp").read_text(encoding="utf-8")
APP = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")
PLATFORMIO = (ROOT / "platformio.ini").read_text(encoding="utf-8")


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing {label}: {needle}")


# Wi-Fi and private credentials must come from ignored local_config.h.
require(APP, '#include "local_config.h"', "local config include")
require(APP, "WIFI_SSID_VALUE", "Wi-Fi SSID macro")
require(APP, "WIFI_PASSWORD_VALUE", "Wi-Fi password macro")
require(APP, "ASSISTANT_BASE_URL_VALUE", "Databricks App URL macro")
require(APP, "DATABRICKS_CLIENT_ID_VALUE", "Databricks M2M client id")
require(APP, "DATABRICKS_CLIENT_SECRET_VALUE", "Databricks M2M client secret")

# Firmware must use the configured credentials in STA mode.
require(MAIN, "WiFi.persistent(false);", "non-persistent Wi-Fi configuration")
require(MAIN, "WiFi.setAutoReconnect(true);", "automatic reconnect")
require(MAIN, "WiFi.mode(WIFI_STA);", "station mode")
require(MAIN, "WiFi.begin(app_config::wifiSsid, app_config::wifiPassword);", "configured Wi-Fi credentials")
require(MAIN, "WiFi.reconnect();", "Wi-Fi reconnect path")
require(MAIN, 'updatePanel(PanelState::Busy, "Reconectando Wi-Fi");', "reconnect state")
require(MAIN, "WiFi.status() == WL_CONNECTED", "real Wi-Fi state check")

if "WiFi.disconnect();" in MAIN:
    raise AssertionError("reconnect path must use WiFi.reconnect(), not repeated disconnect/begin")

# Diagnostics expose connectivity but not credentials.
require(MAIN, "wifiStatusLabel", "Wi-Fi status diagnostic")
require(MAIN, "WiFi.gatewayIP().toString()", "gateway diagnostic")
require(MAIN, "WiFi.RSSI()", "RSSI diagnostic")
require(MAIN, "GET health ->", "backend health diagnostic")
require(MAIN, "Wi-Fi listo:", "Wi-Fi acquisition diagnostic")

# Preserve known-working Guition display initialization.
require(MAIN, "st7701_type9_init_operations", "ST7701 type9 init sequence")
require(MAIN, "kScreenWidth, kScreenHeight, rgbPanel, 1, true", "rotation/RGB display contract")
if "tl040wvs03_init_operations" in MAIN:
    raise AssertionError("obsolete TL040WVS03 init sequence is still referenced")

# Production backend is Databricks Apps; mDNS is development fallback only.
require(MAIN, "cloudEndpointConfigured()", "configured cloud endpoint")
require(MAIN, "databricksAppEndpoint()", "Databricks app endpoint detection")
require(MAIN, "ensureDatabricksAccessToken()", "Databricks OAuth M2M")
require(MAIN, 'if (!cloudEndpointConfigured()) startMdns();', "mDNS fallback gate")
require(MAIN, "MDNS.queryService", "mDNS fallback discovery")
if "192.168." in MAIN or "192.168." in APP:
    raise AssertionError("tracked firmware must not contain a fixed private backend IPv4")

require(PLATFORMIO, "[env:panel_4848s040]", "panel_4848s040 environment")

print("Wi-Fi/cloud source contract: PASS")
print("- credentials sourced from ignored local_config.h")
print("- STA mode + auto-reconnect + non-destructive reconnect present")
print("- diagnostic exposes status/gateway/RSSI without credentials")
print("- Databricks Apps is production backend with OAuth M2M")
print("- mDNS remains development fallback only")
print("- ST7701 type9 Guition init preserved")
