from pathlib import Path
import json
import re

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "platformio" / "src" / "panel_4848s040" / "main.cpp").read_text(encoding="utf-8")
MIRROR = (ROOT / "src" / "main.cpp").read_text(encoding="utf-8")
CONFIG = (ROOT / "include" / "app_config.h").read_text(encoding="utf-8")
CONTRACT = json.loads((ROOT / "contract" / "device-command-v1.json").read_text(encoding="utf-8"))


def require(text: str, needle: str, label: str) -> None:
    if needle not in text:
        raise AssertionError(f"missing {label}: {needle}")


if MAIN != MIRROR:
    raise AssertionError("src/main.cpp and platformio panel firmware must be byte-identical")

for source_name, source in [("main.cpp", MAIN), ("app_config.h", CONFIG)]:
    private_ips = re.findall(r"192\.168\.\d+\.\d+", source)
    if private_ips:
        raise AssertionError(f"fixed private IP remains in {source_name}: {private_ips}")

if "assistantBaseUrl" in MAIN or "BACKEND_IP" in MAIN:
    raise AssertionError("firmware runtime must not use a fixed backend endpoint")

require(MAIN, "#include <ESPmDNS.h>", "mDNS include")
require(MAIN, "#include <Preferences.h>", "NVS Preferences include")
require(MAIN, 'constexpr char kBackendLogicalHost[] = "3c-backend.local";', "backend logical host")
require(MAIN, 'constexpr char kBackendMdnsService[] = "3c";', "mDNS service")
require(MAIN, 'constexpr char kBackendMdnsProtocol[] = "tcp";', "mDNS protocol")
require(MAIN, "MDNS.queryService(kBackendMdnsService, kBackendMdnsProtocol)", "service discovery")
require(MAIN, "MDNS.queryHost(kBackendLogicalHost)", "host resolution")
require(MAIN, "MDNS.port(index)", "dynamic service port")

require(MAIN, "backendPrefs.getString(kBackendAddressKey, \"\")", "NVS address load")
require(MAIN, "backendPrefs.getUShort(kBackendPortKey, 0)", "NVS port load")
require(MAIN, "backendPrefs.putString(kBackendAddressKey, endpointValue.address)", "NVS address save")
require(MAIN, "backendPrefs.putUShort(kBackendPortKey, endpointValue.port)", "NVS port save")

setup = MAIN[MAIN.index("void setup()") :]
if setup.index("loadBackendEndpointFromNvs();") >= setup.index("connectWifi();"):
    raise AssertionError("cached backend endpoint must load before Wi-Fi connection")

health_start = MAIN.index("bool checkBackendHealth()")
health_end = MAIN.index("int send3CCommand", health_start)
health = MAIN[health_start:health_end]
require(health, "if (!backendEndpoint.valid())", "cache-first health decision")
require(health, "if (checkBackendHealthOnce()) return true;", "cached endpoint health attempt")
require(health, "if (!discoverBackendEndpoint()) return false;", "rediscovery after failure")
require(health, "return checkBackendHealthOnce();", "health retry after rediscovery")

require(MAIN, 'endpoint("/api/device/v1/commands")', "command endpoint")
require(MAIN, 'endpoint("/api/device/v1/commands/" + lastCommandId)', "poll endpoint")

expected = CONTRACT["discovery"]
if expected["service_type"] != "_3c._tcp":
    raise AssertionError("contract mDNS service must be _3c._tcp")
if expected["logical_host"] != "3c-backend.local":
    raise AssertionError("contract logical host must be 3c-backend.local")
if expected["port_dynamic"] is not True:
    raise AssertionError("backend discovery port must be dynamic")

print("Backend discovery contract: PASS")
print("- firmware mirror is byte-identical")
print("- no fixed private backend IP remains")
print("- Wi-Fi -> _3c._tcp -> 3c-backend.local -> resolved IPv4 + dynamic port")
print("- NVS cache is loaded first and rediscovered on health failure")
