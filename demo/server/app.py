"""Tiny stdlib web server for the PSI demo (no third-party dependencies)."""

from __future__ import annotations

import argparse
import json
import mimetypes
import os
import sys
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse, parse_qs

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
WEB = os.path.join(ROOT, "web")
sys.path.insert(0, HERE)

from diagnostics import APP_VERSION, selfcheck, server_info  # noqa: E402
from orchestrator import backend_status, bench, cancel, progress, run  # noqa: E402
from scenarios import DISABLED_SCENARIOS, SCENARIOS, default_config  # noqa: E402

MAX_BODY = 48 * 1024 * 1024  # 3 x 131,072 hashes fit comfortably


def _scenario_payload() -> dict:
    return {
        "scenarios": [
            {
                **scenario,
                "defaults": default_config(key),
            }
            for key, scenario in SCENARIOS.items()
        ],
        "disabled": DISABLED_SCENARIOS,
        "backend": backend_status(),
    }


class Handler(BaseHTTPRequestHandler):
    server_version = "PSIDemo/1.0"

    def log_message(self, fmt: str, *args) -> None:
        sys.stdout.write("[web] " + fmt % args + "\n")
        sys.stdout.flush()

    def _json(self, status: int, payload: dict) -> None:
        body = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "no-referrer")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def _file(self, path: str, content_type: str | None = None) -> None:
        if not os.path.isfile(path):
            self.send_error(404, "Not found")
            return
        with open(path, "rb") as handle:
            body = handle.read()
        self.send_response(200)
        self.send_header("Content-Type", content_type or mimetypes.guess_type(path)[0] or "application/octet-stream")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("X-Content-Type-Options", "nosniff")
        self.send_header("Referrer-Policy", "no-referrer")
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        path = parsed.path
        if path in ("/", "/index.html"):
            self._file(os.path.join(WEB, "index.html"), "text/html; charset=utf-8")
            return
        if path in ("/real", "/real.html"):
            self._file(os.path.join(WEB, "real.html"), "text/html; charset=utf-8")
            return
        if path == "/api/config":
            self._json(200, _scenario_payload())
            return
        if path == "/api/health":
            info = server_info(self.server.demo_host, self.server.demo_port)
            self._json(200, {
                "ok": True,
                "version": info["version"],
                "uptime_s": info["uptime_s"],
                "backend": info["backend"],
                "active": bool(progress().get("active")),
            })
            return
        if path == "/api/version":
            self._json(200, server_info(self.server.demo_host, self.server.demo_port))
            return
        if path == "/api/selfcheck":
            self._json(200, selfcheck(self.server.demo_host, self.server.demo_port))
            return
        if path == "/api/progress":
            self._json(200, progress())
            return
        if path == "/api/bench":
            self._json(200, bench())
            return
        if path == "/api/defaults":
            query = parse_qs(parsed.query)
            scenario = (query.get("scenario") or ["ad"])[0]
            try:
                self._json(200, default_config(scenario))
            except ValueError as exc:
                self._json(404, {"error": str(exc)})
            return
        if path.startswith("/api/"):
            self._json(404, {"error": "未知接口"})
            return
        if path.startswith("/static/"):
            rel = path[len("/static/"):].lstrip("/")
            safe = os.path.normpath(os.path.join(WEB, rel))
            if not safe.startswith(WEB + os.sep):
                self.send_error(403, "Forbidden")
                return
            self._file(safe)
            return
        self.send_error(404, "Not found")

    def do_POST(self) -> None:  # noqa: N802
        path = urlparse(self.path).path
        if path == "/api/cancel":
            self._json(200, cancel())
            return
        if path != "/api/run":
            self._json(404, {"error": "未知接口"})
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
            if length > MAX_BODY:
                self._json(413, {"error": "请求体过大（上限 48MB；三方各 2^17 条约 13MB）"})
                return
            if length <= 0:
                raw = b"{}"
            else:
                chunks = []
                remaining = length
                while remaining > 0:
                    chunk = self.rfile.read(min(remaining, 1024 * 1024))
                    if not chunk:
                        break
                    chunks.append(chunk)
                    remaining -= len(chunk)
                raw = b"".join(chunks)
            payload = json.loads(raw.decode("utf-8"))
            if not isinstance(payload, dict):
                self._json(400, {"error": "请求体必须是 JSON 对象"})
                return
            result = run(payload)
            self._json(200, result)
        except json.JSONDecodeError:
            self._json(400, {"error": "请求体不是合法 JSON"})
        except Exception as exc:  # noqa: BLE001 - surface the message to the demo UI
            self._json(400, {"error": str(exc)})


class DemoServer(ThreadingHTTPServer):
    """Threaded server that remembers the bind address for diagnostics."""

    daemon_threads = True

    def __init__(self, address, handler, demo_host: str, demo_port: int):
        super().__init__(address, handler)
        self.demo_host = demo_host
        self.demo_port = demo_port


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8787)
    args = parser.parse_args()
    httpd = DemoServer((args.host, args.port), Handler, args.host, args.port)
    info = server_info(args.host, args.port)
    print(f"PSI demo v{APP_VERSION} bind {args.host}:{args.port}", flush=True)
    for url in info["urls"]:
        print(f"open: {url}", flush=True)
    print(f"backend: {info['backend']}", flush=True)
    print(f"front-end: {info['asset_stamp'] or 'unknown'}", flush=True)
    try:
        httpd.serve_forever()
    except KeyboardInterrupt:
        print("\nbye", flush=True)


if __name__ == "__main__":
    main()
