#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"

# Convenience for the lab VM layout: ~/psi-demo/myPSI-frontend + built relic.
if [[ -z "${MYPSI_BIN:-}" && -x "$HOME/psi-demo/myPSI-frontend" ]]; then
  export MYPSI_BIN="$HOME/psi-demo/myPSI-frontend"
  if [[ -d "$HOME/psi-demo/relic/build-linux/lib" ]]; then
    export MYPSI_LD_LIBRARY_PATH="$HOME/psi-demo/relic/build-linux/lib"
  fi
  export MYPSI_TIMEOUT="${MYPSI_TIMEOUT:-180}"
fi

exec python3 server/app.py --host "${HOST:-127.0.0.1}" --port "${PORT:-8787}"
