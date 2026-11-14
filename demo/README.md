# PSI Lab Demo

把现有 C/C++ 隐私求交后端包成一个可点击、可讲解、可现场演示的 Web Demo。

## 这套东西是什么

- 5 屏 Web：场景选择 → 参数 → 运行 → 结果 → 性能。
- 两种模式：
  - **演示模式（mock）**：不依赖后端，时间按 CCS '23 论文表 7.7 锚定；适合讲流程、讲门限翻转、做备份视频。
  - **真实后端（live）**：在 Linux 上调用 `myPSI-Qin-3/bin/frontend`，解析真实阶段耗时和 `mIntersection's size`。这份 Qin-3 代码是**三方协议**：角色 1 监听 1212、角色 2 监听 1213、角色 0 同时连接两端，页面会自动切到“3 方”。
- 数据准备有三种方式：
  - **自动生成**：填集合规模 `2^n`、门限 `k`、共同元素数 `c`；用于百万级性能演示，对应现有 C++ 后端的伪随机集合。
  - **手动输入**：A/B 两方各填最多 200 条元素（每行一个，也支持逗号/空格分隔），由 Python 演示后端计算真实交集并做门限判断；用于现场讲清“双方各自有数据 + 交集够不够”。这是本地演示，不是密码学 PSI；真实 C++ 后端仍未接入任意集合/文件输入。
  - **真实文件（三方，`/real`）**：A/B/C 各选一个文本文件，浏览器本地哈希后交给 Qin-3 真实协议；明文不出浏览器。见下面「真实数据」一节。
- 运行页接真实进度：live 模式轮询 `/api/progress`，用后端 stdout 的阶段行驱动五步高亮；进度条、阶段 x/5、离线/份额/交集三段真实耗时都会实时显示。
- 取消按钮会真的 kill 收发进程；失败时错误停在红色卡片，可展开原始错误。
- `/api/run` 先校验参数（场景、`2^10..2^20`、门限范围，演示模式 2 方 / 真实后端 3 方），非法请求直接返回中文错误，不会先起进程再崩。
- 窄屏适配：≤640px 时导航单行、指标单列、按钮自动换行，手机/小屏也能演示。
- 结果页可“导出 JSON / 复制结论”，现场老师说“发我一份”时直接给文件或粘贴。
- 性能页可读本机实测 `data/bench.json`；没有实测数据时明确显示“尚无本机实测”，不拿论文数字冒充。
- 性能页支持“复制实测摘要 / 下载 bench.json”，现场被问“数据发我”时直接给结构化文件。
- 运行失败的错误卡片支持“复制错误”，可把中文原因 + 原始堆栈直接发群里。
- 页脚“部署自检”会显示服务地址、版本、启动时间、前端资源时间，并检查前端文件、Python、真实后端、bench 数据、数据目录和任务状态。
- `/api/version`、`/api/selfcheck` 可在现场用 curl 快速核对；`--host 0.0.0.0` 时启动日志会打印可访问的 LAN 地址。
- 3 个场景：广告转化归因、拼车路线匹配、基因医疗比对。
- 儿童教育卡片只做合规提示，不接真实数据。
- 零第三方依赖：Python 标准库 + 原生 HTML/CSS/JS，离线可跑。

## 最快跑起来（Windows / macOS / Linux，演示模式）

要求 Python 3.10+，不需要安装任何 pip 包。

Linux / macOS：

```bash
cd demo
bash start.sh
```

Windows PowerShell（或双击 `start.cmd`）：

```powershell
cd demo
.\start.ps1
```

浏览器打开 <http://127.0.0.1:8787>。

> GitHub Pages 只能托管静态文件，不能运行本项目的 `/api/*` 和真实协议；请在本机或服务器上启动 Python 服务。

## 一次启动，两种模式

同一个 Python 服务同时提供两个入口，不需要分别启动：

- `/`：首页，右上角可在“演示模式 / 真实后端”之间切换；
- `/real`：三份真实文件的三方门限求交。

启动时会自动选择：本地 `MYPSI_BIN` 或 `PSI_REMOTE` / `remote-backend.txt` 指向的远程真实后端可用 → 首页默认进真实模式；不可用 → 首页默认演示模式。两种模式始终都能在页面上切换。

Windows 本地没有 Linux ELF 时，把 `remote-backend.example.txt` 复制成 `remote-backend.txt` 并写入自己的 Linux 服务地址（或设置 `PSI_REMOTE`），即可把真实协议请求转发到服务器。远程不可达时只影响真实模式，演示模式仍可用，`/real` 顶部会给出可点击的服务器地址。

## 真实数据（三方，`/real`）

打开 `http://<服务器>:8787/real`，或从首页右上角「真实数据（三方文件）→」进入。它与首页由同一个服务进程提供。

- 三方各选一个 `.txt/.csv`，每行一个元素（CSV 请先抽成一列）；A 方是接收方，B/C 是发送方。
- 样例区提供「载入样例并演示通过（k=200）」和「载入样例并演示拒绝（k=400）」：自动下载当前场景 A/B/C = 1,200/1,000/800 行样例（共同 300 条）、设置门限并启动真实三方协议，适合现场一键复现通过/⊥。
- 浏览器本地做 `SHA-256`，取前 16 字节（32 位 hex）后上传；HTTP 请求里没有明文，服务端临时目录也只写哈希（`tempfile.mkdtemp`，跑完即删）。
- 容量按最大集合自动取 `2^N`（N=8..17）；门限 k 由页面输入。交集由协议真实计算，结果页再用哈希映射回浏览器里的原始元素。
- 未达门限时协议输出 ⊥；页面上的「参考交集」是服务端对三份哈希本地比对的讲解值，不是协议输出。
- API：`POST /api/run`，`{"mode":"file","scenario":"ad","threshold":100,"set_a":"<hex\\n...>","set_b":"...","set_c":"..."}`。

限制与口径：

- 单方 ≤ 131,072 条（代码里 `setNum` 只映射到 `2^17`），超了服务端直接拒绝。
- 参考实验机（含约 2.5s 进程启动）：`2^9≈3.5s`、`2^10≈4s`、`2^11≈14s`、`2^12≈55s`、`2^13≈3.5min`；不同机器需重新实测。
- 低熵数据不要直接用裸 SHA-256：手机号/身份证可被字典攻击；生产要换 OPRF/加盐哈希（论文里的 OPRF 阶段）。
- 三方进程都跑在同一台演示服务器上，这是“协议演示”，不是三方各自持机的真实部署。

## 在 IDEA 里运行

直接用 IDEA 打开项目根目录（带 `PSI-Demo.iml` 的目录）。仓库不会提交 `.idea/`：

- Windows：运行配置选 **PSI Demo Server (Windows - Demo)**，或直接执行 `.\demo\start.ps1`。是否使用真实后端由本地 `demo/remote-backend.txt` / `PSI_REMOTE` 决定。
- Linux：选 **PSI Demo Server (Linux - Real Data)**，会监听 `0.0.0.0:8787`。
- 如果 IDEA 没立刻显示配置，执行一次 **File → Reload from Disk**，或重启 IDEA。
- 运行后打开 <http://127.0.0.1:8787>；服务器上则打开 `http://<服务器IP>:8787`。

### 在 IDEA 里接真实后端

Windows 本地 IDEA 不能直接执行服务器上的 Linux ELF。`MYPSI_BIN` 指向 Linux 路径只会在 Linux 上生效；Windows 上要把路径写成本地 `.exe` 也不成立，因为当前后端是 ELF。

替代方案是设置 `PSI_REMOTE`：Windows 服务把 `live` / `file` 请求转发给 Linux 服务器，浏览器仍只访问本地 8787。这样一次启动就能同时演示 mock 和真实数据；远程不可达时自动回退 mock。

推荐用 **IDEA Remote Development / JetBrains Gateway**：

1. IDEA 欢迎页选择 **Remote Development → SSH → New Connection**。
2. 主机填 `<服务器用户>@<服务器IP>`，选择 SSH Key 或密码认证。
3. 打开服务器上的项目目录，例如 `/path/to/psidemo`。
4. 选择运行配置 **PSI Demo Server (Linux - Real Data)**，点绿色三角。
5. 打开 `http://<服务器IP>:8787/real`。

这个 Linux 运行配置已经固定为：

```text
工作目录：<服务器项目>/demo
解释器：  /usr/bin/python3
命令：    python3 server/app.py --host 0.0.0.0 --port 8787
MYPSI_BIN=/path/to/myPSI-frontend-patched
MYPSI_PATCHED=1
MYPSI_TIMEOUT=300
```

如果只想在 Windows 本地改代码、远程服务器保持运行，也可以把 IDEA 只作为编辑器和 SSH 终端。Windows 的 `PSI Demo Server (Windows - Demo)` 通过 `PSI_REMOTE` 或 `remote-backend.txt` 桥接真实协议；两者都为空则只跑 mock。

如果拿到的是 `PSI-Demo-*-custom-input.zip` 交付包（里面没有 `.idea`），
按上面的终端命令 `python server/app.py --port 8787` 启动即可。

运行配置实际执行的是：

```text
python demo/server/app.py --host 127.0.0.1 --port 8787
```

工作目录固定为 `demo/`，所以 `from scenarios import ...` 和静态文件路径都能正常解析。

## 现场自检（10 秒）

页面右下角点“部署自检”，或直接请求：

```bash
curl -s http://127.0.0.1:8787/api/selfcheck | python3 -m json.tool
```

- `overall: ok`：全部就绪。
- `overall: warn`：可以演示，但通常是“没配真实后端”或“没生成 bench.json”。
- `overall: fail`：有阻断项（前端文件缺失、Python 太旧、二进制不可执行），先修再上台。
- 自检里会列出 `http://<服务器IP>:8787`；用 `--host 0.0.0.0` 启动时启动日志也会打印同样地址。

Windows 上如果 `python` 指向 3.7，推荐直接使用启动脚本，它会自动寻找 Python 3.10+：

```powershell
cd demo
.\start.ps1
```

## 0. 先准备依赖（只需一次）

`myPSI-Qin-3/CMakeLists.txt` 要求同级目录有 `libOTe`：

```bash
bash demo/scripts/setup_deps_linux.sh --psi /path/to/myPSI-Qin-3
```

脚本会 clone **v1.6.0**（最新版 API 已变，编不过 2023 年的代码）、编译 libOTe，并把源码软链到 `~/psi/myPSI-Qin-3`。
## 接真实后端（Linux）

一条命令搞定（编译 → 基线 → 打补丁 → 重编 → 450/550 翻转 → 实测）：

```bash
cd demo
bash scripts/bootstrap_linux.sh --src /path/to/myPSI-Qin-3 --bench
# 如需改规模：--bench-sizes 10,11,12 --bench-k 512 --bench-common 512
```

下面是不用脚本时的手工步骤。

前提：Linux 上已经能跑 `myPSI-Qin-3/bin/frontend`（注意：这个文件是 ELF，不是 Windows PE；Windows 直接双击会报“版本不兼容”）。

```bash
cd demo
export MYPSI_BIN=/path/to/myPSI-Qin-3/bin/frontend
# 如果二进制依赖的 .so 不在系统默认路径：
export MYPSI_LD_LIBRARY_PATH=/path/to/relic/build-linux/lib:/path/to/myPSI-Qin-3/lib:/path/to/libOTe/out/build/linux
export MYPSI_TIMEOUT=180
python3 server/app.py --host 0.0.0.0 --port 8787
```

浏览器打开 `http://<服务器IP>:8787`，在“参数”页切到“真实后端”。

> 现场实测（单线程二进制）：`2^10 / k=100` 约 3s，`2^11 / k=100` 约 6s（端到端约 8s），`2^12 / k=128` 约 23s；`2^14` 以上会超过 3 分钟。页面切入真实后端时会自动收敛到 `2^11 / k=100`，`2^20` 只作为论文口径展示，不要说成现场实测。

> 后端使用固定端口 `1212/1213`（`localhost`），所以**一次只能跑一个真实任务**；服务端已经加锁，第二个请求会提示串行演示。Qin-3 必须同时起两个发送方（角色 1/2），少一个接收方就会卡在 `client socket connect error`；Demo 后端已经自动按三进程编排。

## 与论文 Table IV/V 的数据对齐（N=3, t=0.6n）

论文实验条件：Intel Xeon E5-2630 v4 @ 2.2GHz、128GB、LAN 1000Mbps、N=3、t=0.6n。本机是**同型号 CPU** 的 32 vCPU VMware 虚拟机、125GB、localhost，因此时间可以直接对照。

| 集合规模 n | 论文 Table IV（LAN） | 本机墙钟 | 本机 C++ `总时间` | 本机通信 |
| --- | --- | --- | --- | --- |
| 1024 | 2.15s | 1.72s | 1.53s | 33.6 MB |
| 2048 | 7.10s | 6.31s | 4.80s | 67.2 MB |
| 4096 | 27.21s | 30.39s | 19.70s | 134.4 MB |

- 墙钟是 `/usr/bin/time` 的真实等待；`总时间` 是二进制 stdout 的值（代码用 `clock()`，更接近 CPU 时间）。论文表未说明计时口径，两个都给，避免拿不同口径硬比。
- 三次都是热运行。服务刚启动后的第一次会因接收方重连（`client socket connect error`）偏慢，可能多 5–15s；演示前建议先空跑一次热身。
- 通信量按论文 Fig. 14 的 N=3 线读约为 31 / 62.5 / 125 MB，本机分别高约 7–8%，与 `O(Nnλ)` 的线性关系一致。论文正文的精确点是 N=10、n=1024、125 MB；本机 N=3、n=1024 为 33.6 MB，按 N 线性外推到 N=10 约 112 MB，低约 10%。
- 截图里的 `n=4096, k=424, 38.20s` **不是**论文的 `t=0.6n` 配置（t=424≈0.10n），而且是冷运行；热运行墙钟约 28.0s、`总时间` 约 25.0s。要和论文对齐请用 `n=4096, k=2458`。
- 论文 Table VI/VII 是 Improved MP-TPSI、`t=0.2n`，Setup 占 26–860s；当前二进制的离线阶段只有 0.07–0.35s，耗时结构更接近 Table IV/V（MP-TPSI），不要拿 VI/VII 直接对比。
- 这篇论文最大规模到 `n=16384`（N=3, LAN, 415.04s）；页面上的 `2^20 / 7.36s` 是 CCS '23 pTPSI 的口径，不是这篇 IoV 论文的数据。

运行中点“取消（真实终止后端）”会调用 `POST /api/cancel`，服务端直接 kill sender/receiver；不会把进程留在后台占端口。运行失败时错误会停在运行页的红色卡片里，可展开原始错误并截图汇报。

## 让“未达到门限”也能真实演示（推荐）

现有二进制把 `threshold` 同时当作“实际交集大小”和“门限”，所以每次都会“达到门限”。要演示老师最关心的“交集不够就输出 ⊥”，应用补丁把这两个参数拆开：

```bash
cd /path/to/myPSI-Qin-3
git apply /path/to/demo/patches/0001-decouple-common-from-threshold.patch

# 重新编译（二选一）
cmake --build build --config Release -j
# 或按原项目的 build 方式重新 make
```

如果原项目的 `libOTe` 是精简版（例如 MP-SPDZ 自带的 `ENABLE_IKNP=OFF`），
直接 make 会报 `IknpOtExtReceiver was not declared`。用仓库里的脚本重编：

```bash
bash demo/scripts/build_patched_linux.sh \
  --src ~/psi-demo/mypsi-src \
  --libote-src ~/TPSST/MP-SPDZ/deps/libOTe \
  --libote-prefix ~/psi-demo/libote-install
```

脚本做四件事：换一份可移植的 `CMakeLists.txt`（原版硬编码作者机器的
`/usr/local/lib/libgmp.a`）、用 IKNP + SimplestOT + libsodium 重编 libOTe、
编译 myPSI、跑 450/550 双向自检。参考环境（Ubuntu 20.04，gcc-11）：

| 配置 | 结果 | C++ 内部总时间 | 页面整轮 wall |
|---|---|---|---|
| `2^10, k=500, common=450` | `Not reach threshold value` | 1.09s | 3.7s |
| `2^10, k=500, common=550` | `mIntersection's size:550` | 1.73s | 4.3s |

两个坑已经修在脚本里：

- libOTe 的 `Acceptor::stop()` 在连接没握手完时每 4 秒轮询、可能永久循环，
  会让每次运行多等 50 秒；脚本把它改成 200ms 非阻塞返回。
- 2021 版 `block` 是类，`prng.get<block>() + i` 需要 `+ toBlock(i)`；
  脚本用 `-include compat/block_u64.h` 补一个 `block + u64` 重载，不改算法源码。

现场别用 `2^14 / k=9000`：实测离线阶段超过 5 分钟仍未结束（k≈n/2 时
多项式次数最大，服务默认 180s 直接超时）。真实后端建议停在
`2^11 / k=100` 或 `2^10 / k=500`；要讲“规模大”，用 `2^16 / k=100`
这种“集合大、门限小”的配置，别把 k 拉满。

补丁只做三件事：

1. `main.cpp` 增加第 5 个参数：`frontend <party> <log2n> <threshold> [common]`。
2. `TPsiMain.h` / `TpsiMain.cpp` 把 `common` 传进生成集合的循环。
3. 生成共同元素用 `common`，门限比较仍用 `threshold`。

验证（开三个终端，先两个 sender，最后 receiver）：

```bash
# 终端 1：实际交集 450 < 门限 500
cd bin && ./frontend 1 10 500 450
# 终端 2：另一个发送方，监听 1213
cd bin && ./frontend 2 10 500 450
# 终端 3
cd bin && ./frontend 0 10 500 450
# 期望 receiver 输出：Not reach threshold value

# 再试 550 > 500
cd bin && ./frontend 1 10 500 550
cd bin && ./frontend 2 10 500 550
cd bin && ./frontend 0 10 500 550
# 期望 receiver 输出：mIntersection's size:...
```

然后重启 Demo 服务：

```bash
export MYPSI_PATCHED=1
```

参数页的“实际交集”滑块会解锁，可以现场演示“门限调高 → 结果翻转”。

## 目录结构

```text
demo/
  server/
    app.py            # 零依赖 HTTP 服务：静态文件 + config/run/progress/cancel/bench/version/selfcheck
    orchestrator.py   # mock 估算 + live 进程编排 + stdout 实时解析 + 取消
    diagnostics.py    # 部署自检：版本、LAN 地址、前端/后端/bench/任务状态
    scenarios.py      # 三个场景的文案、默认参数、业务动作
  web/
    index.html        # 5 屏
    styles.css        # 深色演示主题
    app.js            # 状态机、真实进度轮询、错误卡片、结果导出、性能渲染
  patches/
    0001-decouple-common-from-threshold.patch
    original/         # 补丁前的 3 个文件（便于对照）
    patched/          # 补丁后的 3 个文件
  data/
    generate.py       # 合成数据生成器（CSV/JSON，后续接文件输入用）
```

## API

```http
GET  /api/config
GET  /api/health
GET  /api/version     # 版本、启动时间、Python/系统、可访问 URL、前端资源时间
GET  /api/selfcheck   # 现场自检：前端/后端/bench/数据目录/任务状态
GET  /api/progress    # live 任务快照：{active, stage, message, elapsed, run_id}
POST /api/run
POST /api/cancel      # kill 当前 live 的 sender/receiver
GET  /api/bench       # 读 data/bench.json（本机实测）
```

`POST /api/run` 请求（`parties` 目前只接受 2；`n_log` 范围 10..20）：

```json
{
  "scenario": "ad",
  "data_mode": "generated",
  "n_log": 20,
  "threshold": 8000,
  "common": 12345,
  "parties": 2,
  "mode": "mock"
}
```

手动输入模式（只支持 `mode: "mock"`；每方最多 200 条、单条最多 64 字符）：

```json
{
  "scenario": "ad",
  "data_mode": "custom",
  "set_a": ["u1", "u2", "u3", "u4"],
  "set_b": ["u3", "u4", "u5"],
  "threshold": 2,
  "parties": 2,
  "mode": "mock"
}
```

手动模式的响应会额外返回 `size_a`、`size_b`、`actual_intersection`、`intersection_preview`（前 20 条交集），并把耗时记为本地求交实测值、`comm_mb` 记为 0、`security` 记为 `demo-only`。它不会启动 C++ 协议。

响应（节选）：

```json
{
  "decision": "accept",
  "intersection_size": 12345,
  "metrics": {"total_s": 7.95, "online_s": 7.36, "comm_mb": 28.8},
  "stages": [{"id": "encode", "ms": 266}],
  "source": "演示模拟（时间锚定 CCS'23 论文表 7.7）"
}
```

## 本机实测数据（性能页）

`bootstrap_linux.sh --bench` 默认串行跑 `2^10 / 2^11 / 2^12`（每档单次，`k=512, common=512`），并把结果原子写入 `demo/data/bench.json`；可用 `--bench-sizes`、`--bench-k`、`--bench-common`、`--bench-timeout` 覆盖。字段形如：

```json
{
  "generated_at": "2026-11-09T10:30:00+0800",
  "host": "lab-server", "os": "Linux-...", "kernel": "6.8.0",
  "nproc": 32, "mem_gb": 128.0,
  "benchmark": {"mode": "serial", "runs_per_size": 1, "sizes": [10, 11, 12], "k": 512, "common": 512},
  "cases": [{"n": 10, "k": 512, "wall_s": 4.1, "total_s": 1.9, "comm_mb": 0.8, "intersection": 512, "accepted": true}]
}
```

口径：本机串行、单次实测，包含进程启动开销；不要与论文 `2^20 / 7.36s`（32 线程 LAN）混说。服务运行中重新生成该文件后，刷新性能页即可看到绿色“本机实测”柱和表格行；没有文件时页面显示“尚无本机实测”，柱状图只有论文数字。

## 现场演示的 3 分钟脚本

1. **0:00-0:30** 首页：两边都有名单，但不能互看；广告归因 / 拼车怎么合作？
2. **0:30-0:50** 参数：A 方 100 万、B 方 100 万、门限 8,000；说明这是论文口径，本机实测会单独标注。
3. **0:50-1:50** 运行：live 模式五步跟着后端真实阶段亮；强调“原始数据不出域”，需要时点取消验证进程真的停了。
4. **1:50-2:20** 结果：12,345 ≥ 8,000 → 输出；点“改门限重跑”把门限调高 → 变成 ⊥；点“复制结论/导出 JSON”留档。
5. **2:20-2:50** 性能：先讲论文 7.36s（标注论文口径），再切到绿色“本机实测”柱说明本机串行跑的结果。
6. **2:50-3:00** 换场景、未来抗量子、请老师提问。

## 现在还不能做什么

- 当前真实后端是 Qin-3 三方协议（角色 0/1/2）；5 方、10 方仍是远期目标，需要先在 Linux 上跑通多进程/多端口。
- 当前二进制最大 `2^17`；`2^18..2^20` 是论文基准，页面会明确标注“演示模拟”。
- 真实文件模式已通过 `0002-real-data-input.patch` 支持哈希输入输出；浏览器负责把明文转成 32 位十六进制哈希，单方仍受 `2^17` 限制。生产环境要把裸 SHA-256 换成 OPRF/加盐哈希。
- 页面上的“手动输入”只调用 Python 演示后端做集合求交，不代表真实 C++ PSI 已支持任意文件/名单；现场不要把本地求交耗时说成协议耗时。
- 不接真实个人信息；所有数据为合成/演示数据。
