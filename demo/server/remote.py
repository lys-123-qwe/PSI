"""Bridge from a local demo server to one or more Linux PSI backends.

The real Qin-3 binary is a Linux ELF.  When the demo runs on Windows (or on a
Linux box without the binary), ``PSI_REMOTE`` / ``demo/remote-backend.txt``
points at one or more demo servers that do have the binary.  The first
reachable backend with a real binary wins; if it refuses a run, the next one is
tried before the request fails.

Only the Python standard library is used.  The module is inert unless a
backend list is configured, and it never forwards mock runs.
"""

from __future__ import annotations

import json
import os
import re
import socket
import threading
import time
from concurrent.futures import ThreadPoolExecutor
from typing import Any
from urllib import error, request
from urllib.parse import urlsplit

_LOCK = threading.Lock()
_CACHE: dict[str, Any] = {"at": 0.0, "payload": None}
_INFLIGHT = False
_INFLIGHT_URL = ""
_CONFIG_FILE = os.path.join(
    os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
    "remote-backend.txt",
)

_STATUS_TIMEOUT = float(os.environ.get("PSI_REMOTE_STATUS_TIMEOUT", "2.5"))
_RUN_TIMEOUT = float(os.environ.get("PSI_REMOTE_RUN_TIMEOUT", "420"))
_CACHE_TTL = float(os.environ.get("PSI_REMOTE_CACHE_TTL", "12"))
_MAX_BACKENDS = int(os.environ.get("PSI_REMOTE_MAX", "8"))

# Ignore system-wide proxy settings: the lab servers are reached by direct IP
# addresses, and a corporate proxy would otherwise turn a working backend into
# "unreachable".
_OPENER = request.build_opener(request.ProxyHandler({}))


class RemoteError(RuntimeError):
    """Base class for upstream failures."""


class RemoteUnavailable(RemoteError):
    """The backend could not be reached, so trying the next one is safe."""

    def __init__(self, url: str, message: str) -> None:
        self.url = url
        super().__init__(message)


class RemoteTimeout(RemoteError):
    """The backend accepted the request but did not answer in time.

    A real PSI run may already be executing, so this must not trigger a retry
    on another backend.
    """

    def __init__(self, url: str, message: str) -> None:
        self.url = url
        super().__init__(message)


class RemoteHTTPError(RemoteError):
    """The backend answered with an HTTP error."""

    def __init__(self, url: str, code: int, message: str) -> None:
        self.url = url
        self.code = code
        super().__init__(message)


def _normalize_url(raw: str) -> str:
    text = raw.strip().lstrip("\ufeff").strip('"').strip("'").rstrip("/")
    if not text:
        return ""
    if not re.match(r"^[a-zA-Z][a-zA-Z0-9+.-]*://", text):
        text = "http://" + text
    try:
        parsed = urlsplit(text)
    except ValueError:
        return ""
    if parsed.scheme not in {"http", "https"} or not parsed.netloc:
        return ""
    return text


def _split_urls(raw: str) -> list[str]:
    out: list[str] = []
    seen: set[str] = set()
    for chunk in re.split(r"[\s,;]+", raw):
        url = _normalize_url(chunk)
        if not url:
            continue
        key = url.lower()
        if key in seen:
            continue
        seen.add(key)
        out.append(url)
        if len(out) >= _MAX_BACKENDS:
            break
    return out


def remote_urls() -> list[str]:
    """Return configured upstream demo servers in priority order.

    Precedence: environment variable, then ``demo/remote-backend.txt``.  Both
    accept multiple URLs separated by newlines, commas, semicolons or spaces.
    The text file is intentionally git-ignored so a public repository never
    leaks lab server addresses; ``remote-backend.example.txt`` documents it.
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
                lines = []
                for line in handle:
                    text = line.split("#", 1)[0].strip()
                    if text:
                        lines.append(text)
                raw = "\n".join(lines)
        except OSError:
            raw = ""
    return _split_urls(raw)


def remote_url() -> str:
    """First configured backend, kept for compatibility with older callers."""
    urls = remote_urls()
    return urls[0] if urls else ""


def configured() -> bool:
    return bool(remote_urls())


def _friendly_error(exc: BaseException, url: str) -> str:
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
        return f"远程后端 {url} HTTP {exc.code}: {message}".strip()
    if isinstance(exc, (socket.timeout, TimeoutError)):
        return f"远程后端 {url} 响应超时"
    if isinstance(exc, error.URLError):
        reason = getattr(exc, "reason", exc)
        if isinstance(reason, (socket.timeout, TimeoutError)):
            return f"远程后端 {url} 响应超时"
        return f"无法连接远程后端 {url}：{reason}"
    return str(exc) or exc.__class__.__name__


def _request_json(path: str, method: str = "GET", payload: dict | None = None,
                  timeout: float | None = None, base_url: str = "") -> dict[str, Any]:
    url = (base_url or remote_url()).rstrip("/")
    if not url:
        raise RemoteUnavailable("", "未配置 PSI_REMOTE")
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
        message = _friendly_error(exc, url)
        raise RemoteHTTPError(url, int(exc.code), message) from None
    except (error.URLError, socket.timeout, TimeoutError) as exc:
        message = _friendly_error(exc, url)
        reason = getattr(exc, "reason", exc)
        if isinstance(exc, (socket.timeout, TimeoutError)) or isinstance(reason, (socket.timeout, TimeoutError)):
            raise RemoteTimeout(url, message) from None
        raise RemoteUnavailable(url, message) from None
    try:
        parsed = json.loads(body.decode("utf-8"))
    except (UnicodeDecodeError, ValueError):
        raise RemoteUnavailable(url, f"远程后端 {url} 返回的不是 JSON（请检查地址是否指向 PSI Demo）") from None
    if not isinstance(parsed, dict):
        raise RemoteUnavailable(url, f"远程后端 {url} 返回格式异常")
    return parsed


def _probe(url: str) -> dict[str, Any]:
    result: dict[str, Any] = {
        "url": url,
        "reachable": False,
        "ready": False,
        "backend": None,
        "version": None,
        "active": False,
        "error": "",
    }
    try:
        health = _request_json("/api/health", timeout=_STATUS_TIMEOUT, base_url=url)
    except RemoteError as exc:
        result["error"] = str(exc)
        return result

    backend = health.get("backend") or {}
    if not isinstance(backend, dict):
        backend = {}
    result["reachable"] = bool(health.get("ok"))
    result["backend"] = backend
    result["version"] = health.get("version")
    result["active"] = bool(health.get("active"))
    result["ready"] = bool(health.get("ok")) and bool(backend.get("exists"))
    if not health.get("ok"):
        result["error"] = "健康检查返回 ok=false"
    elif not backend.get("exists"):
        result["error"] = backend.get("note") or "远程服务可达，但没有配置真实后端"
    return result


def _empty_status() -> dict[str, Any]:
    return {
        "configured": False,
        "urls": [],
        "url": "",
        "reachable": False,
        "ready": False,
        "backend": None,
        "version": None,
        "active": False,
        "candidates": [],
        "fallback_used": False,
        "checked_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "error": "",
    }


def status(force: bool = False) -> dict[str, Any]:
    """Cached health snapshot of every configured backend."""
    urls = remote_urls()
    if not urls:
        return _empty_status()

    now = time.time()
    with _LOCK:
        cached = _CACHE["payload"]
        if not force and cached and now - float(_CACHE["at"] or 0.0) < _CACHE_TTL:
            return dict(cached)

    workers = max(1, min(4, len(urls)))
    with ThreadPoolExecutor(max_workers=workers) as pool:
        candidates = list(pool.map(_probe, urls))

    ready = [item for item in candidates if item.get("ready")]
    reachable = [item for item in candidates if item.get("reachable")]
    if ready:
        active = ready[0]
    elif reachable:
        active = reachable[0]
    else:
        active = candidates[0]

    errors = [item.get("error", "") for item in candidates if item.get("error")]
    payload: dict[str, Any] = {
        "configured": True,
        "urls": urls,
        "url": active.get("url", urls[0]),
        "reachable": bool(reachable),
        "ready": bool(ready),
        "backend": active.get("backend") if active.get("reachable") else None,
        "version": active.get("version"),
        "active": bool(active.get("active")),
        "candidates": candidates,
        "fallback_used": bool(ready) and ready[0].get("url") != urls[0],
        "checked_at": time.strftime("%Y-%m-%d %H:%M:%S"),
        "error": "" if ready else ("；".join(errors) or "没有可用的真实后端"),
    }

    with _LOCK:
        _CACHE["at"] = time.time()
        _CACHE["payload"] = dict(payload)
    return payload


def _compact_candidates(snapshot: dict[str, Any]) -> list[dict[str, Any]]:
    out = []
    selected_url = snapshot.get("url", "")
    for index, item in enumerate(snapshot.get("candidates", [])):
        backend = item.get("backend") or {}
        url = item.get("url", "")
        out.append({
            "url": url,
            "index": index,
            "role": "primary" if index == 0 else "backup",
            "selected": bool(url) and url == selected_url,
            "reachable": bool(item.get("reachable")),
            "ready": bool(item.get("ready")),
            "version": item.get("version"),
            "active": bool(item.get("active")),
            "backend_mode": backend.get("mode") if isinstance(backend, dict) else None,
            "error": item.get("error", ""),
        })
    return out


def summary() -> dict[str, Any]:
    """Compact remote status for /api/config and the browser banner."""
    snapshot = status()
    backend = snapshot.get("backend") or {}
    return {
        "configured": snapshot.get("configured", False),
        "url": snapshot.get("url", ""),
        "urls": snapshot.get("urls", []),
        "reachable": snapshot.get("reachable", False),
        "ready": snapshot.get("ready", False),
        "backend_mode": backend.get("mode") if isinstance(backend, dict) else None,
        "backend_exists": bool(backend.get("exists")) if isinstance(backend, dict) else False,
        "patched": bool(backend.get("patched")) if isinstance(backend, dict) else False,
        "version": snapshot.get("version"),
        "active": bool(snapshot.get("active")),
        "fallback_used": bool(snapshot.get("fallback_used")),
        "ready_count": sum(1 for item in snapshot.get("candidates", []) if item.get("ready")),
        "selected_url": snapshot.get("url", ""),
        "selected_index": next(
            (
                index
                for index, item in enumerate(snapshot.get("candidates", []))
                if item.get("url") and item.get("url") == snapshot.get("url")
            ),
            -1,
        ),
        "candidates": _compact_candidates(snapshot),
        "error": snapshot.get("error", ""),
        "checked_at": snapshot.get("checked_at", ""),
    }


def is_inflight() -> bool:
    with _LOCK:
        return _INFLIGHT


def _urls_for_run(snapshot: dict[str, Any]) -> list[str]:
    candidates = snapshot.get("candidates", [])
    ready = [item.get("url") for item in candidates if item.get("ready")]
    others = [item.get("url") for item in candidates if not item.get("ready")]
    ordered = [url for url in ready + others if url]
    return ordered or snapshot.get("urls", [])


def _preferred_url() -> str:
    snapshot = status()
    url = snapshot.get("url") or ""
    if url:
        return url
    urls = remote_urls()
    return urls[0] if urls else ""


def proxy_run(payload: dict[str, Any]) -> dict[str, Any]:
    """Run on the first backend that answers, failing over before it starts."""
    global _INFLIGHT, _INFLIGHT_URL

    snapshot = status(force=True)
    urls = _urls_for_run(snapshot)
    if not urls:
        raise RemoteUnavailable("", "未配置 PSI_REMOTE")

    with _LOCK:
        _INFLIGHT = True
        _INFLIGHT_URL = urls[0]
        _CACHE["at"] = 0.0

    last_error: RemoteError | None = None
    try:
        for url in urls:
            with _LOCK:
                _INFLIGHT_URL = url
            try:
                return _request_json("/api/run", "POST", payload, timeout=_RUN_TIMEOUT, base_url=url)
            except RemoteTimeout:
                # The job may already be running; retrying elsewhere could
                # duplicate work.
                raise
            except RemoteHTTPError as exc:
                if int(exc.code) in {404, 502, 503, 504}:
                    last_error = exc
                    continue
                raise
            except RemoteUnavailable as exc:
                last_error = exc
                continue
        if last_error is not None:
            raise last_error
        raise RemoteUnavailable("", "没有可用的真实后端")
    finally:
        with _LOCK:
            _INFLIGHT = False
            _INFLIGHT_URL = ""
            _CACHE["at"] = 0.0


def _inflight_or_preferred_url() -> str:
    with _LOCK:
        url = _INFLIGHT_URL
    return url or _preferred_url()


def proxy_progress() -> dict[str, Any]:
    return _request_json("/api/progress", timeout=_STATUS_TIMEOUT, base_url=_inflight_or_preferred_url())


def proxy_cancel() -> dict[str, Any]:
    return _request_json("/api/cancel", "POST", {}, timeout=_STATUS_TIMEOUT, base_url=_inflight_or_preferred_url())


def _request_first(path: str, timeout: float) -> dict[str, Any]:
    snapshot = status(force=True)
    urls = _urls_for_run(snapshot)
    last_error: RemoteError | None = None
    for url in urls:
        try:
            return _request_json(path, timeout=timeout, base_url=url)
        except RemoteTimeout:
            raise
        except RemoteHTTPError as exc:
            if int(exc.code) in {404, 502, 503, 504}:
                last_error = exc
                continue
            raise
        except RemoteUnavailable as exc:
            last_error = exc
            continue
    if last_error is not None:
        raise last_error
    raise RemoteUnavailable("", "没有可用的真实后端")


def proxy_paper_bench() -> dict[str, Any]:
    return _request_first("/api/paper-bench", timeout=120)


def proxy_paper_path(run: bool = False) -> dict[str, Any]:
    path = "/api/paper-path" + ("?run=1" if run else "")
    return _request_first(path, timeout=90)
