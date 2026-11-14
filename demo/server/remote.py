"""Optional bridge from a mock-only demo server to a Linux PSI backend.

The real Qin-3 binary is a Linux ELF.  When the demo runs on Windows (or on a
Linux box without the binary), setting ``PSI_REMOTE`` lets the local web server
forward live/file runs to another demo server that does have the binary.  The
browser still talks only to the local server, so one launch exposes both the
mock demo and the real-data page.

This module deliberately uses only the Python standard library.  It is inert
unless ``PSI_REMOTE`` (or ``demo/remote-backend.txt``) is configured, and it
never forwards mock runs.
"""

from __future__ import annotations

import json
import os
import socket
import threading
import time
from typing import Any
from urllib import error, request

_LOCK = threading.Lock()
_CACHE: dict[str, Any] = {"at": 0.0, "payload": None}
_INFLIGHT = False
_CONFIG_FILE = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "remote-backend.txt",
)

_STATUS_TIMEOUT = float(os.environ.get("PSI_REMOTE_STATUS_TIMEOUT", "2.5"))
_RUN_TIMEOUT = float(os.environ.get("PSI_REMOTE_RUN_TIMEOUT", "420"))
_CACHE_TTL = float(os.environ.get("PSI_REMOTE_CACHE_TTL", "12"))

# Ignore system-wide proxy settings: the lab server is usually reached by a
# direct IP address, and a corporate proxy would otherwise turn a working
# backend into "unreachable".
_OPENER = request.build_opener(request.ProxyHandler({}))


def remote_url() -> str:
    """Return the configured upstream demo server, without a trailing slash.

    Precedence: environment variable, then ``demo/remote-backend.txt``.  The
    text file is intentionally git-ignored so a public repository never leaks a
    lab server address; ``remote-backend.example.txt`` documents the format.
    """
    raw = (
        os.environ.get("PSI_REMOTE", "")
        or os.environ.get("PSI_REMOTE_BACKEND", "")
        or os.environ.get("MYPSI_REMOTE", "")
    ).strip()
    if not raw:
        try:
            # utf-8-sig tolerates the BOM that Windows Notepad may add.
            with open(_CONFIG_FILE, "r", encoding="utf-8-sig") as handle:
                for line in handle:
                    text = line.strip()
                    if text and not text.startswith("#"):
                        raw = text
                        break
        except OSError:
            raw = ""
    return raw.rstrip("/") if raw else ""


def configured() -> bool:
    return bool(remote_url())


def _friendly_error(exc: BaseException) -> str:
    if isinstance(exc, error.HTTPError):
        try:
            body = exc.read().decode("utf-8", "replace")
        except Exception:  # noqa: BLE001 - best effort diagnostics
            body = ""
        try:
            payload = json.loads(body)
            message = payload.get("error") or payload.get("message") or body
        except (TypeError, ValueError):
            message = body or exc.reason
        return f"远程后端 HTTP {exc.code}: {message}".strip()
    if isinstance(exc, (socket.timeout, TimeoutError)):
        return "远程后端响应超时"
    if isinstance(exc, error.URLError):
        reason = getattr(exc, "reason", exc)
        if isinstance(reason, (socket.timeout, TimeoutError)):
            return "远程后端响应超时"
        return f"无法连接远程后端：{reason}"
    return str(exc) or exc.__class__.__name__


def _request_json(path: str, method: str = "GET", payload: dict | None = None,
                  timeout: float | None = None) -> dict[str, Any]:
    url = remote_url()
    if not url:
        raise RuntimeError("未配置 PSI_REMOTE")
    data = None
    headers = {"Accept": "application/json"}
    if payload is not None:
        data = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        headers["Content-Type"] = "application/json; charset=utf-8"
    req = request.Request(f"{url}{path}", data=data, headers=headers, method=method)
    try:
        with _OPENER.open(req, timeout=timeout or _STATUS_TIMEOUT) as response:
            body = response.read()
    except error.HTTPError as exc:
        raise RuntimeError(_friendly_error(exc)) from None
    except (error.URLError, socket.timeout, TimeoutError) as exc:
        raise RuntimeError(_friendly_error(exc)) from None
    try:
        parsed = json.loads(body.decode("utf-8"))
    except (UnicodeDecodeError, ValueError):
        raise RuntimeError("远程后端返回的不是 JSON（请检查地址是否指向 PSI Demo）") from None
    if not isinstance(parsed, dict):
        raise RuntimeError("远程后端返回格式异常")
    return parsed


def status(force: bool = False) -> dict[str, Any]:
    """Cached health snapshot of the upstream server."""
    url = remote_url()
    if not url:
        return {
            "configured": False,
            "url": "",
            "reachable": False,
            "backend": None,
            "checked_at": time.strftime("%Y-%m-%d %H:%M:%S"),
            "error": "",
        }

    now = time.time()
    with _LOCK:
        cached = _CACHE["payload"]
        if not force and cached and now - float(_CACHE["at"] or 0.0) < _CACHE_TTL:
            return dict(cached)

    payload: dict[str, Any] = {
        "configured": True,
        "url": url,
        "reachable": False,
        "backend": None,
        "checked_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "error": "",
    }
    try:
        health = _request_json("/api/health", timeout=_STATUS_TIMEOUT)
        payload["reachable"] = True
        payload["backend"] = health.get("backend")
        payload["version"] = health.get("version")
        payload["active"] = bool(health.get("active"))
    except RuntimeError as exc:
        payload["error"] = str(exc)

    with _LOCK:
        _CACHE["at"] = time.time()
        _CACHE["payload"] = dict(payload)
    return payload


def summary() -> dict[str, Any]:
    """Compact remote status for /api/config and the browser banner."""
    snapshot = status()
    backend = snapshot.get("backend") or {}
    return {
        "configured": snapshot.get("configured", False),
        "url": snapshot.get("url", ""),
        "reachable": snapshot.get("reachable", False),
        "backend_mode": backend.get("mode") if isinstance(backend, dict) else None,
        "backend_exists": bool(backend.get("exists")) if isinstance(backend, dict) else False,
        "patched": bool(backend.get("patched")) if isinstance(backend, dict) else False,
        "version": snapshot.get("version"),
        "active": bool(snapshot.get("active")),
        "error": snapshot.get("error", ""),
        "checked_at": snapshot.get("checked_at", ""),
    }


def is_inflight() -> bool:
    with _LOCK:
        return _INFLIGHT


def proxy_run(payload: dict[str, Any]) -> dict[str, Any]:
    global _INFLIGHT
    with _LOCK:
        _INFLIGHT = True
        # Force the next status call to reflect the active remote job.
        _CACHE["at"] = 0.0
    try:
        return _request_json("/api/run", "POST", payload, timeout=_RUN_TIMEOUT)
    finally:
        with _LOCK:
            _INFLIGHT = False
            _CACHE["at"] = 0.0


def proxy_progress() -> dict[str, Any]:
    return _request_json("/api/progress", timeout=_STATUS_TIMEOUT)


def proxy_cancel() -> dict[str, Any]:
    return _request_json("/api/cancel", "POST", {}, timeout=_STATUS_TIMEOUT)
