"""Run the official CCS'23 pTPSI GS-phase benchmark.

The full pTPSI protocol is GS set-size test + F-count (SPDZ) + PSI.  The
official repository only ships the GS-phase prototype, so this module reports
that phase separately and never pretends it is the full 7.36 s protocol.
"""

from __future__ import annotations

import os
import re
import subprocess
import time
from typing import Any

_DEFAULT = os.path.expanduser("~/psi-demo/paper-gs/gs20")
_CACHE: dict[str, Any] = {"at": 0.0, "payload": None}
_TTL = 30.0


def binary_path() -> str:
    return os.environ.get("PAPER_GS_BIN", _DEFAULT)


def run_paper_gs(force: bool = False) -> dict[str, Any]:
    now = time.time()
    if not force and _CACHE["payload"] and now - float(_CACHE["at"] or 0.0) < _TTL:
        return dict(_CACHE["payload"])

    path = binary_path()
    if not os.path.isfile(path):
        return {
            "available": False,
            "error": "未找到官方 GS 基准二进制",
            "binary": path,
            "hint": "在 Linux 上编译 demo/thirdparty/ptpsi-ccs23/gs.cpp（N=2^20）并设置 PAPER_GS_BIN。",
        }

    started = time.time()
    try:
        proc = subprocess.run(
            [path],
            capture_output=True,
            text=True,
            timeout=120,
        )
    except (OSError, subprocess.TimeoutExpired) as exc:
        return {"available": False, "error": str(exc), "binary": path}

    output = (proc.stdout or "") + (proc.stderr or "")
    match = re.search(r"Total time===\s*([0-9.]+)", output)
    if proc.returncode != 0 or not match:
        return {
            "available": False,
            "error": f"GS 基准退出码 {proc.returncode}，未解析到 Total time",
            "binary": path,
            "output_tail": output[-1200:],
        }

    payload = {
        "available": True,
        "gs_s": float(match.group(1)),
        "wall_s": round(time.time() - started, 2),
        "n_log": 20,
        "n": 2 ** 20,
        "rounds": 8000,
        "threads": 32,
        "paper_total_s": 7.36,
        "paper_gs_test_s": 7.02,
        "source": "官方 Frankhtu/pTPSI gs.cpp（CCS'23 原型，N=2^20，Round=8000）",
        "scope": "仅 GS 阶段；完整 pTPSI 还需 F-count/SPDZ 与 PSI，论文总时间 7.36s。",
        "binary": path,
    }
    _CACHE["at"] = time.time()
    _CACHE["payload"] = dict(payload)
    return payload
