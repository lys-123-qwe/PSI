#!/usr/bin/env bash
# One-command verification for the real myPSI-Qin-3 backend.
#
# Usage:
#   MYPSI_BIN=/path/to/myPSI-Qin-3/bin/frontend bash demo/scripts/verify_backend.sh
#   MYPSI_BIN=... MYPSI_PATCHED=1 bash demo/scripts/verify_backend.sh
#
# myPSI-Qin-3 is a 3-party protocol: role 1 listens on 1212, role 2 listens on
# 1213, and role 0 connects to both. Start both senders before the receiver.
# It expects the fixed ports to be free; do not run two demos at once.

set -uo pipefail

BIN="${MYPSI_BIN:-}"
if [[ -z "$BIN" || ! -f "$BIN" ]]; then
  echo "ERROR: set MYPSI_BIN=/path/to/myPSI-Qin-3/bin/frontend" >&2
  exit 2
fi
BIN="$(cd "$(dirname "$BIN")" && pwd)/$(basename "$BIN")"
BIN_DIR="$(dirname "$BIN")"
BIN_NAME="$(basename "$BIN")"

magic="$(head -c 4 "$BIN" | od -An -tx1 | tr -d ' \n')"
if [[ "$magic" != "7f454c46" ]]; then
  echo "WARN: $BIN is not an ELF (magic=$magic). On Windows this is the wrong file." >&2
fi

if command -v ss >/dev/null 2>&1; then
  if ss -ltn 2>/dev/null | grep -qE ':(1212|1213)[[:space:]]'; then
    echo "ERROR: ports 1212/1213 are already in use; stop the old demo/backend first." >&2
    exit 3
  fi
fi

TMP="$(mktemp -d)"
PIDS=()
cleanup() {
  for pid in "${PIDS[@]:-}"; do
    kill "$pid" 2>/dev/null || true
  done
  rm -rf "$TMP"
}
trap cleanup EXIT

run_case() {
  local label="$1" nlog="$2" threshold="$3" common="$4" expect="$5"
  local send1_log="$TMP/send1_${label}.log"
  local send2_log="$TMP/send2_${label}.log"
  local recv_log="$TMP/recv_${label}.log"

  echo ""
  echo "=== ${label}: n=2^${nlog}, k=${threshold}, common=${common}, expect=${expect} ==="

  ( cd "$BIN_DIR" && "./${BIN_NAME}" 1 "$nlog" "$threshold" "$common" >"$send1_log" 2>&1 ) &
  local send1_pid=$!
  PIDS+=("$send1_pid")
  sleep 1.5

  ( cd "$BIN_DIR" && "./${BIN_NAME}" 2 "$nlog" "$threshold" "$common" >"$send2_log" 2>&1 ) &
  local send2_pid=$!
  PIDS+=("$send2_pid")
  sleep 1

  ( cd "$BIN_DIR" && "./${BIN_NAME}" 0 "$nlog" "$threshold" "$common" >"$recv_log" 2>&1 ) &
  local recv_pid=$!
  PIDS+=("$recv_pid")

  local waited=0
  while kill -0 "$recv_pid" 2>/dev/null; do
    sleep 1
    waited=$((waited + 1))
    if (( waited > 180 )); then
      echo "TIMEOUT after 180s; killing all three processes" >&2
      kill "$recv_pid" "$send1_pid" "$send2_pid" 2>/dev/null || true
      return 1
    fi
  done
  wait "$recv_pid" 2>/dev/null || true
  wait "$send1_pid" 2>/dev/null || true
  wait "$send2_pid" 2>/dev/null || true

  echo "--- receiver key lines ---"
  grep -E "离线计算阶段时间|份额收发阶段时间|交集计算阶段时间|总时间|Total Comm|mIntersection's size|Not reach threshold value" "$recv_log" || true

  if [[ "$expect" == "reject" ]]; then
    if grep -q "Not reach threshold value" "$recv_log"; then
      echo "PASS: reject branch observed"
      return 0
    fi
    echo "FAIL: expected 'Not reach threshold value'"
    echo "--- receiver tail ---"
    tail -30 "$recv_log" || true
    return 1
  fi

  if grep -q "mIntersection's size" "$recv_log"; then
    echo "PASS: accept branch observed"
    return 0
  fi
  echo "FAIL: expected 'mIntersection's size'"
  echo "--- receiver tail ---"
  tail -30 "$recv_log" || true
  return 1
}

failures=0

# Baseline: the unpatched binary always treats common == threshold.
run_case "baseline_accept" 10 500 500 accept || failures=$((failures + 1))

if [[ "${MYPSI_PATCHED:-0}" == "1" ]]; then
  run_case "patched_reject" 10 500 450 reject || failures=$((failures + 1))
  run_case "patched_accept" 10 500 550 accept || failures=$((failures + 1))
else
  echo ""
  echo "SKIP: patched 450/550 cases. Re-run with MYPSI_PATCHED=1 after applying the patch and rebuilding."
fi

echo ""
if (( failures == 0 )); then
  echo "ALL CHECKS PASSED"
  exit 0
fi
echo "${failures} CHECK(S) FAILED"
exit 1
