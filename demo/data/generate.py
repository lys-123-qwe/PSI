"""Generate synthetic two-party sets for the PSI demo.

These files are for the UI preview and for the next step (file-input patch).
The current binary still generates its own sets internally; do not present
these CSV files as if the binary consumed them.
"""

from __future__ import annotations

import argparse
import csv
import hashlib
import json
import os
import random


def stable_id(prefix: str, index: int) -> str:
    digest = hashlib.sha256(f"{prefix}:{index}".encode("utf-8")).hexdigest()
    return digest[:32]


def make_sets(scenario: str, n: int, common: int, seed: int) -> tuple[list[str], list[str]]:
    rng = random.Random(seed)
    if scenario == "ad":
        prefix = "user"
        fmt = lambda i: stable_id(prefix, i)
    elif scenario == "ride":
        prefix = "grid"
        fmt = lambda i: f"grid:{31.0 + (i % 500) / 1000:.3f},{121.0 + (i // 500) / 1000:.3f}"
    elif scenario == "gene":
        prefix = "rs"
        fmt = lambda i: f"rs{100000 + i}"
    else:
        raise ValueError(f"unknown scenario: {scenario}")

    shared = [fmt(i) for i in range(common)]
    a_only = [fmt(common + i) for i in range(max(0, n - common))]
    b_only = [fmt(10_000_000 + i) for i in range(max(0, n - common))]
    rng.shuffle(a_only)
    rng.shuffle(b_only)
    return shared + a_only, shared + b_only


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--scenario", choices=["ad", "ride", "gene"], default="ad")
    parser.add_argument("--n", type=int, default=16384)
    parser.add_argument("--common", type=int, default=9800)
    parser.add_argument("--seed", type=int, default=20261105)
    parser.add_argument("--out", default="data/out")
    args = parser.parse_args()

    if args.common > args.n:
        raise SystemExit("--common must be <= --n")
    os.makedirs(args.out, exist_ok=True)
    a, b = make_sets(args.scenario, args.n, args.common, args.seed)
    for name, rows in ((f"{args.scenario}_A.csv", a), (f"{args.scenario}_B.csv", b)):
        with open(os.path.join(args.out, name), "w", newline="", encoding="utf-8") as handle:
            writer = csv.writer(handle)
            writer.writerow(["id"])
            writer.writerows((row,) for row in rows)
    manifest = {
        "scenario": args.scenario,
        "n": args.n,
        "common": args.common,
        "seed": args.seed,
        "note": "synthetic data; not consumed by the current binary",
    }
    with open(os.path.join(args.out, "manifest.json"), "w", encoding="utf-8") as handle:
        json.dump(manifest, handle, ensure_ascii=False, indent=2)
    print(f"wrote {args.out}/{args.scenario}_A.csv and _B.csv (n={args.n}, common={args.common})")


if __name__ == "__main__":
    main()
