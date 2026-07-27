#!/usr/bin/env python3
import argparse
import json
import secrets
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlsplit

PROJECT_ROOT = Path(__file__).resolve().parents[1]
WEB_PAGE_HEADER = PROJECT_ROOT / "src" / "web" / "WebPage.h"
VERSION_FILE = PROJECT_ROOT / "VERSION"
HTML_START = 'R"HTML('
HTML_END = ')HTML";'
BUSY_OTA_STATUSES = {"checking", "downloading", "verifying", "rebooting"}


def extract_web_page(path: Path = WEB_PAGE_HEADER) -> str:
    source = path.read_text(encoding="utf-8")
    start = source.find(HTML_START)
    end = source.rfind(HTML_END)
    if start < 0 or end < 0 or end <= start:
        raise ValueError(f"could not extract embedded HTML from {path}")
    return source[start + len(HTML_START) : end]


def next_patch_version(version: str) -> str:
    major, minor, patch = (int(part) for part in version.split("."))
    return f"{major}.{minor}.{patch + 1}"


class PreviewApplication:
    def __init__(
        self,
        available_version: str | None = None,
        delay_scale: float = 1.0,
        log_requests: bool = True,
    ):
        self.html = extract_web_page().encode("utf-8")
        self.delay_scale = delay_scale
        self.log_requests = log_requests
        current_version = VERSION_FILE.read_text(encoding="ascii").strip()
        self.available_version = available_version or next_patch_version(current_version)
        self.lock = threading.Lock()
        self.state = {
            "power": False,
            "mode": "potentiometer",
            "manualBrightness": 50,
            "potentiometerBrightness": 35,
            "potentiometerAdc": 1433,
            "outputBrightness": 0,
            "wifiConnected": True,
            "portalActive": False,
            "rssi": -48,
            "ip": "127.0.0.1",
            "hostname": "localhost",
            "firmwareVersion": current_version,
            "firmwareBuild": "preview",
            "otaStatus": "idle",
            "otaAvailableVersion": "",
            "otaProgress": 0,
            "otaLastCheck": 0,
            "otaUpdateAvailable": False,
            "otaError": "",
            "csrfToken": secrets.token_hex(8),
        }

    def snapshot(self) -> dict:
        with self.lock:
            return dict(self.state)

    def authorize(self, token: str | None) -> bool:
        with self.lock:
            return secrets.compare_digest(token or "", self.state["csrfToken"])

    def control(self, values: dict[str, list[str]]) -> dict:
        if "power" not in values and "brightness" not in values:
            raise ValueError("Nenhum controle foi informado")

        with self.lock:
            if "power" in values:
                value = values["power"][-1]
                if value not in {"true", "false", "1", "0"}:
                    raise ValueError("Valor de power invalido")
                self.state["power"] = value in {"true", "1"}

            if "brightness" in values:
                try:
                    brightness = int(values["brightness"][-1])
                except ValueError as error:
                    raise ValueError("Brightness deve estar entre 0 e 100") from error
                if not 0 <= brightness <= 100:
                    raise ValueError("Brightness deve estar entre 0 e 100")
                self.state["manualBrightness"] = brightness
                self.state["mode"] = "web"
                self.state["power"] = True

            active_brightness = (
                self.state["potentiometerBrightness"]
                if self.state["mode"] == "potentiometer"
                else self.state["manualBrightness"]
            )
            self.state["outputBrightness"] = (
                active_brightness if self.state["power"] else 0
            )
            return dict(self.state)

    def factory_reset(self) -> None:
        with self.lock:
            self.state.update(
                power=False,
                mode="potentiometer",
                manualBrightness=50,
                outputBrightness=0,
            )

    def start_wifi_reset(self) -> None:
        with self.lock:
            self.state["wifiConnected"] = False
            self.state["portalActive"] = True
            self.state["rssi"] = 0
        threading.Thread(target=self._finish_wifi_reset, daemon=True).start()

    def start_ota_check(self) -> bool:
        with self.lock:
            if self.state["otaStatus"] in BUSY_OTA_STATUSES:
                return False
            self.state.update(otaStatus="checking", otaProgress=0, otaError="")
        threading.Thread(target=self._finish_ota_check, daemon=True).start()
        return True

    def start_ota_install(self) -> bool:
        with self.lock:
            if (
                self.state["otaStatus"] in BUSY_OTA_STATUSES
                or not self.state["otaUpdateAvailable"]
            ):
                return False
            self.state.update(otaStatus="checking", otaProgress=0, otaError="")
        threading.Thread(target=self._run_ota_install, daemon=True).start()
        return True

    def _sleep(self, seconds: float) -> None:
        time.sleep(seconds * self.delay_scale)

    def _finish_wifi_reset(self) -> None:
        self._sleep(1.5)
        with self.lock:
            self.state.update(wifiConnected=True, portalActive=False, rssi=-48)

    def _finish_ota_check(self) -> None:
        self._sleep(1.0)
        with self.lock:
            self.state.update(
                otaStatus="available",
                otaAvailableVersion=self.available_version,
                otaLastCheck=int(time.time()),
                otaUpdateAvailable=True,
            )

    def _run_ota_install(self) -> None:
        self._sleep(0.5)
        with self.lock:
            self.state["otaStatus"] = "downloading"
        for progress in range(0, 101, 5):
            with self.lock:
                self.state["otaProgress"] = progress
            self._sleep(0.12)

        with self.lock:
            self.state["otaStatus"] = "verifying"
        self._sleep(0.8)
        with self.lock:
            self.state["otaStatus"] = "rebooting"
        self._sleep(1.0)
        with self.lock:
            self.state.update(
                firmwareVersion=self.available_version,
                firmwareBuild="preview-ota",
                otaStatus="up_to_date",
                otaAvailableVersion=self.available_version,
                otaProgress=0,
                otaLastCheck=int(time.time()),
                otaUpdateAvailable=False,
                csrfToken=secrets.token_hex(8),
            )


class PreviewHandler(BaseHTTPRequestHandler):
    application: PreviewApplication

    def do_GET(self) -> None:
        path = urlsplit(self.path).path
        if path == "/":
            self._send_bytes(200, "text/html; charset=utf-8", self.application.html)
        elif path == "/api/state":
            self._send_json(200, self.application.snapshot())
        elif path.startswith("/api/"):
            self._send_error(404, "Endpoint nao encontrado")
        else:
            self.send_response(302)
            self.send_header("Location", "/")
            self.end_headers()

    def do_POST(self) -> None:
        if not self.application.authorize(self.headers.get("X-CSRF-Token")):
            self._send_error(403, "Token de seguranca ausente ou invalido")
            return

        path = urlsplit(self.path).path
        if path == "/api/control":
            try:
                self._send_json(200, self.application.control(self._read_form()))
            except ValueError as error:
                self._send_error(400, str(error))
        elif path == "/api/wifi/reset":
            self.application.start_wifi_reset()
            self._send_json(
                202, {"ok": True, "message": "Reinicio Wi-Fi simulado"}
            )
        elif path == "/api/factory-reset":
            self.application.factory_reset()
            self._send_json(
                202, {"ok": True, "message": "Restauracao simulada"}
            )
        elif path == "/api/ota/check":
            if not self.application.start_ota_check():
                self._send_error(409, "Uma operacao OTA ja esta em andamento")
                return
            self._send_json(202, {"ok": True, "message": "Consulta OTA simulada"})
        elif path == "/api/ota/install":
            if not self.application.start_ota_install():
                self._send_error(409, "Nenhuma atualizacao esta disponivel")
                return
            self._send_json(202, {"ok": True, "message": "Atualizacao OTA simulada"})
        else:
            self._send_error(404, "Endpoint nao encontrado")

    def _read_form(self) -> dict[str, list[str]]:
        try:
            content_length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            content_length = 0
        if content_length < 0 or content_length > 4096:
            raise ValueError("Corpo da requisicao invalido")
        body = self.rfile.read(content_length).decode("utf-8")
        return parse_qs(body, keep_blank_values=True)

    def _send_error(self, status: int, message: str) -> None:
        self._send_json(status, {"ok": False, "error": message})

    def _send_json(self, status: int, payload: dict) -> None:
        content = json.dumps(payload, ensure_ascii=True, separators=(",", ":")).encode(
            "ascii"
        )
        self._send_bytes(status, "application/json", content)

    def _send_bytes(self, status: int, content_type: str, content: bytes) -> None:
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(content)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(content)

    def log_message(self, message_format: str, *args) -> None:
        if self.application.log_requests:
            print(f"[preview] {self.address_string()} {message_format % args}")


def create_server(
    host: str = "127.0.0.1",
    port: int = 8000,
    application: PreviewApplication | None = None,
) -> ThreadingHTTPServer:
    configured_handler = type(
        "ConfiguredPreviewHandler",
        (PreviewHandler,),
        {"application": application or PreviewApplication()},
    )
    server = ThreadingHTTPServer((host, port), configured_handler)
    server.daemon_threads = True
    return server


def main() -> None:
    parser = argparse.ArgumentParser(description="Preview the embedded ESP32 web UI")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8000)
    parser.add_argument(
        "--update-version",
        help="version exposed by the simulated OTA check (default: next patch)",
    )
    args = parser.parse_args()

    application = PreviewApplication(available_version=args.update_version)
    server = create_server(args.host, args.port, application)
    print(f"Web preview: http://{args.host}:{server.server_port}")
    print(
        f"Firmware {application.state['firmwareVersion']}; simulated OTA "
        f"{application.available_version}"
    )
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nPreview stopped")
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
