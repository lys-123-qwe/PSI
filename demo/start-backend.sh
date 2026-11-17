#!/usr/bin/env bash
# Start this demo as a real Linux backend for other machines.
#
#   ./start-backend.sh
#   HOST=0.0.0.0 PORT=8787 ./start-backend.sh
#
# It is a thin wrapper around start.sh: it finds the patched Qin-3 binary and
# the bundled shared libraries, then binds the demo server to 0.0.0.0 so a
# Windows demo (or a colleague's computer) can use it through PSI_REMOTE.
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(pwd)"

if [[ -f "$ROOT/backend.env" ]]; then
  set -a
  # shellcheck disable=SC1091
  source "$ROOT/backend.env"
  set +a
fi

if [[ -z "${MYPSI_BIN:-}" ]]; then
  for candidate in \
    "$ROOT/bin/myPSI-frontend-patched" \
    "$ROOT/../bin/myPSI-frontend-patched" \
    "$ROOT/bin/frontend" \
    "$HOME/psi-demo/myPSI-frontend-patched" \
    "$HOME/psi-demo/myPSI-frontend"; do
    if [[ -x "$candidate" ]]; then
      export MYPSI_BIN="$candidate"
      break
    fi
  done
fi

if [[ -z "${MYPSI_BIN:-}" || ! -f "${MYPSI_BIN}" ]]; then
  echo "ERROR: 未找到 myPSI-frontend-patched。请设置 MYPSI_BIN=/path/to/binary。" >&2
  exit 1
fi

if [[ -z "${MYPSI_LD_LIBRARY_PATH:-}" ]]; then
  for candidate in \
    "$ROOT/lib" \
    "$ROOT/../lib" \
    "$ROOT/relic/build-linux/lib" \
    "$HOME/psi-demo/relic/build-linux/lib"; do
    if [[ -d "$candidate" ]]; then
      export MYPSI_LD_LIBRARY_PATH="$candidate"
      break
    fi
  done
fi

export MYPSI_PATCHED="${MYPSI_PATCHED:-1}"
export MYPSI_TIMEOUT="${MYPSI_TIMEOUT:-300}"
export HOST="${HOST:-0.0.0.0}"
export PORT="${PORT:-8787}"

echo "[backend] binary: $MYPSI_BIN"
echo "[backend] library path: ${MYPSI_LD_LIBRARY_PATH:-system default}"
echo "[backend] listening: http://$HOST:$PORT"
echo "[backend] health: http://<this-host>:$PORT/api/health"
exec bash start.sh
