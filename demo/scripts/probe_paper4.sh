#!/usr/bin/env bash
# Probe whether the CCS'23 pTPSI prototype code can be compiled here.
#   bash demo/scripts/probe_paper4.sh
#   LIBOTE_DIR=/path/to/libOTe bash demo/scripts/probe_paper4.sh
# Only runs -fsyntax-only; it does not execute any experiment.

set -uo pipefail

DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../thirdparty/ptpsi-ccs23" && pwd)"
CXX="${CXX:-g++}"
INC=(-I"$DIR")
if [[ -n "${LIBOTE_DIR:-}" ]]; then
  INC+=(-I"$LIBOTE_DIR" -I"$LIBOTE_DIR/cryptoTools")
fi

echo "compiler : $($CXX --version 2>/dev/null | head -1)"
echo "sources  : $DIR"
echo "libOTe   : ${LIBOTE_DIR:-<not set>}"
echo ""

if [[ -f "$DIR/sha1.hpp" ]]; then
  echo "sha1.hpp : present"
else
  echo "sha1.hpp : MISSING (3 files include it; they cannot compile without it)"
fi
echo ""

ok=0; bad=0
while IFS= read -r f; do
  name="$(basename "$f")"
  err="$("$CXX" -std=c++17 -fsyntax-only -fopenmp -w "${INC[@]}" "$f" 2>&1)"
  if [[ $? -eq 0 ]]; then
    printf '  OK    %s\n' "$name"
    ok=$((ok + 1))
  else
    printf '  FAIL  %s\n' "$name"
    printf '%s\n' "$err" | grep -E "fatal error|error:" | head -2 | sed 's/^/        /'
    bad=$((bad + 1))
  fi
done < <(find "$DIR" -maxdepth 1 -name '*.cpp' | sort)

echo ""
echo "OK=$ok  FAIL=$bad"
[[ $bad -eq 0 ]] || echo "Missing headers above are expected: sha1.hpp is not in the repo."