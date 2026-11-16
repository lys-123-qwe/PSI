"""Effective backend selection: local binary first, then an optional remote."""

from __future__ import annotations

import os
from typing import Any

import remote
from orchestrator import backend_status


def effective_backend() -> dict[str, Any]:
    """Describe the backend the web UI should use.

    ``source`` is one of ``local``, ``remote`` or ``mock``.  ``exists`` stays the
    single field the browser checks, so the same UI works for a local Linux
    binary and for a Windows server bridging to a Linux demo server.
    """
    backend = backend_status()
    backend["source"] = "local" if backend["exists"] else "mock"
    backend["public_url"] = os.environ.get("PSI_PUBLIC_URL", "").strip().rstrip("/")

    if backend["exists"]:
        return backend

    snapshot = remote.status()
    backend["remote_configured"] = bool(snapshot.get("configured"))
    backend["remote_url"] = snapshot.get("url", "")
    backend["remote_reachable"] = bool(snapshot.get("reachable"))
    backend["remote_error"] = snapshot.get("error", "")

    remote_backend = snapshot.get("backend") or {}
    if (
        snapshot.get("configured")
        and snapshot.get("reachable")
        and isinstance(remote_backend, dict)
        and remote_backend.get("exists")
    ):
        return {
            **backend,
            "mode": "remote",
            "exists": True,
            "patched": bool(remote_backend.get("patched")),
            "binary": remote_backend.get("binary") or snapshot.get("url", ""),
            "binary_size": remote_backend.get("binary_size"),
            "binary_mtime": remote_backend.get("binary_mtime"),
            "platform": remote_backend.get("platform", "posix"),
            "source": "remote",
            "note": (
                f"本地无 Linux 二进制，已连接远程真实后端 {snapshot.get('url', '')}；"
                "演示模式仍可直接使用。"
            ),
        }

    if snapshot.get("configured"):
        reason = snapshot.get("error") or "远程服务未配置可用的 MYPSI_BIN"
        backend["note"] = f"未配置本地 MYPSI_BIN；远程真实后端不可用（{reason}）。当前只能使用演示模式。"
    else:
        backend["note"] = "未配置本地 MYPSI_BIN；当前只能使用演示模式。"
    return backend
