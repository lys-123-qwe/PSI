#!/usr/bin/env python3
"""Run a small, reproducible serial benchmark for the Qin-3 demo backend."""

from __future__ import annotations

import argparse
import json
import os
import platform
import re
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

RE_OFFLINE = re.compile(r"离线计算阶段时间\s*=\s*([\d.]+)s")
RE_SHARE = re.compile(r"份额收发阶段时间\s*=\s*([\d.]+)s")
RE_INTERSECTION = re.compile(r"交集计算阶段时间\s*=\s*([\d.]+)s")
RE_TOTAL = re.compile(r"总时间\s*=\s*([\d.]+)s")
RE_COMM = re.compile(r"Total Comm\s*=\s*([\d.]+)\s*MB")
RE_SIZE = re.compile(r"mIntersection's size:(\d+)")


def parse_sizes(raw: str) -> list[int]:
    values: list[int] = []
    for chunk in raw.replace(" ", "").split(","):
        if not chunk:
            continue
        value = int(chunk)
        if not 8 <= value <= 17:
            raise ValueError(f"benchmark 规模必须在 2^8..2^17，收到 2^{value}")
        if value not in values:
            values.append(value)
    if not values:
        raise ValueError("至少需要一个 benchmark 规模")
    return values


def port_is_busy(port: int) -> bool:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.settimeout(0.2)
        return sock.connect_ex(("127.0.0.1", port)) == 0


def grab(pattern: re.Pattern[str], text: str, cast=float):
    match = pattern.search(text)
    return cast(match.group(1)) if match else None


def run_case(binary: Path, n_log: int, threshold: int, common: int, timeout_s: float) -> dict:
    if port_is_busy(1212) or port_is_busy(1213):
        raise RuntimeError("端口 1212/1213 已被占用；请先停止旧的真实求交任务")

    cwd = binary.parent
    started = time.time()
    sender_logs = [tempfile.TemporaryFile(), tempfile.TemporaryFile()]
    receiver_log = tempfile.TemporaryFile()
    senders: list[subprocess.Popen[bytes]] = []
    receiver: subprocess.Popen[bytes] | None = None
    def spawn_sender(role: int, log) -> None:
        senders.append(subprocess.Popen(
            [str(binary), str(role), str(n_log), str(threshold), str(common)],
            cwd=cwd,
            stdout=log,
            stderr=subprocess.STDOUT,
        ))

    try:
        spawn_sender(1, sender_logs[0])
        time.sleep(1.5)
        spawn_sender(2, sender_logs[1])
        time.sleep(1.0)
        receiver = subprocess.Popen(
            [str(binary), "0", str(n_log), str(threshold), str(common)],
            cwd=cwd,
            stdout=receiver_log,
            stderr=subprocess.STDOUT,
        )
        try:
            return_code = receiver.wait(timeout=timeout_s)
        except subprocess.TimeoutExpired as exc:
            raise RuntimeError(f"2^{n_log} 在 {timeout_s:.0f}s 内未完成；请检查端口/进程") from exc
        wall_s = time.time() - started
    finally:
        for proc in senders:
            if proc.poll() is None:
                proc.kill()
        if receiver is not None and receiver.poll() is None:
            receiver.kill()
        for proc in senders:
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                proc.kill()
        if receiver is not None:
            try:
                receiver.wait(timeout=10)
            except subprocess.TimeoutExpired:
                receiver.kill()

    sender_logs[0].seek(0)
    sender_logs[1].seek(0)
    receiver_log.seek(0)
    sender_text = b"\n".join((sender_logs[0].read(), sender_logs[1].read())).decode("utf-8", "replace")
    receiver_text = receiver_log.read().decode("utf-8", "replace")
    sender_logs[0].close()
    sender_logs[1].close()
    receiver_log.close()
    text = receiver_text + "\n" + sender_text
    size = grab(RE_SIZE, text, int)
    return {
        "n": n_log,
        "k": threshold,
        "common": common,
        "wall_s": round(wall_s, 2),
        "total_s": grab(RE_TOTAL, text),
        "offline_s": grab(RE_OFFLINE, text),
        "share_s": grab(RE_SHARE, text),
        "intersection_s": grab(RE_INTERSECTION, text),
        "comm_mb": grab(RE_COMM, text),
        "intersection": size,
        "accepted": size is not None,
        "returncode": return_code,
    }


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bin", required=True, help="patched Qin-3 frontend ELF")
    parser.add_argument("--out", default="demo/data/bench.json")
    parser.add_argument("--sizes", default="10,11,12", help="comma-separated log2 sizes")
    parser.add_argument("--k", type=int, default=512)
    parser.add_argument("--common", type=int, default=512)
    parser.add_argument("--timeout", type=float, default=300.0, help="seconds per case")
    args = parser.parse_args()

    binary = Path(args.bin).expanduser().resolve()
    if not binary.is_file():
        parser.error(f"binary not found: {binary}")
    if args.timeout <= 0:
        parser.error("--timeout must be positive")
    if args.k <= 0 or args.common < 0:
        parser.error("--k must be positive and --common cannot be negative")

    try:
        sizes = parse_sizes(args.sizes)
    except ValueError as exc:
        parser.error(str(exc))

    cases: list[dict] = []
    print(f"benchmarking {', '.join(f'2^{n}' for n in sizes)}; serial, single run per size", flush=True)
    for n_log in sizes:
        if args.k > 2**n_log or args.common > 2**n_log:
            raise SystemExit(f"k/common 不能超过 2^{n_log}")
        print(f"  running 2^{n_log} …", flush=True)
        case = run_case(binary, n_log, args.k, args.common, args.timeout)
        missing = [key for key in ("total_s", "comm_mb", "intersection") if case[key] is None]
        if missing or case["returncode"] != 0 or not case["accepted"]:
            raise RuntimeError(
                f"2^{n_log} 结果不可用（returncode={case['returncode']}, "
                f"accepted={case['accepted']}, missing={','.join(missing) or 'none'}）"
            )
        cases.append(case)
        print(
            f"  2^{n_log}: wall={case['wall_s']:.2f}s total={case['total_s']} "
            f"comm={case['comm_mb']}MB accepted={case['accepted']}",
            flush=True,
        )

    report = {
        "generated_at": time.strftime("%Y-%m-%dT%H:%M:%S%z"),
        "os": platform.platform(),
        "kernel": platform.release(),
        "patched": os.environ.get("MYPSI_PATCHED", "0").lower() in {"1", "true", "yes", "on"},
        "host": platform.node(),
        "cpu": platform.processor() or platform.machine(),
        "nproc": os.cpu_count(),
        "mem_gb": round(os.sysconf("SC_PAGE_SIZE") * os.sysconf("SC_PHYS_PAGES") / 2**30, 1),
        "binary": str(binary),
        "benchmark": {
            "mode": "serial",
            "runs_per_size": 1,
            "sizes": sizes,
            "k": args.k,
            "common": args.common,
            "timeout_s": args.timeout,
        },
        "cases": cases,
    }
    out = Path(args.out).expanduser().resolve()
    out.parent.mkdir(parents=True, exist_ok=True)
    temp = out.with_suffix(out.suffix + ".tmp")
    with temp.open("w", encoding="utf-8") as handle:
        json.dump(report, handle, ensure_ascii=False, indent=2)
        handle.write("\n")
    os.replace(temp, out)
    print(f"wrote {out}", flush=True)
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError) as exc:
        print(f"BENCH FAILED: {exc}", file=sys.stderr)
        raise SystemExit(1)
