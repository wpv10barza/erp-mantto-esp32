from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "platformio" / "src" / "panel_4848s040" / "main.cpp").read_text(encoding="utf-8")
MIRROR = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
CONFIG = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")
EXAMPLE = (ROOT / "include" / "local_config.example.h").read_text(encoding="utf-8")
CONTRACT = json.loads((ROOT / "contract" / "device-command-v1.json").read_text(encoding="utf-8"))


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing {label}: {needle}")


if MAIN != MIRROR:
    raise AssertionError("src/main.cpp and platformio panel firmware must be byte-identical")

for source_name, source in [("main.cpp", MAIN), ("app_config.h", CONFIG)]:
    private_ips = re.findall(r"192\.168\.\d+\.\d+", source)
    if private_ips:
        raise AssertionError(f"fixed private backend IP remains in {source_name}: {private_ips}")

require(CONFIG, "ASSISTANT_BASE_URL_VALUE", "Databricks app URL setting")
require(CONFIG, "DATABRICKS_WORKSPACE_URL_VALUE", "Databricks workspace setting")
require(CONFIG, "DATABRICKS_CLIENT_ID_VALUE", "M2M client id setting")
require(CONFIG, "DATABRICKS_CLIENT_SECRET_VALUE", "M2M client secret setting")
require(EXAMPLE, "YOUR_DATABRICKS_SERVICE_PRINCIPAL_CLIENT_ID", "safe M2M placeholder")
require(EXAMPLE, "YOUR_DATABRICKS_SERVICE_PRINCIPAL_OAUTH_SECRET", "safe OAuth secret placeholder")

require(MAIN, "bool cloudEndpointConfigured()", "cloud endpoint switch")
require(MAIN, "bool databricksAppEndpoint()", "Databricks endpoint detection")
require(MAIN, "bool ensureDatabricksAccessToken()", "M2M token refresh")
require(MAIN, 'workspace + "/oidc/v1/token"', "workspace OAuth token endpoint")
require(MAIN, '"grant_type=client_credentials&scope="', "OAuth client credentials grant")
require(MAIN, 'http.addHeader("Authorization", String("Bearer ") + databricksAccessToken)', "Bearer header")
require(MAIN, 'http.addHeader("X-3C-Device-Token", app_config::apiToken)', "device token header")
require(MAIN, 'endpoint("/api/device/v1/commands")', "command endpoint")
require(MAIN, 'endpoint("/api/device/v1/commands/" + lastCommandId)', "poll endpoint")
require(MAIN, '"DATABRICKS LISTO"', "cloud-ready UI state")
require(MAIN, '"PROBAR CLOUD"', "cloud test button")

require(MAIN, "#include <ESPmDNS.h>", "mDNS fallback include")
require(MAIN, "if (!cloudEndpointConfigured()) startMdns();", "mDNS fallback gate")
require(MAIN, "if (!cloudEndpointConfigured() && !backendEndpoint.valid())", "discovery fallback gate")

if CONTRACT["transport"] != "HTTPS to Databricks Apps":
    raise AssertionError("production transport must be Databricks HTTPS")
if CONTRACT["platform_authentication"]["mode"] != "service-principal M2M":
    raise AssertionError("production auth must use Databricks service-principal M2M")
if CONTRACT["direct_sheet_write"] is not False:
    raise AssertionError("ESP32 must never write Google Sheets directly")
if CONTRACT["requires_human_confirmation"] is not True:
    raise AssertionError("human confirmation must remain mandatory")

print("Databricks firmware transport contract: PASS")
print("- firmware mirror is byte-identical")
print("- production endpoint is configured HTTPS Databricks Apps")
print("- OAuth M2M token acquisition + refresh is implemented")
print("- Authorization Bearer + X-3C-Device-Token are both applied")
print("- mDNS remains local fallback only")
print("- direct Google Sheets writes remain forbidden")
