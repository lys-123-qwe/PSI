  #!/usr/bin/env bash
# Prepare the libOTe dependency that myPSI-Qin-3 needs.
#
#   bash demo/scripts/setup_deps_linux.sh --psi /path/to/myPSI-Qin-3
#   bash demo/scripts/setup_deps_linux.sh --root ~/psi --ref v1.6.0
#   bash demo/scripts/setup_deps_linux.sh --libote /path/to/existing/libOTe
#
# Layout produced (myPSI-Qin-3/CMakeLists.txt looks for ../libOTe):
#   $ROOT/libOTe          v1.6.0, built
#   $ROOT/myPSI-Qin-3     symlink to --psi
#
# Behind a slow link set a proxy first, e.g.
#   export https_proxy=http://127.0.0.1:7890 http_proxy=$https_proxy

set -euo pipefail

ROOT="${PSI_ROOT:-$HOME/psi}"
REF="${LIBOTE_REF:-v1.6.0}"
JOBS="${JOBS:-$(nproc 2>/dev/null || echo 4)}"
LIBOTE_SRC="${LIBOTE_SRC:-}"
PSI_SRC=""
REPO="https://github.com/osu-crypto/libOTe.git"
SELF="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/$(basename "${BASH_SOURCE[0]}")"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --root) ROOT="$2"; shift 2 ;;
    --ref) REF="$2"; shift 2 ;;
    --libote) LIBOTE_SRC="$2"; shift 2 ;;
    --psi) PSI_SRC="$2"; shift 2 ;;
    --jobs) JOBS="$2"; shift 2 ;;
    -h|--help) sed -n '2,18p' "$0"; exit 0 ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

say() { printf '\n\033[1m[%s]\033[0m %s\n' "$1" "$2"; }
die() { printf '\nERROR: %s\n' "$1" >&2; exit 1; }

[[ "$(uname -s)" == "Linux" ]] || die "run this on Linux"

# ---------------------------------------------------------------- 1. toolchain
say "1/5" "checking toolchain"
for tool in git cmake g++ ninja python3; do
  command -v "$tool" >/dev/null 2>&1 || die "missing $tool (apt install -y git cmake g++ ninja-build python3)"
done
GXX_MAJOR="$(g++ -dumpversion | cut -d. -f1)"
CMAKE_VER="$(cmake --version | head -1 | awk '{print $3}')"
echo "  g++     $GXX_MAJOR"
echo "  cmake   $CMAKE_VER"
echo "  ninja   $(ninja --version)"
if (( GXX_MAJOR < 11 )); then
  die "g++ >= 11 required; libOTe v1.6.0 uses C++17 features (apt install g++-11)"
fi
if (( GXX_MAJOR >= 14 )); then
  echo "  warning g++ $GXX_MAJOR is newer than the code expects; g++-11 is the safe choice"
fi

# ---------------------------------------------------------------- 2. source
mkdir -p "$ROOT"
say "2/5" "libOTe source under $ROOT/libOTe"
if [[ -n "$LIBOTE_SRC" ]]; then
  [[ -f "$LIBOTE_SRC/CMakeLists.txt" ]] || die "$LIBOTE_SRC is not a libOTe checkout"
  LIBOTE_DIR="$LIBOTE_SRC"
  echo "  using existing tree: $LIBOTE_DIR"
elif [[ -f "$ROOT/libOTe/CMakeLists.txt" ]]; then
  LIBOTE_DIR="$ROOT/libOTe"
  echo "  reusing $LIBOTE_DIR"
else
  git clone --recursive "$REPO" "$ROOT/libOTe"
  git -C "$ROOT/libOTe" checkout "$REF"
  git -C "$ROOT/libOTe" submodule update --init --recursive
  LIBOTE_DIR="$ROOT/libOTe"
fi
VER="$(grep -m1 -o 'VERSION [0-9.]*' "$LIBOTE_DIR/CMakeLists.txt" | awk '{print $2}' || true)"
echo "  version: ${VER:-unknown}   (expected 1.6.0)"
[[ -z "$VER" || "$VER" == 1.6.* ]] || echo "  warning: not 1.6.x, the 2023 PSI code may fail to compile"

# ---------------------------------------------------------------- 3. build
say "3/5" "building libOTe (-j$JOBS)"
BUILD="$LIBOTE_DIR/out/build/linux"
cmake -S "$LIBOTE_DIR" -B "$BUILD" -GNinja -DCMAKE_BUILD_TYPE=Release -DENABLE_ALL_OT=ON 2>&1 | tail -15
cmake --build "$BUILD" -j "$JOBS" 2>&1 | tail -15

# ---------------------------------------------------------------- 4. verify
say "4/5" "verifying artifacts"
LIBS="$(find "$BUILD" -name 'libcryptoTools.a' -o -name 'liblibOTe.a' 2>/dev/null | head -5)"
if [[ -z "$LIBS" ]]; then
  echo "  no static libs found; listing what was built:"
  find "$BUILD" -name '*.a' 2>/dev/null | head -5
  die "libOTe build produced no libraries; check the cmake output above (boost?)"
fi
echo "$LIBS" | sed 's/^/  /'

# ---------------------------------------------------------------- 5. layout
say "5/5" "placing myPSI-Qin-3 next to libOTe"
if [[ -n "$PSI_SRC" ]]; then
  PSI_SRC="$(cd "$PSI_SRC" && pwd)"
  if [[ -e "$ROOT/myPSI-Qin-3" && ! -L "$ROOT/myPSI-Qin-3" ]]; then
    echo "  $ROOT/myPSI-Qin-3 already exists (real dir), left untouched"
  else
    ln -sfn "$PSI_SRC" "$ROOT/myPSI-Qin-3"
    echo "  $ROOT/myPSI-Qin-3 -> $PSI_SRC"
  fi
else
  echo "  --psi not given; put the source at $ROOT/myPSI-Qin-3 manually"
fi

cat <<EOF

Done. Next:

  bash demo/scripts/bootstrap_linux.sh --src $ROOT/myPSI-Qin-3 --bench

If the link step complains about boost, install it and rebuild:

  sudo apt install -y libboost-all-dev
  rm -rf "$BUILD" && bash "$SELF" --root "$ROOT"
EOF