"""Status and staged runner for the CCS'23 2-party pTPSI paper path.

The official repository ships the GS benchmark and an F-count MPC prototype,
but not the glue code or the final PSI stage.  This module exposes the three
stages explicitly so the demo can show what is measured, what is wired, and
what is still missing instead of presenting an aggregate that does not exist.
"""

from __future__ import annotations

import os
from typing import Any

from paper import binary_path as gs_binary, run_paper_gs

PAPER_TOTAL_S = 7.36
PAPER_PSIZE_TEST_S = 7.02
PAPER_GS_S = 3.52
PAPER_FCOUNT_S = 1.62
PAPER_PSI_S = round(PAPER_TOTAL_S - PAPER_PSIZE_TEST_S, 2)

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
FCOUNT_SOURCE = os.path.join(ROOT, "thirdparty", "ptpsi-ccs23", "F-count-SPDZ.mpc")


def _mp_spdz_dir() -> str:
    return os.environ.get("PAPER_MP_SPDZ_DIR", os.path.expanduser("~/TPSST/MP-SPDZ"))


def _fcount_mpc() -> str:
    return os.environ.get("PAPER_FCOUNT_MPC", FCOUNT_SOURCE)


def _psi_bin() -> str:
    return os.environ.get("PAPER_PSI_BIN", "")


def _stage(stage_id: str, name: str, status: str, scope: str, paper_s: float | None,
           engine: str = "", hint: str = "", local_s: float | None = None,
           **extra: Any) -> dict[str, Any]:
    payload: dict[str, Any] = {
        "id": stage_id,
        "name": name,
        "status": status,
        "scope": scope,
        "paper_s": paper_s,
        "engine": engine,
        "hint": hint,
    }
    if local_s is not None:
        payload["local_s"] = round(float(local_s), 3)
    payload.update(extra)
    return payload


def status(run_gs: bool = False) -> dict[str, Any]:
    """Return the paper-path stage table without pretending stages are wired."""
    gs_path = gs_binary()
    gs_ok = os.path.isfile(gs_path)
    mpc_path = _fcount_mpc()
    mpc_ok = os.path.isfile(mpc_path)
    mp_spdz = _mp_spdz_dir()
    mp_spdz_ok = os.path.isdir(mp_spdz)
    psi_path = _psi_bin()
    psi_ok = bool(psi_path) and os.path.isfile(psi_path)

    gs_stage = _stage(
        "gs",
        "GS 集合大小测试",
        "unavailable" if not gs_ok else "available",
        "仅 GS 阶段（N=2^20、8000 轮）",
        PAPER_GS_S,
        engine=gs_path,
        hint="" if gs_ok else "设置 PAPER_GS_BIN，或先编译官方 gs.cpp（N=2^20）。",
    )
    if run_gs and gs_ok:
        try:
            measured = run_paper_gs(force=True)
        except Exception as exc:  # noqa: BLE001 - surface the exact failure to the UI
            gs_stage.update(status="failed", error=str(exc))
        else:
            if measured.get("available"):
                gs_stage.update(
                    status="measured",
                    local_s=measured.get("gs_s"),
                    wall_s=measured.get("wall_s"),
                    n=measured.get("n"),
                    rounds=measured.get("rounds"),
                    threads=measured.get("threads"),
                )
            else:
                gs_stage.update(
                    status="failed",
                    error=measured.get("error") or "GS 基准未返回结果",
                )

    fcount_stage = _stage(
        "fcount",
        "F∩-count（MP-SPDZ）",
        "not_integrated" if mpc_ok else "unavailable",
        "未接 GS 输出；官方 mpc 是固定小样例",
        PAPER_FCOUNT_S,
        engine=mp_spdz,
        hint=(
            "官方 F-count-SPDZ.mpc 已就位；还缺 GS→MP-SPDZ 的输入生成器和参数化。"
            if mpc_ok else
            "未找到 F-count-SPDZ.mpc；请从官方 Frankhtu/pTPSI 仓库取回。"
        ),
        mpc_path=mpc_path,
        runtime_ready=mp_spdz_ok,
    )

    psi_stage = _stage(
        "psi",
        "最终 PSI（论文 [16]）",
        "not_integrated" if psi_ok else "unavailable",
        "官方 pTPSI 仓库未提供 PSI 阶段",
        PAPER_PSI_S,
        engine=psi_path,
        hint=(
            "" if psi_ok else
            "设置 PAPER_PSI_BIN；需接 2-party OPRF PSI（论文 [16] 或明确标注的兼容实现）。"
        ),
    )

    stages = [gs_stage, fcount_stage, psi_stage]
    measured = [item for item in stages if item["status"] == "measured"]
    complete = all(item["status"] == "measured" for item in stages)
    local_total = round(sum(float(item.get("local_s") or 0.0) for item in stages), 2) if complete else None
    return {
        "mode": "paper-2party",
        "n_log": 20,
        "n": 2 ** 20,
        "k": 8000,
        "threads": 32,
        "paper_total_s": PAPER_TOTAL_S,
        "paper_psize_test_s": PAPER_PSIZE_TEST_S,
        "local_total_s": local_total,
        "comparable": bool(complete),
        "measured_stages": [item["id"] for item in measured],
        "stages": stages,
        "verdict": (
            "三个阶段均已实测；仍需核对线程数、网络与安全模型后才能与论文横比。"
            if complete else
            "当前未复现完整 2^20 pTPSI；GS / F-count / PSI 分开标注，不能把阶段耗时相加冒充 7.36s。"
        ),
        "doc": os.path.join(ROOT, "PAPER_PATH.md"),
    }
