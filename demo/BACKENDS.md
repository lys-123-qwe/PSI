# 两个真实后端：主备自动切换

目标：你和对方各有一个 Linux 真实后端实例。可以在两台 Linux，也可以在导师同一台 Linux 上各起一个实例（端口隔离）。任何一个没有启动时，另一个仍能提供真实数据模式；全部不可用时，页面自动回落到演示模式，不影响讲解。

## 1. 结构

```text
你的 Windows Demo ──┐
                    ├── 主：实例 A :8787 ── Qin-3 角色 0/1/2（1212/1213）
对方的 Windows Demo ┘   └── 备：实例 B :8788 ── Qin-3 角色 0/1/2（1332/1333）
```

- 可以在两台 Linux，也可以都在导师同一台 Linux：每个实例使用不同的 `PORT` 和 `MYPSI_PORT_BASE` 即可，不会抢端口。
- 同一个实例内一次只跑一个真实任务（服务端串行锁）；不同实例可以并发。
- Web 端口对 Windows 开放（如 8787/8788）；协议端口只在本机使用，不要对公网开放。

## 2. 启动第二个实例（同一台 Linux 或第二台 Linux）

### 同一台导师 Linux 上起两个实例（推荐）

两个实例共用同一份代码、同一个二进制和同一套动态库，只是 Web 端口和协议基址不同：

```bash
cd ~/psi-demo/demo

# 实例 A：Web 8787，协议 1212/1213
MYPSI_BIN=/home/lys/psi-demo/myPSI-frontend-patched \
MYPSI_PATCHED=1 \
MYPSI_LD_LIBRARY_PATH=/home/lys/psi-demo/relic/build-linux/lib \
HOST=0.0.0.0 PORT=8787 MYPSI_PORT_BASE=1200 \
bash start.sh

# 实例 B：Web 8788，协议 1332/1333
MYPSI_BIN=/home/lys/psi-demo/myPSI-frontend-patched \
MYPSI_PATCHED=1 \
MYPSI_LD_LIBRARY_PATH=/home/lys/psi-demo/relic/build-linux/lib \
HOST=0.0.0.0 PORT=8788 MYPSI_PORT_BASE=1320 \
bash start.sh
```

不显式设置 `MYPSI_PORT_BASE` 时，`start.sh` 会根据 `PORT` 自动派生一个基址；显式写死更直观，也方便排错。启动后 `/api/health` 的 `backend.port_base` / `backend.ports` 会显示实际值。

验证两组端口同时在听：

```bash
ss -ltnp | grep -E ':(8787|8788|1212|1213|1332|1333)\b'
```

启动后先看到的是 Web 端口 `8787/8788`；协议端口 `1212/1213`、`1332/1333` 只在真实任务运行期间出现。跑一次真实任务时再执行上面的 `ss`，预期同时看到两组。如果运行中只看到一组，说明第二个实例没有拿到不同的 `MYPSI_PORT_BASE`，或者还在用旧二进制（旧版写死 1212/1213）。

服务端还会为每个 `MYPSI_PORT_BASE` 加独占锁：如果第二个实例误用了同一个基址，它会**在启动时直接报错并退出**，不会等到真实任务跑到一半才抢端口。报错示例：

```text
ERROR: 协议端口基址 1200（1212/1213）已被另一个 Demo 实例占用（PID 12345）。请给这个实例换一个 MYPSI_PORT_BASE，例如 1320。
```

### 用现成后端包（第二台 Linux 或独立目录）

项目负责人会生成：

```text
outputs/PSI-Backend-Linux-x86_64-*.tar.gz
```

把它拷到第二台 Linux：

```bash
tar -xzf PSI-Backend-Linux-x86_64-*.tar.gz
cd PSI-Backend
./start-backend.sh
```

默认监听 `0.0.0.0:8787`。脚本会自动找到包内的二进制和共享库。

### 从源码/已有二进制启动

```bash
cd demo
MYPSI_BIN=/path/to/myPSI-frontend-patched \
MYPSI_PATCHED=1 \
MYPSI_LD_LIBRARY_PATH=/path/to/relic/build-linux/lib \
HOST=0.0.0.0 PORT=8787 \
bash start.sh
```

或者直接：

```bash
cd demo
./start-backend.sh
```

前提是 `demo/bin/myPSI-frontend-patched` 存在，或者设置了 `MYPSI_BIN`。

## 3. 验证第二个实例

在第二个实例所在机器（或同一台 Linux）本机：

```bash
python3 - <<'PY'
import json, urllib.request
h = json.load(urllib.request.urlopen('http://127.0.0.1:8788/api/health'))
print(json.dumps(h['backend'], ensure_ascii=False, indent=2))
PY
```

必须看到：

```json
{"exists": true, "mode": "local", "patched": true}
```

从你的 Windows 测试：

```powershell
Invoke-RestMethod http://第二个实例IP:8788/api/health | ConvertTo-Json -Depth 5
```

如果 8788 不通：

- 确认启动时是 `HOST=0.0.0.0`，不是 `127.0.0.1`；
- 云服务器安全组/防火墙放行对应 Web 端口；
- 只放行你们两人的来源 IP，不要对全网开放。

## 4. 两边 Windows 各写自己的顺序

每台 Windows 的 `demo/remote-backend.txt` 都是“自己优先”：

你的电脑：

```text
http://你的后端IP:8787
http://对方的后端IP:8788
```

对方的电脑：

```text
http://对方的后端IP:8788
http://你的后端IP:8787
```

也可以用脚本一次写两个地址：

```powershell
cd PSI-Demo
.\connect-server.cmd -Primary http://你的后端IP:8787 -Backup http://对方的后端IP:8788
```

交互方式：

```text
connect-server.cmd
主后端地址: http://你的后端IP:8787
备用后端地址（可留空）: http://对方的后端IP:8788
```

### 我的 Linux 关了，自动改用对方的 Linux

只要对方 Linux 上的真实后端已经启动，并且你的 `remote-backend.txt` 里把他排在第二位，你的 Demo 就会在主后端连不上时自动切过去：

1. 对方 Linux 启动后端（独立机器用 8787/1200；和你在同一台导师服务器则用 8788/1320）：

```bash
cd ~/psi-demo/demo
MYPSI_BIN=/path/to/myPSI-frontend-patched \
MYPSI_PATCHED=1 \
MYPSI_LD_LIBRARY_PATH=/path/to/relic/build-linux/lib \
HOST=0.0.0.0 PORT=8788 MYPSI_PORT_BASE=1320 \
bash start.sh
```

2. 在你这台 Windows 上确认能访问对方：

```powershell
Invoke-RestMethod http://对方的IP:8788/api/health | ConvertTo-Json -Depth 5
```

返回里应有 `"exists": true`。如果这里就不通，先解决网络/防火墙，而不是改 Demo。

3. 你的 `demo/remote-backend.txt` 写两行，自己的在前、对方的在后：

```text
http://你的后端IP:8787
http://对方的IP:8788
```

4. 重新打开或刷新 Demo。你的 Linux 关闭后，首页/`/real` 顶部会显示：

```text
主 不可达   备 在线 已选中
```

这时真实数据请求会自动转到对方的 Linux，不需要改代码。

### 对方的 Linux 是 VMware / VirtualBox / WSL 虚拟机

先看虚拟机自己的 IP：

```bash
hostname -I
```

- 桥接模式（Bridged）：通常得到 `192.168.x.x`，Windows 主机可直接用这个地址访问。
- NAT 模式：常见是 `10.0.2.15`，Windows 主机不能直接用这个地址；需要在虚拟机软件里做端口转发：
  - VirtualBox：设置 → 网络 → 高级 → 端口转发 → 新建：主机端口 `8788`，子系统端口 `8788`。
  - VMware：虚拟机设置 → 网络适配器 → NAT 设置 → 端口转发 → 主机 `8788` → 虚拟机 `8788`。
  - WSL2：用 `netsh interface portproxy` 把 Windows 的 8788 转发到 WSL 的 8788，并放行 Windows 防火墙。

然后在 Ubuntu 里放行端口：

```bash
sudo ufw allow 8788/tcp
```

Windows 主机测试：

```powershell
Invoke-RestMethod http://虚拟机IP:8788/api/health
```

如果是 NAT 且已做端口转发，也可以试 `http://127.0.0.1:8788/api/health`。只有这里返回 `"exists": true`，Windows Demo 才能走真实数据模式。

虚拟机里推荐直接用后端包，不要现场编译：

```bash
tar -xzf PSI-Backend-Linux-x86_64-latest.tar.gz
cd PSI-Backend
HOST=0.0.0.0 PORT=8788 ./start-backend.sh
```

如果 `uname -m` 不是 `x86_64`，这个包不能运行，需要在那台机器上按源码重新编译。

## 5. 页面上的状态

- 首页参数区、`/real` 顶部会显示“主 / 备”两个后端状态点；
- `主 在线 已选中`：当前用主后端；
- `主 不可达，备 在线 已选中`：主后端没启动，自动切到备；
- `主/备 都不可达`：页面提示真实后端不可用，演示模式仍可正常使用；
- 点“重新检测”会立即重新探活，不用重启 Demo。

## 6. 实测主备切换

### 场景 1：主后端正常

```powershell
cd PSI-Demo
.\connect-server.cmd -Primary http://你的后端IP:8787 -Backup http://对方的后端IP:8788
```

打开 `http://127.0.0.1:8787`，应显示主后端在线。

### 场景 2：你的 Linux 没启动，对方仍能用

在对方的 Windows 上，配置顺序必须是“对方后端在前”：

```text
http://对方的后端IP:8788
http://你的后端IP:8787
```

你的 Linux 关闭后，对方刷新页面或点“重新检测”，应显示：

```text
主 不可达   备 在线 已选中
```

然后直接跑 `/real` 三方样例，仍会调用对方自己的 Linux 后端。

### 场景 3：两个实例都关闭

页面会提示真实后端不可用，自动回到演示模式；`/real` 的“开始”按钮会禁用，不会假装成功。

### 场景 4：主后端运行中掉线

- 已经发起并正常运行的任务，进度/取消绑定实际后端；
- 如果主后端在 **任务开始前** 就不可达，会自动换下一个；
- 如果请求已经送达、只是响应超时，系统**不会**自动换后端重跑，避免重复计算；页面会显示超时错误，等后端恢复后重试。

## 7. 常驻运行（可选）

每个实例建议用 `systemd` 或 `tmux` 常驻；同一台机器起两个实例时，两个 unit/session 必须使用不同的 `PORT` 和 `MYPSI_PORT_BASE`：

```bash
tmux new -s psi-backend
cd PSI-Backend
./start-backend.sh
# Ctrl-B D 退出，不停止服务
```

或写一个简单的 systemd unit：

```ini
[Unit]
Description=PSI Demo Real Backend
After=network.target

[Service]
Type=simple
User=psi
WorkingDirectory=/opt/PSI-Backend
ExecStart=/opt/PSI-Backend/start-backend.sh
Restart=always

[Install]
WantedBy=multi-user.target
```

## 8. 安全边界

- 8787 没有登录鉴权，适合内部演示；长期对外需要反向代理、鉴权或 IP 白名单。
- 不要把 `remote-backend.txt`、`outputs/`、真实服务器 IP 提交到公开 GitHub。
- 真实数据模式仍然只发 SHA-256 哈希；低熵数据生产环境需要 OPRF/加盐哈希。
- 基因场景只用合成数据；儿童教育场景不接真实数据。

## 9. 生成后端包

在已有二进制的 Linux 上：

```bash
bash demo/scripts/make-backend-bundle.sh \
  --bin /home/lys/psi-demo/myPSI-frontend-patched
```

输出在 `outputs/PSI-Backend-Linux-x86_64-*.tar.gz`。包内包含：

```text
PSI-Backend/
  start-backend.sh
  demo/                 # 最新 Demo 代码
  bin/myPSI-frontend-patched
  lib/                  # libntl/libgmp/libgf2x/librelic
  README-BACKEND.md
  MANIFEST.txt          # sha256、大小、构建机信息
```
