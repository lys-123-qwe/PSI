#!/usr/bin/env bash
# Rebuild the patched myPSI-Qin-3 frontend on a Linux box.
#
# Why this script exists: the 2023 code needs an old-ish libOTe with IKNP +
# SimplestOT, while the VM only ships MP-SPDZ's trimmed libOTe (IKNP off).
# We rebuild libOTe with IKNP/SimplestOT/libsodium, then link myPSI against it.
#
#   bash demo/scripts/build_patched_linux.sh \
#     --src ~/psi-demo/mypsi-src \
#     --libote-src ~/TPSST/MP-SPDZ/deps/libOTe
#
# Output: <src>/bin/frontend.exe
#   patched CLI: frontend <role> <n_log> <threshold> [common]
#   real data:   MYPSI_IN=<file> (32-hex per line) + MYPSI_OUT=<file> for role 0
#
# Verified on 59.70.82.96 (Ubuntu 20.04, gcc-11, cmake 3.22):
#   2^10 k=500 common=450 -> "Not reach threshold value"   (~1.1s internal)
#   2^10 k=500 common=550 -> "mIntersection's size:550"    (~1.7s internal)

set -euo pipefail

DEMO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SRC="${MYPSI_SRC:-$HOME/psi-demo/mypsi-src}"
LIBOTE_SRC="${LIBOTE_SRC:-$HOME/TPSST/MP-SPDZ/deps/libOTe}"
LIBOTE_PREFIX="${LIBOTE_PREFIX:-$HOME/psi-demo/libote-install}"
BUILD_DIR="${MYPSI_BUILD_DIR:-${SRC}-build}"
JOBS="${JOBS:-$(nproc)}"
CMAKE="${CMAKE:-cmake}"
BOOST_PREFIX="${BOOST_PREFIX:-$HOME/psi-demo/boost-prefix}"
SODIUM_DIR="${SODIUM_DIR:-$HOME/PTPSI/out/install/linux}"
CXX="${CXX:-g++-11}"
CC="${CC:-gcc-11}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --src) SRC="$2"; shift 2 ;;
    --libote-src) LIBOTE_SRC="$2"; shift 2 ;;
    --libote-prefix) LIBOTE_PREFIX="$2"; shift 2 ;;
    --build-dir) BUILD_DIR="$2"; shift 2 ;;
    --jobs) JOBS="$2"; shift 2 ;;
    -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

say() { printf '\n\033[1m[%s]\033[0m %s\n' "$1" "$2"; }
die() { printf '\nERROR: %s\n' "$1" >&2; exit 1; }

[[ "$(uname -s)" == "Linux" ]] || die "run this on Linux"
[[ -f "$SRC/CMakeLists.txt" ]] || die "no myPSI source at $SRC"
[[ -d "$LIBOTE_SRC/cryptoTools" ]] || die "no libOTe source at $LIBOTE_SRC"
[[ -f "$SODIUM_DIR/lib/libsodium.a" ]] || die "no libsodium.a under $SODIUM_DIR/lib"

# ---------------------------------------------------------------- 1. patch check
if ! grep -q 'argc > 4' "$SRC/frontend/main.cpp" 2>/dev/null; then
  say "1/5" "applying patches/0001-decouple-common-from-threshold.patch"
  ( cd "$SRC" && patch -p1 < "$DEMO_DIR/patches/0001-decouple-common-from-threshold.patch" )
else
  say "1/5" "myPSI source already patched (frontend <role> <n_log> <k> [common])"
fi

# 0002: real-data input (MYPSI_IN) + intersection output (MYPSI_OUT).
if ! grep -q 'MYPSI_IN' "$SRC/frontend/TpsiMain.cpp" 2>/dev/null; then
  say "1/5" "applying patches/0002-real-data-input.patch"
  ( cd "$SRC" && patch -p1 < "$DEMO_DIR/patches/0002-real-data-input.patch" )
else
  say "1/5" "real-data input already patched (MYPSI_IN / MYPSI_OUT)"
fi

# Upstream CMakeLists hardcodes the author's paths (/usr/local/lib/libgmp.a,
# ../libOTe, boost under libOTe/cryptoTools/thirdparty). Swap in the portable
# versions kept next to this script; originals are backed up once.
if ! grep -q 'MYPSI_FORCE_INCLUDE' "$SRC/CMakeLists.txt" 2>/dev/null; then
  say "1/5" "installing portable CMakeLists (backups: *.orig)"
  [[ -f "$SRC/CMakeLists.txt.orig" ]] || cp "$SRC/CMakeLists.txt" "$SRC/CMakeLists.txt.orig"
  [[ -f "$SRC/libPSI/CMakeLists.txt.orig" ]] || cp "$SRC/libPSI/CMakeLists.txt" "$SRC/libPSI/CMakeLists.txt.orig"
  cp "$DEMO_DIR/patches/build/CMakeLists.txt" "$SRC/CMakeLists.txt"
  cp "$DEMO_DIR/patches/build/libPSI-CMakeLists.txt" "$SRC/libPSI/CMakeLists.txt"
fi

# ---------------------------------------------------------------- 2. build libOTe
say "2/5" "building libOTe (IKNP + SimplestOT + libsodium) -> $LIBOTE_PREFIX"
LIBOTE_SRC_ABS="$(cd "$LIBOTE_SRC" && pwd)"

# Shutdown hang: Acceptor::stop() waits 4s per pending-socket poll and can loop
# forever when the peer never finishes the handshake. Make it non-blocking.
python3 - "$LIBOTE_SRC_ABS/cryptoTools/cryptoTools/Network/IOService.cpp" <<'PY'
import sys
p = sys.argv[1]
s = open(p).read()
if "Non-blocking shutdown" in s:
    print("  IOService.cpp already patched")
    raise SystemExit(0)
start = s.find("            // wait for the pending events.")
if start < 0:
    print("  WARN: shutdown block not found; leaving IOService.cpp untouched")
    raise SystemExit(0)
end = s.find("mStoppedFuture.get();", start)
if end < 0:
    print("  WARN: shutdown tail not found; leaving IOService.cpp untouched")
    raise SystemExit(0)
end = s.find("\n", end) + 1
new = """            // Non-blocking shutdown: only wait while there is something to wait for.
            using namespace std::chrono;
            auto status = mPendingSockets.empty()
                ? std::future_status::ready
                : mPendingSocketsEmptyFuture.wait_for(milliseconds(200));
            while (status == std::future_status::timeout && !mPendingSockets.empty())
            {
                status = mPendingSocketsEmptyFuture.wait_for(milliseconds(200));
            }

            auto stopped = hasSubscriptions()
                ? mStoppedFuture.wait_for(milliseconds(200))
                : std::future_status::ready;
            while (stopped == std::future_status::timeout && hasSubscriptions())
            {
                stopped = mStoppedFuture.wait_for(milliseconds(200));
            }

            try {
                if (mPendingSocketsEmptyFuture.wait_for(milliseconds(0)) == std::future_status::ready)
                    mPendingSocketsEmptyFuture.get();
            } catch (...) {}
            try {
                if (mStoppedFuture.wait_for(milliseconds(0)) == std::future_status::ready)
                    mStoppedFuture.get();
            } catch (...) {}
"""
open(p, "w").write(s[:start] + new + s[end:])
print("  patched IOService.cpp shutdown")
PY

if [[ ! -f "$LIBOTE_PREFIX/lib/liblibOTe.a" ]]; then
  "$CMAKE" -S "$LIBOTE_SRC_ABS" -B "$LIBOTE_SRC_ABS/out/build" \
    -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER="$CXX" -DCMAKE_C_COMPILER="$CC" \
    -DCMAKE_INSTALL_PREFIX="$LIBOTE_PREFIX" \
    -DENABLE_SIMPLESTOT=ON -DENABLE_IKNP=ON -DENABLE_KOS=ON \
    -DENABLE_SILENTOT=ON -DENABLE_SOFTSPOKEN_OT=ON \
    -DENABLE_SODIUM=ON -DENABLE_RELIC=OFF \
    -DOC_THIRDPARTY_HINT="$SODIUM_DIR" >/dev/null
  "$CMAKE" --build "$LIBOTE_SRC_ABS/out/build" -j "$JOBS" --target install >/dev/null
  say "2/5" "libOTe installed"
else
  say "2/5" "libOTe already installed at $LIBOTE_PREFIX"
fi

# ---------------------------------------------------------------- 3. link tree
say "3/5" "wiring libOTe headers/libs into $SRC/../libOTe"
COMPAT_ROOT="$(cd "$SRC/.." && pwd)"
rm -rf "$COMPAT_ROOT/libOTe"
mkdir -p "$COMPAT_ROOT/libOTe/lib"
for d in "$LIBOTE_PREFIX"/include/*; do
  ln -sfn "$d" "$COMPAT_ROOT/libOTe/$(basename "$d")"
done
ln -sf "$LIBOTE_PREFIX/lib/liblibOTe.a" "$COMPAT_ROOT/libOTe/lib/"
ln -sf "$LIBOTE_PREFIX/lib/libcryptoTools.a" "$COMPAT_ROOT/libOTe/lib/"

# boost prefix: include/ + lib/ (FindBoost wants that layout)
if [[ ! -d "$BOOST_PREFIX/include/boost" ]]; then
  say "3/5" "creating boost prefix at $BOOST_PREFIX (set BOOST_PREFIX to override)"
  mkdir -p "$BOOST_PREFIX"
  ln -sfn "${BOOST_SRC:?set BOOST_SRC=/path/to/boost}" "$BOOST_PREFIX/include"
  mkdir -p "$BOOST_PREFIX/lib"
  for f in "${BOOST_LIB_DIR:?set BOOST_LIB_DIR=/path/to/static/boost/libs}"/libboost_{system,thread,atomic}.a; do
    ln -sf "$f" "$BOOST_PREFIX/lib/"
  done
fi

# libOTe 2021 block is a class; the 2023 code relies on `block + u64`.
mkdir -p "$COMPAT_ROOT/compat"
cat > "$COMPAT_ROOT/compat/block_u64.h" <<'EOF'
#pragma once
#include "cryptoTools/Common/block.h"
#include <cstdint>
namespace osuCrypto {
inline block operator+(block a, std::uint64_t b) { return a + toBlock(b); }
}
EOF

# ---------------------------------------------------------------- 4. build myPSI
say "4/5" "building patched frontend into $BUILD_DIR"
"$CMAKE" -S "$SRC" -B "$BUILD_DIR" \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$CC" -DCMAKE_CXX_COMPILER="$CXX" \
  -DMYPSI_CXX_STANDARD=17 \
  -DMYPSI_FORCE_INCLUDE="$COMPAT_ROOT/compat/block_u64.h" \
  -DBOOST_ROOT="$BOOST_PREFIX" -DBoost_NO_BOOST_CMAKE=ON \
  -DBoost_LIBRARY_DIR="$BOOST_PREFIX/lib" \
  -DCMAKE_CXX_STANDARD_LIBRARIES="$SODIUM_DIR/lib/libsodium.a -lpthread -ldl" \
  -DENABLE_SIMPLESTOT=OFF -DENABLE_MR_KYBER=OFF -DENABLE_MIRACL=OFF -DENABLE_RELIC=OFF \
  >/dev/null
"$CMAKE" --build "$BUILD_DIR" -j "$JOBS" 2>&1 | tail -3

BIN="$SRC/bin/frontend.exe"
[[ -x "$BIN" ]] || die "build finished but $BIN is missing"
say "4/5" "built $BIN ($(stat -c%s "$BIN" 2>/dev/null || echo '?') bytes)"

# ---------------------------------------------------------------- 5. verify
say "5/5" "verifying reject/accept split"
MYPSI_BIN="$BIN" MYPSI_PATCHED=1 bash "$DEMO_DIR/scripts/verify_backend.sh"

cat <<EOF

Patched binary is ready:
  export MYPSI_BIN=$BIN
  export MYPSI_PATCHED=1

Run a live case:
  python3 demo/server/app.py --host 0.0.0.0 --port 8787
  curl -s -X POST http://127.0.0.1:8787/api/run \\
    -d '{"mode":"live","n_log":10,"threshold":500,"common":450,"parties":3,"scenario":"ad"}'
EOF
