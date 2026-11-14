#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(pwd)"

# Optional remote real backend.  The URL lives in the git-ignored
# remote-backend.txt, never in the repository.
if [[ -z "${PSI_REMOTE:-}" && -f remote-backend.txt ]]; then
  PSI_REMOTE="$(sed -n '/^[[:space:]]*[^#[:space:]]/ { s/^[[:space:]]*//; s/[[:space:]]*$//; p; q; }' remote-backend.txt || true)"
  export PSI_REMOTE
fi
if [[ -n "${PSI_REMOTE:-}" && -z "${PSI_PUBLIC_URL:-}" ]]; then
  export PSI_PUBLIC_URL="$PSI_REMOTE"
fi

# Optional local Linux backend.  Set MYPSI_BIN explicitly in normal use; these
# paths are only convenience fallbacks for the original lab VM layout.
if [[ -z "${MYPSI_BIN:-}" ]]; then
  for candidate in \
    "$ROOT/bin/myPSI-frontend-patched" \
    "$ROOT/bin/frontend" \
    "$HOME/psi-demo/myPSI-frontend-patched" \
    "$HOME/psi-demo/myPSI-frontend"; do
    if [[ -x "$candidate" ]]; then
      export MYPSI_BIN="$candidate"
      break
    fi
  done
  if [[ -n "${MYPSI_BIN:-}" ]]; then
    export MYPSI_PATCHED="${MYPSI_PATCHED:-1}"
    export MYPSI_TIMEOUT="${MYPSI_TIMEOUT:-300}"
  fi
fi

if [[ -n "${MYPSI_BIN:-}" && -z "${MYPSI_LD_LIBRARY_PATH:-}" ]]; then
  for library_dir in \
    "$ROOT/relic/build-linux/lib" \
    "$HOME/psi-demo/relic/build-linux/lib"; do
    if [[ -d "$library_dir" ]]; then
      export MYPSI_LD_LIBRARY_PATH="$library_dir"
      break
    fi
  done
fi

PYTHON_BIN="${PYTHON:-}"
if [[ -z "$PYTHON_BIN" ]]; then
  for candidate in python3 python; do
    if command -v "$candidate" >/dev/null 2>&1 \
      && "$candidate" -c 'import sys; raise SystemExit(0 if sys.version_info >= (3, 10) else 1)' >/dev/null 2>&1; then
      PYTHON_BIN="$candidate"
      break
    fi
  done
fi
if [[ -z "$PYTHON_BIN" ]]; then
  echo "ERROR: Python 3.10+ not found. Set PYTHON=/path/to/python3.10+." >&2
  exit 1
fi

exec "$PYTHON_BIN" server/app.py --host "${HOST:-127.0.0.1}" --port "${PORT:-8787}"
