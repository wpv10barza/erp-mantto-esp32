from __future__ import annotations

import json
import threading
import uuid
from dataclasses import dataclass, asdict
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Dict


@dataclass
class Command:
    command_id: str
    device_id: str
    command: str
    state: str = "pending_confirmation"
    detail: str = "Awaiting human confirmation"
    audit: list[str] | None = None

    def __post_init__(self) -> None:
        if self.audit is None:
            self.audit = ["created:pending_confirmation"]


class DeviceState:
    def __init__(self) -> None:
        self.lock = threading.Lock()
        self.commands: Dict[str, Command] = {}

    def create(self, device_id: str, command: str) -> Command:
        item = Command(uuid.uuid4().hex[:12], device_id, command)
        with self.lock:
            self.commands[item.command_id] = item
        return item

    def get(self, command_id: str) -> Command | None:
        with self.lock:
            return self.commands.get(command_id)

    def transition(self, command_id: str, state: str) -> Command | None:
        with self.lock:
            item = self.commands.get(command_id)
            if item is None:
                return None
            if item.state != "pending_confirmation":
                return item
            item.state = state
            item.detail = {
                "applied": "Confirmed by human operator",
                "rejected": "Rejected by human operator",
                "error": "Backend processing error",
            }.get(state, item.detail)
            item.audit.append(f"transition:{state}")
            return item


STORE = DeviceState()


def as_json(item: Command) -> dict:
    return asdict(item)


class Handler(BaseHTTPRequestHandler):
    server_version = "ESP32DeviceAPI/1.0"

    def _send(self, code: int, payload: dict) -> None:
        body = json.dumps(payload).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        if self.path == "/api/device/v1/health":
            self._send(200, {
                "status": "ok",
                "service": "device-api",
                "physical_device": "unknown",
            })
            return

        prefix = "/api/device/v1/commands/"
        if self.path.startswith(prefix):
            command_id = self.path[len(prefix):]
            if command_id.endswith("/confirm") or command_id.endswith("/reject"):
                self._send(405, {"error": "use_post"})
                return
            item = STORE.get(command_id)
            if item is None:
                self._send(404, {"error": "command_not_found"})
                return
            self._send(200, as_json(item))
            return

        self._send(404, {"error": "not_found"})

    def do_POST(self) -> None:
        length = int(self.headers.get("Content-Length", "0"))
        raw = self.rfile.read(length) if length else b"{}"
        try:
            payload = json.loads(raw or b"{}")
        except json.JSONDecodeError:
            self._send(400, {"error": "invalid_json"})
            return

        if self.path == "/api/device/v1/commands":
            device_id = str(payload.get("device_id", "")).strip()
            command = str(payload.get("command", "")).strip()
            require_confirmation = bool(payload.get("require_confirmation", True))
            if not command:
                self._send(400, {"error": "empty_command"})
                return
            if not require_confirmation:
                self._send(409, {"error": "confirmation_required"})
                return
            self._send(202, as_json(STORE.create(device_id, command)))
            return

        prefix = "/api/device/v1/commands/"
        if self.path.startswith(prefix) and self.path.endswith("/confirm"):
            command_id = self.path[len(prefix):-len("/confirm")]
            item = STORE.transition(command_id, "applied")
            if item is None:
                self._send(404, {"error": "command_not_found"})
            else:
                self._send(200, as_json(item))
            return

        if self.path.startswith(prefix) and self.path.endswith("/reject"):
            command_id = self.path[len(prefix):-len("/reject")]
            item = STORE.transition(command_id, "rejected")
            if item is None:
                self._send(404, {"error": "command_not_found"})
            else:
                self._send(200, as_json(item))
            return

        self._send(404, {"error": "not_found"})

    def log_message(self, *_args) -> None:
        return


def make_server(host: str = "127.0.0.1", port: int = 0) -> ThreadingHTTPServer:
    return ThreadingHTTPServer((host, port), Handler)
