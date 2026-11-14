"""Deployment diagnostics for the PSI demo (stdlib only)."""

from __future__ import annotations

import os
import platform
import socket
import sys
import time
from typing import Any

import remote
from backends import effective_backend
from orchestrator import backend_status, bench, progress

APP_NAME = "PSI Lab Demo"
APP_VERSION = "1.1.0"
STARTED_AT = time.time()

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
WEB = os.path.join(ROOT, "web")
DATA = os.path.join(ROOT, "data")


def _fmt_time(ts: float) -> str:
    return time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(ts))


def lan_urls(host: str, port: int) -> list[str]:
    """Addresses the demo can be opened at from another machine."""
    if host not in ("", "0.0.0.0", "::"):
        return [f"http://{host}:{port}"]
    ips: set[str] = set()
    try:
        for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            ip = str(info[4][0])
            if not ip.startswith("127."):
                ips.add(ip)
    except OSError:
        pass
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.connect(("8.8.8.8", 80))
            ip = str(sock.getsockname()[0])
            if not ip.startswith("127."):
                ips.add(ip)
    except OSError:
        pass
    ordered = sorted(ips, key=lambda value: tuple(int(part) for part in value.split(".")))
    urls = [f"http://{ip}:{port}" for ip in ordered]
    return urls or [f"http://127.0.0.1:{port}"]


def asset_stamp() -> str | None:
    """Newest front-end asset mtime, useful for confirming a redeploy."""
    latest = 0.0
    for name in ("index.html", "styles.css", "app.js", "favicon.svg"):
        try:
            latest = max(latest, os.path.getmtime(os.path.join(WEB, name)))
        except OSError:
            continue
    return _fmt_time(latest) if latest else None


def server_info(host: str, port: int, backend: dict[str, Any] | None = None) -> dict[str, Any]:
    return {
        "app": APP_NAME,
        "version": APP_VERSION,
        "started_at": _fmt_time(STARTED_AT),
        "uptime_s": round(time.time() - STARTED_AT, 1),
        "python": platform.python_version(),
        "platform": platform.platform(),
        "host": host,
        "port": port,
        "urls": lan_urls(host, port),
        "asset_stamp": asset_stamp(),
        "backend": backend if backend is not None else backend_status(),
    }


def _check(status: str, label: str, detail: str) -> dict[str, str]:
    return {"status": status, "label": label, "detail": detail}


def selfcheck(host: str, port: int) -> dict[str, Any]:
    """Read-only checks an operator can run before a live demo."""
    checks: list[dict[str, str]] = []

    required = ("index.html", "styles.css", "app.js", "favicon.svg")
    missing = [name for name in required if not os.path.isfile(os.path.join(WEB, name))]
    checks.append(_check(
        "fail" if missing else "ok",
        "前端文件",
        f"缺少：{', '.join(missing)}" if missing else "index.html / styles.css / app.js / favicon.svg 均存在",
    ))

    checks.append(_check(
        "ok" if sys.version_info >= (3, 8) else "fail",
        "Python",
        f"Python {platform.python_version()}（需要 3.8+）",
    ))

    backend = effective_backend()
    if backend["exists"]:
        if backend.get("source") == "remote":
            checks.append(_check(
                "ok",
                "真实后端",
                f"远程 {backend.get('remote_url') or '未知地址'}（{backend.get('binary') or '二进制未知'}）",
            ))
        else:
            executable = True
            if os.name == "posix":
                executable = os.access(backend["binary"], os.X_OK)
            checks.append(_check(
                "ok" if executable else "fail",
                "真实后端",
                f"{backend['binary']}（{backend.get('binary_mtime') or '时间未知'}，可执行={executable}）",
            ))
    else:
        checks.append(_check(
            "warn",
            "真实后端",
            backend.get("note") or "未设置 MYPSI_BIN，当前只能跑 mock。",
        ))

    if remote.configured() and backend.get("source") != "remote":
        remote_snapshot = remote.status()
        checks.append(_check(
            "ok" if remote_snapshot.get("reachable") else "warn",
            "远程桥接",
            (
                f"PSI_REMOTE={remote_snapshot.get('url')} 可达"
                if remote_snapshot.get("reachable")
                else f"PSI_REMOTE={remote_snapshot.get('url')} 不可达：{remote_snapshot.get('error') or '未知原因'}"
            ),
        ))

    bench_data = bench()
    if bench_data.get("available"):
        checks.append(_check(
            "ok",
            "本机实测",
            f"data/bench.json 含 {len(bench_data['cases'])} 组；性能页会显示绿色实测柱。",
        ))
    else:
        checks.append(_check(
            "warn",
            "本机实测",
            "尚无 data/bench.json；性能页只有论文数字。Linux 上运行 bootstrap_linux.sh --bench 生成。",
        ))

    writable = os.access(DATA, os.W_OK)
    checks.append(_check(
        "ok" if writable else "warn",
        "数据目录",
        DATA + ("（可写）" if writable else "（不可写：生成 bench.json 会失败）"),
    ))

    snapshot = progress()
    if snapshot.get("active"):
        checks.append(_check(
            "warn",
            "任务状态",
            f"有任务运行中（stage {snapshot.get('stage')}）；后端固定端口 1212/1213，请串行演示。",
        ))
    else:
        checks.append(_check("ok", "任务状态", "当前空闲，可以开始新一轮演示。"))

    fail = any(item["status"] == "fail" for item in checks)
    warn = any(item["status"] == "warn" for item in checks)
    return {
        "overall": "fail" if fail else ("warn" if warn else "ok"),
        "checked_at": _fmt_time(time.time()),
        "checks": checks,
        "server": server_info(host, port, backend=backend),
    }
