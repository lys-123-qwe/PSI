#!/usr/bin/env bash
# One-command bring-up for the real pTPSI backend on any Linux box
# (local VM, teacher's server, cloud host).
#
#   bash demo/scripts/bootstrap_linux.sh --src /path/to/myPSI-Qin-3
#   bash demo/scripts/bootstrap_linux.sh --src ... --bench
#   bash demo/scripts/bootstrap_linux.sh --bin /path/to/bin/frontend
#
# What it does, in order:
#   1. sanity-check the binary is an ELF and its shared libs resolve
#   2. build from source (fresh build dir, Release)
#   3. verify the baseline accept branch (unpatched semantics)
#   4. apply patches/0001-decouple-common-from-threshold.patch and rebuild
#   5. verify the patched reject/accept branches (450 vs 550, k=500)
#   6. optionally benchmark 2^10 / 2^11 / 2^12 and write data/bench.json
#
# Nothing here touches the network, pip, or sudo.

set -euo pipefail

DEMO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="${MYPSI_SRC:-}"
BIN="${MYPSI_BIN:-}"
BUILD_DIR="${MYPSI_BUILD_DIR:-}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
BENCH_SIZES="${MYPSI_BENCH_SIZES:-10,11,12}"
BENCH_K="${MYPSI_BENCH_K:-512}"
BENCH_COMMON="${MYPSI_BENCH_COMMON:-512}"
BENCH_TIMEOUT="${MYPSI_BENCH_TIMEOUT:-300}"
DO_BENCH=0
DO_PATCH=1

while [[ $# -gt 0 ]]; do
  case "$1" in
    --src) SRC="$2"; shift 2 ;;
    --bin) BIN="$2"; shift 2 ;;
    --build-dir) BUILD_DIR="$2"; shift 2 ;;
    --jobs) JOBS="$2"; shift 2 ;;
    --bench) DO_BENCH=1; shift ;;
    --bench-sizes) BENCH_SIZES="$2"; shift 2 ;;
    --bench-k) BENCH_K="$2"; shift 2 ;;
    --bench-common) BENCH_COMMON="$2"; shift 2 ;;
    --bench-timeout) BENCH_TIMEOUT="$2"; shift 2 ;;
    --no-patch) DO_PATCH=0; shift ;;
    -h|--help) sed -n '2,25p' "$0"; exit 0 ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

say() { printf '\n\033[1m[%s]\033[0m %s\n' "$1" "$2"; }
die() { printf '\nERROR: %s\n' "$1" >&2; exit 1; }

[[ "$(uname -s)" == "Linux" ]] || die "run this on Linux (the backend is an ELF binary)"

# ---------------------------------------------------------------- 1. locate
if [[ -z "$BIN" && -n "$SRC" ]]; then
  BIN="$SRC/bin/frontend"
fi

if [[ -n "$BIN" && -f "$BIN" ]]; then
  magic="$(head -c 4 "$BIN" | od -An -tx1 | tr -d ' \n')"
  [[ "$magic" == "7f454c46" ]] || die "$BIN is not an ELF (magic=$magic)"
  say "1/6" "binary: $BIN"
  if ldd "$BIN" 2>&1 | grep -q "not found"; then
    ldd "$BIN" 2>&1 | grep "not found"
    die "missing shared libs above; set MYPSI_LD_LIBRARY_PATH"
  fi
  say "1/6" "shared libs OK"
else
  [[ -n "$SRC" ]] || die "pass --src /path/to/myPSI-Qin-3 (or --bin if already built)"
  [[ -f "$SRC/CMakeLists.txt" ]] || die "no CMakeLists.txt under $SRC"
fi

# ---------------------------------------------------------------- 2. build
if [[ -n "$SRC" ]]; then
  BUILD_DIR="${BUILD_DIR:-$SRC/build-demo}"
  say "2/6" "building into $BUILD_DIR (-j$JOBS)"
  cmake -S "$SRC" -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build "$BUILD_DIR" --target frontend -j "$JOBS" 2>&1 | tail -5
  BIN="$SRC/bin/frontend"
  [[ -x "$BIN" ]] || die "build finished but $BIN is missing"
  export MYPSI_BIN="$BIN"
fi

# ---------------------------------------------------------------- 3. baseline
say "3/6" "baseline check (n=2^10, k=500, common=500)"
MYPSI_BIN="$BIN" bash "$DEMO_DIR/scripts/verify_backend.sh" || die "baseline failed; fix the backend before patching"

# ---------------------------------------------------------------- 4. patch
if [[ "$DO_PATCH" == "1" ]]; then
  PATCH="$DEMO_DIR/patches/0001-decouple-common-from-threshold.patch"
  say "4/6" "applying $PATCH"
  if grep -q 'argc > 4' "$SRC/frontend/main.cpp" 2>/dev/null; then
    echo "already patched, skipping"
  elif git -C "$SRC" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    git -C "$SRC" apply --check "$PATCH" || die "patch does not apply cleanly"
    git -C "$SRC" apply "$PATCH"
  else
    ( cd "$SRC" && patch -p1 --dry-run <"$PATCH" >/dev/null ) || die "patch does not apply cleanly"
    ( cd "$SRC" && patch -p1 <"$PATCH" >/dev/null )
  fi
  say "4/6" "rebuilding patched frontend"
  cmake --build "$BUILD_DIR" --target frontend -j "$JOBS" 2>&1 | tail -5

  say "5/6" "patched check: 450 < 500 must reject, 550 > 500 must accept"
  MYPSI_BIN="$BIN" MYPSI_PATCHED=1 bash "$DEMO_DIR/scripts/verify_backend.sh" \
    || die "patched binary did not produce the reject/accept flip"
else
  say "4/6" "patch skipped (--no-patch)"
  say "5/6" "patched check skipped"
fi

# ---------------------------------------------------------------- 6. bench
if [[ "$DO_BENCH" == "1" ]]; then
  say "6/6" "benchmarking 2^{${BENCH_SIZES}} (single serial run per size)"
  mkdir -p "$DEMO_DIR/data"
  MYPSI_PATCHED="${MYPSI_PATCHED:-1}" python3 "$DEMO_DIR/scripts/bench_linux.py" \
    --bin "$BIN" \
    --out "$DEMO_DIR/data/bench.json" \
    --sizes "$BENCH_SIZES" \
    --k "$BENCH_K" \
    --common "$BENCH_COMMON" \
    --timeout "$BENCH_TIMEOUT"
else
  say "6/6" "benchmark skipped (--bench to run it)"
fi

cat <<EOF

Backend is ready for the demo:
  export MYPSI_BIN=$BIN
  export MYPSI_PATCHED=1
  export MYPSI_TIMEOUT=180
  python3 demo/server/app.py --host 0.0.0.0 --port 8787

Then open http://<this-host>:8787 and switch the parameter page to the real backend.
EOF
