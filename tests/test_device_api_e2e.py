#!/usr/bin/env python3
"""E2E device contract against the Databricks-only FastAPI app running locally.

The Databricks edge OAuth layer cannot be reproduced on a GitHub runner, so this
test covers the application-level Device API behind that edge:
health -> X-3C-Device-Token -> idempotent enqueue -> status polling -> human
review rejection -> terminal polling.
"""
import json
import os
import time
import urllib.error
import urllib.request

TOKEN = os.environ.get("ESP32_API_TOKEN", "ci-e2e-token")
BASE_URL = os.environ.get("DEVICE_API_BASE_URL", "http://127.0.0.1:3000").rstrip("/")


def request(method: str, path: str, payload=None, token: str | None = TOKEN):
    data = json.dumps(payload).encode() if payload is not None else None
    headers = {"Content-Type": "application/json"}
    if token is not None:
        headers["X-3C-Device-Token"] = token
    req = urllib.request.Request(BASE_URL + path, data=data, method=method, headers=headers)
    try:
        with urllib.request.urlopen(req, timeout=5) as response:
            raw = response.read()
            return response.status, json.loads(raw) if raw else {}
    except urllib.error.HTTPError as exc:
        raw = exc.read()
        return exc.code, json.loads(raw) if raw else {}


def wait_for_health():
    last_error = None
    for _ in range(30):
        try:
            code, body = request("GET", "/api/device/v1/health", token=None)
            if code == 200:
                return body
            last_error = f"HTTP {code}: {body}"
        except urllib.error.URLError as exc:
            last_error = str(exc)
        time.sleep(1)
    raise AssertionError(f"Databricks app did not become healthy locally: {last_error}")


health = wait_for_health()
assert health["accepts_commands"] is True, health
assert health["requires_human_confirmation"] is True, health
assert health["protocol_version"] == "1.0", health
assert health["supports_status_polling"] is True, health

device_id = "panel-4848s040-3c-ci"
request_id = "panel-4848s040-3c-ci-001"
command = "Cambia la tarea J10 a mensual"

code, unauthorized = request(
    "POST", "/api/device/v1/commands",
    {"device_id": device_id, "request_id": request_id, "text": command},
    token="wrong-token",
)
assert code == 401, unauthorized

code, queued = request(
    "POST", "/api/device/v1/commands",
    {"device_id": device_id, "request_id": request_id, "text": command},
)
assert code in (200, 202), queued
assert queued["status"] == "pending_confirmation", queued
assert queued["requires_human_confirmation"] is True, queued
command_id = queued["command_id"]

code, duplicate = request(
    "POST", "/api/device/v1/commands",
    {"device_id": device_id, "request_id": request_id, "text": command},
)
assert code == 200, duplicate
assert duplicate["duplicate"] is True, duplicate
assert duplicate["command_id"] == command_id, duplicate

code, pending = request("GET", f"/api/device/v1/commands/{command_id}")
assert code == 200, pending
assert pending["command"]["status"] == "pending_confirmation", pending

proposal_payload = {
    "row": 5,
    "matched": "CI",
    "external_command_id": command_id,
    "operations": [{
        "campo": "frecuencia",
        "columna_actualizar": "L",
        "encabezado": "Frecuencia",
        "valor_actualizar": 1,
        "razon": "CI: rechazo humano sin escritura en Sheets",
    }],
}
code, proposal = request("POST", "/api/review/proposals", proposal_payload, token=None)
assert code == 201, proposal
proposal_id = proposal["id"]

code, rejected = request(
    "POST", f"/api/review/proposals/{proposal_id}/reject", token=None
)
assert code == 200, rejected
assert rejected["status"] == "rejected", rejected

code, terminal = request("GET", f"/api/device/v1/commands/{command_id}")
assert code == 200, terminal
assert terminal["command"]["status"] == "rejected", terminal

print("DATABRICKS-ONLY DEVICE API E2E: PASS")
print(f"- base: {BASE_URL}")
print("- health contract")
print("- X-3C-Device-Token")
print("- command enqueue + request_id idempotence")
print("- authenticated status polling")
print("- human review rejection")
print("- terminal rejected polling")
