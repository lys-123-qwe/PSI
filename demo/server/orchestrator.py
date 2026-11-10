"""Run the PSI demo in mock mode or against the real Linux binary."""

from __future__ import annotations

import hashlib
import json
import os
import re
import subprocess
import tempfile
import shutil
import threading
import time
import uuid
from typing import Any

from scenarios import SCENARIOS, get_scenario

LIVE_LOCK = threading.Lock()
_JOB_LOCK = threading.Lock()
_JOB: dict[str, Any] = {}


def _truthy(value: str | None) -> bool:
    return str(value).lower() in {"1", "true", "yes", "on"}


MAX_SET_ITEMS = 200
MAX_ITEM_LEN = 64
MAX_FILE_ITEMS = 131072
_RE_HEX32 = re.compile(r"^[0-9a-f]{32}$")


def _normalize_set(raw: Any, label: str) -> list[str]:
    """Validate and de-duplicate a small, manually entered party set."""
    if raw is None:
        return []
    if not isinstance(raw, list):
        raise ValueError(f"{label}必须是字符串数组")
    if len(raw) > MAX_SET_ITEMS:
        raise ValueError(f"{label}最多 {MAX_SET_ITEMS} 条；百万级数据请用自动生成模式")
    out: list[str] = []
    seen: set[str] = set()
    for item in raw:
        if not isinstance(item, str):
            raise ValueError(f"{label}的每个元素都必须是字符串")
        text = item.strip()
        if not text:
            continue
        if len(text) > MAX_ITEM_LEN:
            raise ValueError(f"{label}单个元素最多 {MAX_ITEM_LEN} 个字符")
        if any(ord(ch) < 32 for ch in text):
            raise ValueError(f"{label}元素不能包含控制字符")
        if text not in seen:
            seen.add(text)
            out.append(text)
    return out


def _normalize_hashes(raw: Any, label: str) -> list[str]:
    """Validate browser-side SHA-256 hashes (32 hex chars), de-duplicated."""
    if raw is None:
        return []
    if isinstance(raw, str):
        items: list[Any] = raw.splitlines()
    elif isinstance(raw, list):
        items = raw
    else:
        raise ValueError(f"{label}必须是哈希字符串或哈希数组")
    out: list[str] = []
    seen: set[str] = set()
    for item in items:
        text = str(item).strip().lower()
        if not text:
            continue
        if not _RE_HEX32.match(text):
            raise ValueError(f"{label}存在非 32 位十六进制的哈希值（请用本页面读取文件，不要手工粘贴明文）")
        if text in seen:
            continue
        seen.add(text)
        out.append(text)
    if len(out) > MAX_FILE_ITEMS:
        raise ValueError(f"{label}去重后 {len(out)} 条，超过当前二进制上限 {MAX_FILE_ITEMS} 条/方")
    return out


def backend_status() -> dict[str, Any]:
    binary = os.environ.get("MYPSI_BIN", "")
    exists = bool(binary) and os.path.isfile(binary)
    size = None
    mtime = None
    if exists:
        try:
            stat = os.stat(binary)
            size = stat.st_size
            mtime = time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(stat.st_mtime))
        except OSError:
            pass
    return {
        "mode": "live" if exists else "mock",
        "binary": binary,
        "exists": exists,
        "binary_size": size,
        "binary_mtime": mtime,
        "patched": _truthy(os.environ.get("MYPSI_PATCHED")),
        "platform": os.name,
        "protocol": "myPSI-Qin-3 / 3-party",
        "min_parties": 3,
        "note": (
            "真实后端已配置（三方 Qin-3：角色 0/1/2，端口 1212/1213）" if exists else
            "未配置 MYPSI_BIN；当前只能使用演示模式（Linux 上指向 Qin-3 的 frontend 二进制即可）"
        ),
    }


# --------------------------------------------------------------------------
# live job state (progress + cancel)
# --------------------------------------------------------------------------
def _job_set(**fields: Any) -> None:
    with _JOB_LOCK:
        _JOB.update(fields)


def progress() -> dict[str, Any]:
    """Snapshot of the running live job; polled by the browser."""
    with _JOB_LOCK:
        snapshot = dict(_JOB)
    if not snapshot or not snapshot.get("active"):
        return {"active": False}
    started = snapshot.get("started_at")
    snapshot.pop("procs", None)
    snapshot.pop("cancel_event", None)
    snapshot["active"] = True
    snapshot["elapsed"] = round(time.time() - started, 2) if started else 0.0
    snapshot["percent"] = min(100, max(5, int(snapshot.get("stage", 0)) * 20))
    return snapshot


def cancel() -> dict[str, Any]:
    """Kill the sender/receiver processes; the waiting /api/run then aborts."""
    with _JOB_LOCK:
        if not _JOB.get("active"):
            return {"cancelled": False, "message": "当前没有运行中的任务"}
        _JOB["cancelled"] = True
        _JOB["message"] = "正在终止后端进程…"
        event = _JOB.get("cancel_event")
        procs = list(_JOB.get("procs") or [])
    if event is not None:
        event.set()
    killed = 0
    for proc in procs:
        try:
            if proc.poll() is None:
                proc.kill()
                killed += 1
        except Exception:  # noqa: BLE001 - best effort cleanup
            pass
    return {"cancelled": True, "killed": killed, "message": "已终止后端进程"}


def bench() -> dict[str, Any]:
    """Parsed contents of data/bench.json (produced by bootstrap --bench)."""
    path = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data", "bench.json")
    if not os.path.isfile(path):
        return {"available": False, "cases": [], "path": path}
    try:
        with open(path, "r", encoding="utf-8") as handle:
            payload = json.load(handle)
    except (OSError, json.JSONDecodeError) as exc:
        return {"available": False, "cases": [], "path": path, "error": str(exc)}
    payload["available"] = bool(payload.get("cases"))
    payload["path"] = path
    return payload


# --------------------------------------------------------------------------
# mock mode
# --------------------------------------------------------------------------
def _seed_for(cfg: dict[str, Any]) -> int:
    raw = f"{cfg['scenario']}|{cfg['n_log']}|{cfg['threshold']}|{cfg['common']}|{cfg['parties']}"
    return int(hashlib.sha256(raw.encode("utf-8")).hexdigest()[:12], 16)


def _estimate_times(n: int, threshold: int, parties: int) -> dict[str, float]:
    # Anchored at the CCS'23 table: n=2^20, k=8000 -> 7.36s online, LAN, 32 threads.
    online = 7.36 * (n / 1048576.0) ** 0.75
    online *= 0.90 + 0.10 * min(2.2, max(0.2, threshold / 8000.0))
    online = max(0.12, online)
    offline = max(0.03, 0.59 * (n / 1048576.0) ** 0.80)
    return {
        "offline_s": round(offline, 3),
        "share_s": round(online * 0.30, 3),
        "intersection_s": round(online * 0.65, 3),
        "total_s": round(offline + online, 3),
        "online_s": round(online, 3),
        "comm_mb": round(parties * n * 16 / (1024 * 1024) * 0.9, 2),
    }


def _stages_from_metrics(m: dict[str, float], live: bool) -> list[dict[str, Any]]:
    def ms(x: float) -> int:
        return max(1, int(round(x * 1000)))

    offline = m.get("offline_s", 0.0)
    share = m.get("share_s", 0.0)
    inter = m.get("intersection_s", 0.0)
    return [
        {
            "id": "encode",
            "name": "本地编码 / 哈希",
            "detail": "双方各自把集合元素编码成 128-bit ID，原始元素不出域。",
            "ms": ms(offline * 0.45),
        },
        {
            "id": "oprf",
            "name": "OPRF / 密钥交换",
            "detail": "用不经意伪随机函数对齐元素，对方无法反推你的集合。",
            "ms": ms(offline * 0.55),
        },
        {
            "id": "share",
            "name": "秘密分享 / 集合大小测试",
            "detail": "把交集规模拆成份额，只测试“够不够”，不暴露具体元素。",
            "ms": ms(share),
        },
        {
            "id": "threshold",
            "name": "门限比较",
            "detail": "把测试结果与门限 k 比较：达到才继续，否则输出 ⊥。",
            "ms": ms(inter * 0.55),
        },
        {
            "id": "output",
            "name": "输出交集 / 匹配结果",
            "detail": "达到门限才输出交集/业务匹配；未达到则双方只知道“不够”。",
            "ms": ms(inter * 0.45),
        },
    ]


def run_mock(cfg: dict[str, Any]) -> dict[str, Any]:
    scenario = get_scenario(cfg["scenario"])
    parties = int(cfg["parties"])
    data_mode = cfg.get("data_mode", "generated")
    common_items: list[str] = []
    measured_s = 0.0
    if data_mode == "custom":
        set_a = list(cfg.get("set_a") or [])
        set_b = list(cfg.get("set_b") or [])
        other = set(set_b)
        started = time.perf_counter()
        common_items = [item for item in set_a if item in other]
        measured_s = max(1e-6, time.perf_counter() - started)
        size_a = len(set_a)
        size_b = len(set_b)
        n = max(size_a, size_b, 1)
        n_log = max(0, (n - 1).bit_length())
        threshold = max(0, min(n, int(cfg["threshold"])))
        actual = len(common_items)
    else:
        n_log = int(cfg["n_log"])
        n = 2 ** n_log
        threshold = max(0, min(n, int(cfg["threshold"])))
        actual = max(0, min(n, int(cfg["common"])))
        size_a = n
        size_b = n
    if data_mode == "custom":
        metrics = {
            "offline_s": 0.0,
            "share_s": 0.0,
            "intersection_s": round(measured_s, 6),
            "total_s": round(measured_s, 6),
            "online_s": round(measured_s, 6),
            "comm_mb": 0.0,
        }
        stages = [
            {"id": "encode", "name": "本地读取 / 校验", "detail": "读取 A/B 两方输入并去重，不涉及网络传输。", "ms": 1},
            {"id": "oprf", "name": "集合对齐（本地演示）", "detail": "演示服务在本机按字符串相等对齐元素，不是 OPRF。", "ms": 1},
            {"id": "share", "name": "本地集合求交", "detail": "在本演示服务内计算 A∩B，仅用于展示交集关系。", "ms": max(1, int(round(measured_s * 1000)))},
            {"id": "threshold", "name": "门限比较", "detail": "把实际交集条数与门限 k 比较。", "ms": 1},
            {"id": "output", "name": "输出结果", "detail": "达到门限输出交集；未达到则不输出具体交集。", "ms": 1},
        ]
    else:
        metrics = _estimate_times(n, threshold, parties)
        stages = _stages_from_metrics(metrics, live=False)
    decision = "accept" if actual >= threshold else "reject"
    return {
        "run_id": uuid.uuid4().hex[:12],
        "mode": "mock",
        "scenario": cfg["scenario"],
        "scenario_name": scenario["name"],
        "n": n,
        "n_log": n_log,
        "data_mode": data_mode,
        "size_a": size_a,
        "size_b": size_b,
        "threshold": threshold,
        "actual_intersection": actual,
        "intersection_preview": common_items[:20],
        "intersection_size": actual if decision == "accept" else 0,
        "decision": decision,
        "parties": parties,
        "metrics": {
            **metrics,
            "parties": parties,
            "threads": 1 if data_mode == "custom" else 32,
            "security": "demo-only" if data_mode == "custom" else cfg.get("security", "semi-honest"),
        },
        "stages": stages,
        "source": (
            "本地演示（Python 集合求交，非密码学协议）"
            if data_mode == "custom" else
            "演示模拟（时间锚定 CCS'23 论文表 7.7）"
        ),
        "note": (
            scenario["note"] + " 手动输入模式只用于讲清 A/B 集合关系；真实 C++ PSI 仍需单独接入任意集合。"
            if data_mode == "custom" else
            scenario["note"]
        ),
    }


# --------------------------------------------------------------------------
# live mode
# --------------------------------------------------------------------------
_RE_OFFLINE = re.compile(r"离线计算阶段时间\s*=\s*([\d.]+)s")
_RE_SHARE = re.compile(r"份额收发阶段时间\s*=\s*([\d.]+)s")
_RE_INTER = re.compile(r"交集计算阶段时间\s*=\s*([\d.]+)s")
_RE_TOTAL = re.compile(r"总时间\s*=\s*([\d.]+)s")
_RE_COMM = re.compile(r"Total Comm\s*=\s*([\d.]+)\s*MB")
_RE_SIZE = re.compile(r"mIntersection's size:(\d+)")


def _parse_live_output(text: str) -> dict[str, Any]:
    def grab(regex: re.Pattern[str]) -> float | None:
        found = regex.search(text)
        return float(found.group(1)) if found else None

    size = _RE_SIZE.search(text)
    reject = "Not reach threshold value" in text
    parsed = {
        "offline_s": grab(_RE_OFFLINE),
        "share_s": grab(_RE_SHARE),
        "intersection_s": grab(_RE_INTER),
        "total_s": grab(_RE_TOTAL),
        "comm_mb": grab(_RE_COMM),
        "intersection_size": int(size.group(1)) if size else None,
        "decision": "reject" if reject else ("accept" if size else None),
    }
    if parsed["decision"] is None:
        tail = "\n".join([lin for lin in text.strip().splitlines() if lin.strip()][-8:]) or "（无输出）"
        raise RuntimeError(
            "无法从后端输出中识别结果（既没有 mIntersection's size，也没有 Not reach threshold value）。"
            "后端最后输出：\n" + tail
        )
    return parsed


def _filtered_lines(text: str) -> list[str]:
    keep = ("阶段时间", "总时间", "Total Comm", "mIntersection", "Not reach threshold")
    lines = [lin.strip() for lin in text.splitlines() if any(k in lin for k in keep)]
    return lines[-12:]


def _reader(stream, sink: list[str], role: str) -> None:
    """Drain a process pipe and map the backend's own timings to UI stages."""
    try:
        for raw in stream:
            line = raw.rstrip("\n")
            sink.append(line)
            if role != "recv":
                continue
            match = _RE_OFFLINE.search(line)
            if match:
                _job_set(stage=2, offline_s=float(match.group(1)),
                         message="离线阶段完成（编码 + OPRF），进入份额收发")
                continue
            match = _RE_SHARE.search(line)
            if match:
                _job_set(stage=3, share_s=float(match.group(1)),
                         message="份额收发完成，进入门限比较")
                continue
            match = _RE_INTER.search(line)
            if match:
                _job_set(stage=4, intersection_s=float(match.group(1)),
                         message="交集阶段完成，整理输出")
                continue
            match = _RE_SIZE.search(line)
            if match:
                _job_set(stage=5, intersection_size=int(match.group(1)), decision="accept",
                         message="已得到结果（达到门限）")
                continue
            if "Not reach threshold" in line:
                _job_set(stage=5, decision="reject", message="已得到结果（未达到门限，输出 ⊥）")
    except Exception:  # noqa: BLE001 - pipe closed on kill
        pass


def run_live(cfg: dict[str, Any]) -> dict[str, Any]:
    binary = os.environ.get("MYPSI_BIN", "")
    if not binary or not os.path.isfile(binary):
        raise RuntimeError("未配置 MYPSI_BIN，或二进制不存在。请在 Linux 上设置 MYPSI_BIN=/path/to/frontend")

    if not LIVE_LOCK.acquire(blocking=False):
        raise RuntimeError("已有一个真实后端任务在运行；当前后端使用固定端口 1212/1213，请串行演示")

    wall_started = time.time()

    # myPSI-Qin-3 是三方概率门限 PSI：
    #   角色 0（接收方）作为 client 同时连接 1212 与 1213；
    #   角色 1（发送方 P0）监听 1212，角色 2（发送方 P2）监听 1213。
    # 少起任何一个发送方，接收方都会卡在 client socket connect error。
    parties = 3
    procs: list[subprocess.Popen[str]] = []
    try:
        scenario = get_scenario(cfg["scenario"])
        n_log = int(cfg["n_log"])
        if n_log > 17:
            raise RuntimeError("当前二进制只映射到 2^17；请改用演示模式展示 2^18..2^20 的论文基准")
        threshold = int(cfg["threshold"])
        patched = _truthy(os.environ.get("MYPSI_PATCHED"))
        in_files = cfg.get("in_files") or {}
        out_file = cfg.get("out_file")
        file_mode = bool(in_files)
        common = int(cfg["common"]) if patched else threshold
        if file_mode:
            common = threshold
        if not patched and not file_mode and int(cfg["common"]) != threshold:
            raise RuntimeError("当前二进制把门限同时当作实际交集；要演示“未达到门限”，请先应用 patches/ 下的补丁并设置 MYPSI_PATCHED=1")

        cwd = os.path.dirname(os.path.abspath(binary))
        env = os.environ.copy()
        env.pop("MYPSI_IN", None)
        env.pop("MYPSI_OUT", None)
        if os.environ.get("MYPSI_LD_LIBRARY_PATH"):
            env["LD_LIBRARY_PATH"] = os.environ["MYPSI_LD_LIBRARY_PATH"]

        def spawn(role: int) -> subprocess.Popen[str]:
            proc_env = env
            if file_mode:
                proc_env = env.copy()
                if in_files.get(role):
                    proc_env["MYPSI_IN"] = str(in_files[role])
                if role == 0 and out_file:
                    proc_env["MYPSI_OUT"] = str(out_file)
            proc = subprocess.Popen(
                [binary, str(role), str(n_log), str(threshold), str(common)],
                cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, bufsize=1, env=proc_env,
            )
            procs.append(proc)
            _job_set(procs=list(procs))
            return proc

        def stop_all() -> None:
            for proc in procs:
                if proc.poll() is None:
                    proc.kill()

        run_id = uuid.uuid4().hex[:12]
        cancel_event = threading.Event()
        _job_set(
            active=True,
            run_id=run_id,
            mode="live",
            scenario=cfg["scenario"],
            scenario_name=scenario["name"],
            stage=0,
            message="启动两个发送方（绑定 127.0.0.1:1212/1213）",
            started_at=time.time(),
            cancelled=False,
            procs=[],
            cancel_event=cancel_event,
            n_log=n_log,
            threshold=threshold,
            common=common,
            offline_s=None,
            share_s=None,
            intersection_s=None,
            intersection_size=None,
            decision=None,
        )

        # 两个发送方必须先进入监听，接收方才能同时连上 1212/1213。
        spawn(1)
        time.sleep(1.5)
        if cancel_event.is_set():
            stop_all()
            _job_set(active=False, cancelled=True, stage=5, message="已取消")
            raise RuntimeError("本次运行已被取消")
        spawn(2)
        time.sleep(1.0)
        if cancel_event.is_set():
            stop_all()
            _job_set(active=False, cancelled=True, stage=5, message="已取消")
            raise RuntimeError("本次运行已被取消")
        recv = spawn(0)
        _job_set(procs=list(procs), stage=0, message="三方进程已启动（角色 0/1/2），等待离线阶段…")

        outputs: dict[int, list[str]] = {0: [], 1: [], 2: []}
        threads = [
            threading.Thread(target=_reader, args=(proc.stdout, outputs[role], f"p{role}"), daemon=True)
            for role, proc in zip((1, 2, 0), procs)
        ]
        for thread in threads:
            thread.start()

        timeout = float(os.environ.get("MYPSI_TIMEOUT", "300"))
        deadline = time.time() + timeout
        try:
            while recv.poll() is None:
                if cancel_event.is_set():
                    stop_all()
                    for proc in procs:
                        proc.wait(5)
                    _job_set(active=False, cancelled=True, stage=5, message="已取消")
                    raise RuntimeError("本次运行已被取消")
                if time.time() > deadline:
                    stop_all()
                    raise RuntimeError(f"后端运行超过 {timeout:.0f}s，已终止；请检查端口/进程是否残留")
                time.sleep(0.2)
            for proc in procs:
                try:
                    proc.wait(timeout=10)
                except subprocess.TimeoutExpired:
                    proc.kill()
            if cancel_event.is_set():
                stop_all()
                for proc in procs:
                    proc.wait(5)
                _job_set(active=False, cancelled=True, stage=5, message="已取消")
                raise RuntimeError("本次运行已被取消")
        finally:
            for thread in threads:
                thread.join(timeout=2)

        recv_out = "\n".join(outputs[0])
        send_out = "\n".join(outputs[1] + outputs[2])
        if recv.returncode not in (0, None) and not recv_out:
            raise RuntimeError(f"后端退出码 {recv.returncode}，且没有输出")

        parsed = _parse_live_output(recv_out + "\n" + send_out)
        metrics = {
            "offline_s": parsed["offline_s"] or 0.0,
            "share_s": parsed["share_s"] or 0.0,
            "intersection_s": parsed["intersection_s"] or 0.0,
            "total_s": parsed["total_s"] or 0.0,
            "online_s": round((parsed["total_s"] or 0.0) - (parsed["offline_s"] or 0.0), 3),
            "comm_mb": parsed["comm_mb"] or 0.0,
        }
        _job_set(active=False, cancelled=False, stage=5, message="完成")

        hashes: list[str] = []
        if file_mode and out_file and os.path.isfile(out_file):
            try:
                with open(out_file, "r", encoding="utf-8") as handle:
                    hashes = [line.strip() for line in handle if line.strip()]
            except OSError:
                hashes = []
        sizes = cfg.get("sizes") or {}
        real_size = len(hashes)
        reference = cfg.get("reference")
        return {
            "run_id": run_id,
            "mode": "live",
            "data_mode": "file" if file_mode else "generated",
            "scenario": cfg["scenario"],
            "scenario_name": scenario["name"],
            "n": 2 ** n_log,
            "n_log": n_log,
            "threshold": threshold,
            "size_a": sizes.get("a"),
            "size_b": sizes.get("b"),
            "size_c": sizes.get("c"),
            "actual_intersection": real_size if file_mode else common,
            "reference_intersection": reference,
            "reference_matches_protocol": (
                real_size == reference
                if file_mode and parsed["decision"] == "accept" and reference is not None
                else None
            ),
            "intersection_size": (real_size if parsed["decision"] == "accept" else 0) if file_mode
                                 else (parsed["intersection_size"] or 0),
            "intersection_hashes": hashes,
            "decision": parsed["decision"],
            "parties": parties,
            "metrics": {**metrics, "parties": parties, "threads": 1, "security": "binary default"},
            "stages": _stages_from_metrics(metrics, live=True),
            "source": ("真实三方文件 + Qin-3 协议（浏览器本地哈希，服务端只收到哈希）"
                       if file_mode else "真实后端 stdout 解析（未展示份额/密钥）"),
            "note": scenario["note"] + (
                f" 本次为真实文件：A {sizes.get('a', '?')} / B {sizes.get('b', '?')} / C {sizes.get('c', '?')} 条，"
                f"容量 2^{n_log} = {2 ** n_log:,}；交集由协议真实计算。"
                if file_mode else
                f" 本次为真实后端实测 2^{n_log} = {2 ** n_log:,} 条；更大规模为论文口径。"),
            "key_lines": _filtered_lines(recv_out),
            "protocol": "myPSI-Qin-3 三方概率门限 PSI（角色 0/1/2）",
            "wall_s": round(time.time() - wall_started, 2),
        }
    finally:
        with _JOB_LOCK:
            leftovers = list(_JOB.get("procs") or [])
        for proc in leftovers:
            try:
                if proc.poll() is None:
                    proc.kill()
            except Exception:  # noqa: BLE001 - best effort cleanup
                pass
        _job_set(procs=[], active=False)
        LIVE_LOCK.release()


def _cfg_int(cfg: dict[str, Any], key: str, low: int, high: int, default: int) -> int:
    raw = cfg.get(key, default)
    if isinstance(raw, bool) or not isinstance(raw, (int, float, str)):
        raise ValueError(f"参数 {key} 必须是整数")
    try:
        value = int(raw)
    except (TypeError, ValueError):
        raise ValueError(f"参数 {key} 必须是整数") from None
    if value < low or value > high:
        raise ValueError(f"参数 {key} 必须在 {low}..{high} 之间，收到 {value}")
    return value


def _validate_file_config(cfg: dict[str, Any], scenario_id: str) -> dict[str, Any]:
    """Real three-party sets: browser sends only 32-hex hashes, never plaintext."""
    set_a = _normalize_hashes(cfg.get("set_a"), "A 方")
    set_b = _normalize_hashes(cfg.get("set_b"), "B 方")
    set_c = _normalize_hashes(cfg.get("set_c"), "C 方")
    if not set_a or not set_b or not set_c:
        raise ValueError("真实文件模式需要 A、B、C 三方都至少有一个非空元素")
    capacity = max(len(set_a), len(set_b), len(set_c))
    n_log = max(8, (capacity - 1).bit_length())
    if n_log > 17:
        raise ValueError(f"单方最多 {MAX_FILE_ITEMS} 条（2^17），当前最大 {capacity} 条，请先抽样")
    n = 2 ** n_log
    threshold = _cfg_int(cfg, "threshold", 1, n, min(100, n))
    return {
        "scenario": scenario_id,
        "n_log": n_log,
        "threshold": threshold,
        "common": threshold,
        "data_mode": "files",
        "set_a": set_a,
        "set_b": set_b,
        "set_c": set_c,
        "parties": 3,
        "mode": "file",
        "security": "semi-honest",
        "sizes": {"a": len(set_a), "b": len(set_b), "c": len(set_c)},
    }


def run_file(cfg: dict[str, Any]) -> dict[str, Any]:
    """Write hash-only temp files, run the real three-party binary, read hashes back."""
    workdir = tempfile.mkdtemp(prefix="psi-file-")
    try:
        # Reference value for teaching only: the orchestrator already holds the
        # three hash lists, so this adds no new leakage. The protocol's own
        # output is still authoritative (and is empty when the threshold fails).
        reference = len(set(cfg["set_a"]) & set(cfg["set_b"]) & set(cfg["set_c"]))
        inputs: dict[int, str] = {}
        for role, key in ((0, "a"), (1, "b"), (2, "c")):
            path = os.path.join(workdir, f"{key}.txt")
            with open(path, "w", encoding="utf-8", newline="\n") as handle:
                handle.write("\n".join(cfg[f"set_{key}"]))
                handle.write("\n")
            inputs[role] = path
        run_cfg = dict(cfg)
        run_cfg["in_files"] = inputs
        run_cfg["out_file"] = os.path.join(workdir, "intersection.txt")
        run_cfg["reference"] = reference
        return run_live(run_cfg)
    finally:
        shutil.rmtree(workdir, ignore_errors=True)


def validate_config(cfg: dict[str, Any]) -> dict[str, Any]:
    """Normalise and validate a /api/run payload before any process starts."""
    if not isinstance(cfg, dict):
        raise ValueError("请求体必须是 JSON 对象")
    scenario_id = str(cfg.get("scenario", "ad"))
    if scenario_id not in SCENARIOS:
        raise ValueError(f"未知场景 {scenario_id}；可选：{', '.join(SCENARIOS)}")
    mode = str(cfg.get("mode", "mock")).lower()
    if mode not in {"mock", "live", "file"}:
        raise ValueError("mode 只能是 mock、live 或 file")
    if mode == "file":
        return _validate_file_config(cfg, scenario_id)
    data_mode = str(cfg.get("data_mode", "generated")).lower()
    if data_mode not in {"generated", "custom"}:
        raise ValueError("data_mode 只能是 generated 或 custom")
    parties = _cfg_int(cfg, "parties", 1, 10, 2)
    if mode == "live":
        # 真实二进制是 Qin-3 三方协议；旧页面即使传 2，也统一按 3 方执行。
        parties = 3
    elif parties != 2:
        raise ValueError("演示模式按两方流程展示；三方协议请切换到真实后端（Qin-3：角色 0/1/2）")
    if data_mode == "custom":
        if mode == "live":
            raise ValueError("手动输入模式当前只支持演示模式；真实后端尚未接入任意集合")
        set_a = _normalize_set(cfg.get("set_a"), "A 方集合")
        set_b = _normalize_set(cfg.get("set_b"), "B 方集合")
        if not set_a or not set_b:
            raise ValueError("手动输入模式需要 A、B 两方都至少有一个非空元素")
        n = max(len(set_a), len(set_b))
        n_log = max(0, (n - 1).bit_length())
        threshold = _cfg_int(cfg, "threshold", 0, n, min(4, n))
        common = len(set(set_a) & set(set_b))
    else:
        n_log = _cfg_int(cfg, "n_log", 10, 20, 20)
        n = 2 ** n_log
        threshold = _cfg_int(cfg, "threshold", 0, n, min(8000, n))
        common = _cfg_int(cfg, "common", 0, n, threshold)
        set_a = []
        set_b = []
    return {
        "scenario": scenario_id,
        "n_log": n_log,
        "threshold": threshold,
        "common": common,
        "data_mode": data_mode,
        "set_a": set_a,
        "set_b": set_b,
        "parties": parties,
        "mode": mode,
        "security": str(cfg.get("security", "semi-honest")),
    }


def run(cfg: dict[str, Any]) -> dict[str, Any]:
    cfg = validate_config(cfg)
    if cfg["mode"] == "file":
        return run_file(cfg)
    if cfg["mode"] == "live":
        return run_live(cfg)
    return run_mock(cfg)
