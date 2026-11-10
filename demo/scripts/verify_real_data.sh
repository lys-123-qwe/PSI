#!/usr/bin/env bash
# End-to-end check for real-data mode (MYPSI_IN / MYPSI_OUT).
#
#   bash demo/scripts/verify_real_data.sh [n_log] [threshold] [common] [a,b,c sizes]
#   bash demo/scripts/verify_real_data.sh 10 500 550 1024,900,800
#
# Exit 0 only when the protocol returns exactly the expected intersection,
# and the reject case (threshold > common) returns ⊥ with no elements.
set -euo pipefail

BIN="${MYPSI_BIN:-$HOME/psi-demo/myPSI-frontend-patched}"
NLOG="${1:-10}"
THRESHOLD="${2:-500}"
COMMON="${3:-550}"
SIZES="${4:-1024,900,800}"
ROOT="${TMPDIR:-/tmp}/psi-file-verify-$$"
CAP=$((1 << NLOG))

[[ -x "$BIN" ]] || { echo "ERROR: MYPSI_BIN not executable: $BIN" >&2; exit 2; }
SORTED=$(python3 - "$SIZES" "$CAP" <<'PY'
import sys
sizes = [int(x) for x in sys.argv[1].split(',')]
cap = int(sys.argv[2])
if len(sizes) != 3 or any(s < 1 for s in sizes):
    raise SystemExit('sizes must be three positive integers')
if max(sizes) > cap:
    raise SystemExit(f'sizes {sizes} exceed 2^{cap.bit_length()-1}={cap}')
print(','.join(str(s) for s in sizes))
PY
)

mkdir -p "$ROOT"
trap 'rm -rf "$ROOT"' EXIT

python3 - "$ROOT" "$COMMON" "$SORTED" <<'PY'
import hashlib, sys
root, common, sizes = sys.argv[1], int(sys.argv[2]), [int(x) for x in sys.argv[3].split(',')]
h = lambda s: hashlib.sha256(s.encode()).hexdigest()[:32]
shared = [h(f'common-{i}') for i in range(common)]
for name, size in zip(('a', 'b', 'c'), sizes):
    lines = shared + [h(f'{name}-{i}') for i in range(size - common)]
    with open(f'{root}/{name}.txt', 'w') as fh:
        fh.write('\n'.join(lines) + '\n')
PY

run_case() {
  local threshold="$1" tag="$2"
  rm -f "$ROOT/out.txt" "$ROOT/p0.log" "$ROOT/p1.log" "$ROOT/p2.log"
  MYPSI_IN="$ROOT/b.txt" "$BIN" 1 "$NLOG" "$threshold" "$threshold" >"$ROOT/p1.log" 2>&1 &
  local p1=$!
  sleep 1.5
  MYPSI_IN="$ROOT/c.txt" "$BIN" 2 "$NLOG" "$threshold" "$threshold" >"$ROOT/p2.log" 2>&1 &
  local p2=$!
  sleep 1.0
  local started=$(date +%s.%N)
  MYPSI_IN="$ROOT/a.txt" MYPSI_OUT="$ROOT/out.txt" timeout 300 "$BIN" 0 "$NLOG" "$threshold" "$threshold" >"$ROOT/p0.log" 2>&1
  local rc=$?
  wait "$p1" "$p2" 2>/dev/null || true
  local ended=$(date +%s.%N)
  local wall=$(python3 -c "print(f'{$ended-$started:.2f}')")
  local count=$(wc -l < "$ROOT/out.txt" 2>/dev/null || echo 0)
  echo "[$tag] threshold=$threshold wall=${wall}s output=$count"
  grep -E "mIntersection's size|Not reach threshold" "$ROOT/p0.log" | tail -1 || true
  return $rc
}

echo "binary=$BIN n_log=$NLOG capacity=$CAP common=$COMMON sizes=$SORTED"
run_case "$THRESHOLD" accept
ACCEPT_COUNT=$?
# Re-count because run_case's return code is the binary exit code.
COUNT=$(wc -l < "$ROOT/out.txt" 2>/dev/null || echo 0)
if [[ "$COUNT" != "$COMMON" ]]; then
  echo "FAIL: expected $COMMON elements, got $COUNT" >&2
  exit 1
fi

REJECT_THRESHOLD=$((COMMON + 1))
if [[ "$REJECT_THRESHOLD" -le "$CAP" ]]; then
  run_case "$REJECT_THRESHOLD" reject
  COUNT2=$(wc -l < "$ROOT/out.txt" 2>/dev/null || echo 0)
  if [[ "$COUNT2" != "0" ]]; then
    echo "FAIL: reject case returned $COUNT2 elements" >&2
    exit 1
  fi
  grep -q "Not reach threshold value" "$ROOT/p0.log" || { echo "FAIL: reject case did not print ⊥" >&2; exit 1; }
fi

echo "PASS: real-data accept/reject verified"