#!/usr/bin/env python3
"""Generate deterministic, synthetic A/B/C samples for the /real demo."""

from __future__ import annotations

import argparse
from pathlib import Path


def ad_elements(common: int, size_a: int, size_b: int, size_c: int) -> dict[str, list[str]]:
    shared = [f"uid_{index:06x}" for index in range(1, common + 1)]
    return {
        "A": shared + [f"uid_{0xA00000 + index:06x}" for index in range(1, size_a - common + 1)],
        "B": shared + [f"uid_{0xB00000 + index:06x}" for index in range(1, size_b - common + 1)],
        "C": shared + [f"uid_{0xC00000 + index:06x}" for index in range(1, size_c - common + 1)],
    }


def ride_elements(common: int, size_a: int, size_b: int, size_c: int) -> dict[str, list[str]]:
    def grid(lat_base: float, lon_base: float, count: int) -> list[str]:
        return [
            f"geo_{lat_base + (index // 20) * 0.001:.3f}_{lon_base + (index % 20) * 0.001:.3f}"
            for index in range(count)
        ]

    shared = grid(31.230, 121.470, common)
    return {
        "A": shared + grid(31.300, 121.470, size_a - common),
        "B": shared + grid(31.400, 121.470, size_b - common),
        "C": shared + grid(31.500, 121.470, size_c - common),
    }


def gene_elements(common: int, size_a: int, size_b: int, size_c: int) -> dict[str, list[str]]:
    shared = [f"locus_rs{100000 + index:06d}" for index in range(1, common + 1)]
    return {
        "A": shared + [f"locus_rs{200000 + index:06d}" for index in range(1, size_a - common + 1)],
        "B": shared + [f"locus_rs{300000 + index:06d}" for index in range(1, size_b - common + 1)],
        "C": shared + [f"locus_rs{400000 + index:06d}" for index in range(1, size_c - common + 1)],
    }


def write_samples(out_dir: Path, scenario: str, sets: dict[str, list[str]], common: int) -> None:
    out_dir.mkdir(parents=True, exist_ok=True)
    assert len(set(sets["A"]) & set(sets["B"]) & set(sets["C"])) == common
    for role in ("A", "B", "C"):
        values = sets[role]
        assert len(values) == len(set(values)), f"{scenario}_{role} contains duplicates"
        path = out_dir / f"{scenario}_{role}.txt"
        path.write_text("\n".join(values) + "\n", encoding="utf-8")
        print(f"{path}: {len(values)} lines")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--out", default="demo/web/samples")
    parser.add_argument("--common", type=int, default=300)
    parser.add_argument("--size-a", type=int, default=1200)
    parser.add_argument("--size-b", type=int, default=1000)
    parser.add_argument("--size-c", type=int, default=800)
    args = parser.parse_args()

    sizes = (args.size_a, args.size_b, args.size_c)
    if args.common <= 0 or any(size < args.common for size in sizes):
        parser.error("每方规模必须不小于 common，且 common 必须为正数")

    out_dir = Path(args.out).resolve()
    generators = {
        "ad": ad_elements,
        "ride": ride_elements,
        "gene": gene_elements,
    }
    for scenario, generator in generators.items():
        write_samples(out_dir, scenario, generator(args.common, *sizes), args.common)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
