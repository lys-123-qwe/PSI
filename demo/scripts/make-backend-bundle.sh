#!/usr/bin/env bash
# Build a self-contained Linux real-backend bundle.
#
#   bash demo/scripts/make-backend-bundle.sh
#   bash demo/scripts/make-backend-bundle.sh --bin /path/to/myPSI-frontend-patched --out /tmp/psi-backend.tar.gz
#
# The bundle contains the demo, the patched Qin-3 binary and the small set of
# shared libraries that binary needs.  Copy it to a second Linux machine and run
# ./start-backend.sh there.  It is intentionally not committed to Git.
set -euo pipefail

DEMO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
REPO_DIR="$(cd "$DEMO_DIR/.." && pwd)"
BIN="${MYPSI_BIN:-$DEMO_DIR/bin/myPSI-frontend-patched}"
OUT=""

while [[ $# -gt 0 ]]; do
  case "$1" in
    --bin) BIN="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    -h|--help) sed -n '2,14p' "$0"; exit 0 ;;
    *) echo "unknown arg: $1" >&2; exit 2 ;;
  esac
done

[[ -f "$BIN" ]] || { echo "ERROR: binary not found: $BIN" >&2; exit 1; }
if [[ -z "$OUT" ]]; then
  OUT="$REPO_DIR/outputs/PSI-Backend-Linux-x86_64-$(date +%Y%m%d-%H%M).tar.gz"
fi
mkdir -p "$(dirname "$OUT")"

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
PKG="$STAGE/PSI-Backend"
mkdir -p "$PKG/demo" "$PKG/bin" "$PKG/lib"

tar -C "$DEMO_DIR" \
  --exclude='./remote-backend.txt' \
  --exclude='./bin' \
  --exclude='./server.log' \
  --exclude='./server-*.log' \
  --exclude='./*.log' \
  --exclude='./server.pid' \
  --exclude='./*.pid' \
  --exclude='__pycache__' \
  --exclude='*.pyc' \
  --exclude='*.pyo' \
  -cf - . | tar -C "$PKG/demo" -xf -

cp -f "$BIN" "$PKG/bin/myPSI-frontend-patched"
chmod 755 "$PKG/bin/myPSI-frontend-patched"

for lib in libntl.so.43 libgmp.so.10 libgf2x.so.3; do
  for base in /lib/x86_64-linux-gnu /usr/lib/x86_64-linux-gnu /lib64 /usr/lib64; do
    if [[ -e "$base/$lib" ]]; then
      cp -Lf "$base/$lib" "$PKG/lib/$lib"
      break
    fi
  done
done
for relic in "$DEMO_DIR/relic/build-linux/lib/librelic.so" "$HOME/psi-demo/relic/build-linux/lib/librelic.so"; do
  if [[ -f "$relic" ]]; then
    cp -f "$relic" "$PKG/lib/librelic.so"
    break
  fi
done

cat > "$PKG/start-backend.sh" <<'EOF'
#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
export MYPSI_BIN="${MYPSI_BIN:-$PWD/bin/myPSI-frontend-patched}"
export MYPSI_LD_LIBRARY_PATH="${MYPSI_LD_LIBRARY_PATH:-$PWD/lib}"
export MYPSI_PATCHED="${MYPSI_PATCHED:-1}"
export MYPSI_TIMEOUT="${MYPSI_TIMEOUT:-300}"
export HOST="${HOST:-0.0.0.0}"
export PORT="${PORT:-8787}"
exec bash demo/start-backend.sh
EOF
chmod 755 "$PKG/start-backend.sh"

cat > "$PKG/README-BACKEND.md" <<'EOF'
# PSI 真实后端（Linux x86_64）

这是一份可以直接拷到第二台 Linux 的后端包。它包含 Demo、已打补丁的
Qin-3 二进制，以及运行所需的三个系统共享库。

## 启动

```bash
tar -xzf PSI-Backend-Linux-x86_64-*.tar.gz
cd PSI-Backend
./start-backend.sh
```

默认监听 `0.0.0.0:8787`。检查：

```bash
python3 - <<'PY'
import json, urllib.request
print(json.load(urllib.request.urlopen('http://127.0.0.1:8787/api/health'))['backend'])
PY
```

看到 `"exists": True` 即表示真实后端就绪。

## 端口与并发

- Web/API：8787（对外开放给对方的 Windows Demo）
- Qin-3 内部端口：默认 1212/1213（可用 MYPSI_PORT_BASE 改，只在本机 localhost，不要对外）
- 同一个实例内一次只能跑一个真实任务；同一台机器可起多个实例（PORT 和 MYPSI_PORT_BASE 不同即可并发）

## 两个实例/两台机器怎么配

每台 Windows 的 `demo/remote-backend.txt` 按“自己优先”写两行：

```
http://自己的后端IP:8787
http://对方的后端IP:8788
```

主后端不可达时，本机 Demo 会自动尝试备用；两台都不可用时自动回落到演示模式。

## 注意

- 需要 x86_64 Linux，推荐 Ubuntu 20.04/22.04。
- 对外开放 Web 端口时请限制来源 IP，或加反向代理/鉴权；不要把 1212/1213、1332/1333 暴露到公网。
- 如果二进制缺少系统库，执行 `ldd ./bin/myPSI-frontend-patched` 查看缺失项。
EOF

{
  echo "generated_at=$(date -Iseconds)"
  echo "binary_sha256=$(sha256sum "$BIN" | awk '{print $1}')"
  echo "binary_size=$(stat -c %s "$BIN")"
  echo "host=$(uname -n)"
  echo "kernel=$(uname -r)"
} > "$PKG/MANIFEST.txt"

rm -f "$OUT"
tar -czf "$OUT" -C "$STAGE" PSI-Backend
echo "[bundle] $OUT"
du -h "$OUT"
