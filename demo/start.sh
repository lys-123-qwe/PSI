#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
ROOT="$(pwd)"

# Optional remote real backends.  The URLs live in the git-ignored
# remote-backend.txt, never in the repository.  One URL per line, priority order.
if [[ -z "${PSI_REMOTE:-}" && -f remote-backend.txt ]]; then
  PSI_REMOTE="$(sed '1s/^\xEF\xBB\xBF//' remote-backend.txt 2>/dev/null \
    | sed 's/#.*//' \
    | sed 's/^[[:space:]]*//; s/[[:space:]]*$//' \
    | sed '/^$/d' \
    | paste -sd ';' - || true)"
  export PSI_REMOTE
fi
if [[ -n "${PSI_REMOTE:-}" && -z "${PSI_PUBLIC_URL:-}" ]]; then
  export PSI_PUBLIC_URL="${PSI_REMOTE%%;*}"
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
      && "$candidate" -c 'import sys; raise SystemExit(0 if sys.version_info >= (3, 8) else 1)' >/dev/null 2>&1; then
      PYTHON_BIN="$candidate"
      break
    fi
  done
fi
if [[ -z "$PYTHON_BIN" ]]; then
  echo "ERROR: 未找到 Python 3.8+。请安装后重试，或设置 PYTHON=/path/to/python3.8+。" >&2
  exit 1
fi

PORT_VALUE="${PORT:-8787}"
if [[ -z "${MYPSI_PORT_BASE:-}" ]]; then
  # Give each web port its own Qin-3 port block so two demo instances can run
  # on the same Linux host without fighting over 1212/1213.
  # 8787 -> 1200 (1212/1213), 8788 -> 1320 (1332/1333).
  if (( PORT_VALUE >= 8787 )); then
    slot=$(( (PORT_VALUE - 8787) % 100 ))
  else
    slot=$(( PORT_VALUE % 100 ))
  fi
  export MYPSI_PORT_BASE="$((1200 + slot * 120))"
fi
echo "[backend] web port: $PORT_VALUE, Qin-3 port base: $MYPSI_PORT_BASE (p0=$((MYPSI_PORT_BASE + 12)), p1=$((MYPSI_PORT_BASE + 13)))"

exec "$PYTHON_BIN" server/app.py --host "${HOST:-127.0.0.1}" --port "$PORT_VALUE"
