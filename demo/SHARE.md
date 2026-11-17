# 别的电脑怎么用这个 Demo

这套 Demo 有两种模式，可以在同一个页面里切换：

- **演示模式（mock）**：不需要服务器、不需要联网，装好 Python 就能跑，适合讲流程和门限翻转。
- **真实数据模式（live）**：调用 Linux 上的 Qin-3 三方真实协议，需要一台已经部署好的 Linux 真实后端。

下面按“对方是谁”分三种用法。

> **你们俩都用导师同一台 Linux 时**：不用两台机器。A 起 `PORT=8787` + `MYPSI_PORT_BASE=1200`，B 起 `PORT=8788` + `MYPSI_PORT_BASE=1320`；两个人各自的 Windows Demo 分别把 `remote-backend.txt` 指向 `http://服务器:8787` 和 `http://服务器:8788`。同一实例内串行，两个实例可以同时跑真实任务。具体命令见第四节。

## 一、只给对方看：浏览器直接打开

对方电脑什么都不用装，直接访问项目负责人提供的地址：

```
http://<服务器IP>:8787          # 首页：演示模式 / 真实后端
http://<服务器IP>:8787/real     # 三方真实文件页
```

注意：

- 地址通过微信/邮件单独发，不要写进公开的 GitHub 仓库。
- 8787 目前没有登录鉴权，适合内部演示；长期对外需要加反向代理、鉴权或限制安全组。
- 两个人同时用真实协议时，各用各的实例：不同 Web 端口 + 不同 `MYPSI_PORT_BASE`（如 8787/1200 与 8788/1320），不会抢 1212/1213。

## 二、对方自己电脑启动（推荐）

### 1. 安装 Python

需要 **Python 3.8 或更高版本（推荐 3.10+）**，不需要安装任何 pip 包。

Windows 安装时勾选 **Add Python to PATH**。

### 2. 解压并启动

解压 `PSI-Demo-Windows-*.zip`，进入解压出来的 `PSI-Demo` 文件夹，双击：

```
start-demo.cmd
```

浏览器会自动打开 `http://127.0.0.1:8787`。这就是完整的演示模式。

如果 Windows 提示“已保护你的电脑”或双击没反应：右键压缩包 → 属性 → 勾选“解除锁定” → 重新解压。这是 SmartScreen 对网上下载文件的常规提示，不是 Demo 本身的问题。

如果双击没有反应，打开 PowerShell 手动运行：

```powershell
cd PSI-Demo
.\start.ps1
```

### 3. 同时使用真实数据模式

双击：

```
connect-server.cmd
```

按提示粘贴服务器地址，例如 `http://1.2.3.4:8787`；如果有主/备两个后端，
用逗号或空格分开，按优先级从主到备填写：

```
http://1.2.3.4:8787, http://5.6.7.8:8787
```

脚本会：

1. 逐个请求 `/api/health` 检查是否在线、有没有真实后端；
2. 把所有地址按顺序写入本机 `demo/remote-backend.txt`（不会上传到 Git）；
3. 启动本机 Demo，并自动把真实协议请求转发到第一个可用后端。

浏览器仍然只访问 `127.0.0.1:8787`。服务器不可达时，页面会自动回落到演示模式，不影响讲解。

也可以一条命令完成：

```powershell
cd PSI-Demo
.\scripts\connect-remote.ps1 -Url http://1.2.3.4:8787,http://5.6.7.8:8787
```

## 三、对方自己部署一台真实后端（Linux）

适合实验室服务器或商业化部署。要求：

- Linux x86_64；
- Python 3.8+（推荐 3.10+）；
- 已编译好的 `myPSI-frontend-patched`；
- RELIC 等动态库路径正确。

```bash
cd demo
MYPSI_BIN=/path/to/myPSI-frontend-patched \
MYPSI_PATCHED=1 \
MYPSI_LD_LIBRARY_PATH=/path/to/relic/build-linux/lib \
bash start.sh
```

对外开放时用 `--host 0.0.0.0`，并确认防火墙放行该 Web 端口。真实协议默认占用本机 1212/1213；同一台机器起第二个实例时用 `MYPSI_PORT_BASE=1320` 改成 1332/1333。

## 四、两个真实后端（主/备自动切换）

如果你们各有一个 Linux 后端（两台机器，或导师同一台机器上的两个实例），任何一个没启动时另一个仍可继续提供真实数据。

在每台 Windows 的 `demo/remote-backend.txt` 里写两行，顺序是“自己优先”：

你的电脑（实例 A：8787 / base 1200）：

```
http://你的后端:8787
http://对方的后端:8788
```

对方的电脑（实例 B：8788 / base 1320）：

```
http://对方的后端:8788
http://你的后端:8787
```

启动后本机会并行探测所有地址，行为是：

- 第一个“可达且有真实后端”的地址被选中；
- 主后端连接失败（拒绝连接、返回 404/502/503/504）时，真实运行自动尝试下一个；
- 进度查询和取消绑定到实际跑任务的那台后端；
- 两个实例都不可用时，页面自动回落到演示模式，演示模式不受影响；
- 页面底部和 `/real` 顶部会显示当前用的是主还是备。

注意：

- 两个实例都要部署同一版 demo，并配置 `MYPSI_BIN` 和 RELIC 动态库；
- 同一台 Linux 可以放两个实例，但必须 `PORT` 和 `MYPSI_PORT_BASE` 都不同；同一个实例内仍串行；
- 响应超时不会自动换后端，因为任务可能已经在跑，换过去可能重复计算；
- 两个实例都建议开 `--host 0.0.0.0`，并只对你们两人的网络开放各自的 Web 端口。

第二个实例这样起（同一台导师 Linux 也可以，只要端口不同）：

```bash
# 把项目里的 demo/ 目录和已编译好的二进制放到第二个实例/第二台机器
cd demo
MYPSI_BIN=/path/to/myPSI-frontend-patched \
MYPSI_PATCHED=1 \
MYPSI_LD_LIBRARY_PATH=/path/to/relic/build-linux/lib \
HOST=0.0.0.0 PORT=8788 MYPSI_PORT_BASE=1320 \
bash start.sh
```

同一台 Linux 的第一个实例用 `PORT=8787 MYPSI_PORT_BASE=1200`；两个实例可以同时跑真实任务，互不抢协议端口。

如果第二个实例没有编译环境，直接用项目负责人生成的独立后端包更省事：

```bash
tar -xzf PSI-Backend-Linux-x86_64-*.tar.gz
cd PSI-Backend
./start-backend.sh
```

这个包已经带了 `myPSI-frontend-patched` 和运行所需的共享库。完整步骤、端口、systemd 和排错见 `BACKENDS.md`。

如果没有现成二进制，用 `demo/scripts/bootstrap_linux.sh --src /path/to/myPSI-Qin-3` 在第二个实例/第二台机器上编译并打补丁。启动后确认：

```bash
curl -s http://第二个实例IP:8788/api/health
```

返回里应有 `"exists": true`。两个实例都确认后，再按上面的顺序填写 `remote-backend.txt`。

检查两个实例都健康：

```bash
curl -s http://主后端:8787/api/health
curl -s http://备后端:8788/api/health
```

两边都应返回 `"exists": true`。

页面参数区和 `/real` 顶部会显示“主 / 备”状态点。点“重新检测”可以立即刷新，不需要重启 Demo。

## 五、同一局域网共享自己电脑上的 Demo

对方和你在同一个办公室/校园网时，可以让你自己的电脑当服务器：

```powershell
cd demo
$env:HOST = "0.0.0.0"
.\start.ps1
```

别人访问 `http://你的局域网IP:8787`。Windows 首次会弹防火墙提示，选择允许“专用网络”。

## 六、上传到 GitHub 给别人用

可以，但要注意：

- **GitHub Pages 不能用**：它只能托管静态文件，没有 `/api/*`，也没有真实协议进程。别人必须 clone 后本地启动 Python。
- 仓库里不要提交 `demo/remote-backend.txt`，它已被 `.gitignore` 排除；模板是 `demo/remote-backend.example.txt`。
- `demo/bin/` 里的 Linux 二进制已被忽略，不会上传；别人用方式二连接你的服务器即可。
- 公开仓库前先检查提交历史里有没有服务器 IP、用户名、`/home/lys` 等内网信息。

对方拿到仓库后：

```bash
git clone <你的仓库地址>
cd <仓库>/demo
python start.sh        # Linux / macOS
.\start.ps1            # Windows PowerShell
```

## 七、常见问题

**Windows Git Bash 里执行 `cd ~/psi-demo/demo` 或 `bash start.sh`，报 `No such file or directory`？**

这说明命令敲在了 Windows 上，而不是那台 Linux 服务器上。`/home/lys/...`、`~/psi-demo`、`start.sh` 都是 Linux 路径/文件，Windows 的 Git Bash 里没有。

- 如果只是要用 Demo（推荐）：解压 `PSI-Demo-Windows-*.zip`，双击 `start-demo.cmd`；真实数据模式用 `connect-server.cmd` 指向 `http://服务器:8788`。Windows 上不需要设置 `MYPSI_BIN`。
- 如果确实要启动 Linux 后端：先 `ssh lys@59.70.82.96`（或你们自己的 Linux 地址），登录成功后再执行 `cd ~/psi-demo/demo` 和 `bash start.sh`。
- Windows 用户名带单引号（如 `he'yv`）时，Git Bash 处理路径容易出错；建议把压缩包解压到 `C:\PSI-Demo` 这类不含特殊字符的目录，并用 `cmd`/PowerShell 启动。

**页面右上角显示“演示模式”，切不到真实后端？**

说明本机没有 `MYPSI_BIN`，也没有配置可用的 `remote-backend.txt`。先运行 `connect-server.cmd`，或检查服务器 `/api/health`。

**提示 Python 版本太低？**

本项目要求 Python 3.8+（推荐 3.10+）。Windows 可以在 Microsoft Store 或 python.org 安装，安装时勾选 Add to PATH。

**8787 端口被占用？**

```powershell
$env:PORT = "8788"
.\start.ps1
```

然后访问 `http://127.0.0.1:8788`。

**真实模式很慢或失败？**

- 真实协议是三方 Qin-3，2^16 大约 18–21 秒，2^18 约 105–120 秒，属于正常范围。
- 先确认服务器上 `myPSI-frontend-patched` 存在且可执行。
- 三方进程都跑在同一台服务器上，这是协议演示，不是三方各自持机的生产部署。
- 2^20 / 7.36s 是论文的 2-party 在线阶段口径，不能和这里的 Qin-3 三方结果直接横比。

**数据和隐私怎么保证？**

- 浏览器先本地做 SHA-256，再把哈希交给后端；明文文件不出浏览器。
- 低熵数据（手机号、身份证）不能直接用裸 SHA-256，生产需要 OPRF/加盐哈希。
- 基因场景只用合成数据；儿童教育场景不接真实数据。

## 八、给对方发什么

最少发两样：

1. 一个压缩包（或 GitHub 地址）；
2. 真实后端地址（只有对方需要真实模式时才发）。

压缩包可以用下面的命令重新生成：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\make-share-package.ps1
```

生成位置在仓库根目录的 `outputs/`，文件名形如 `PSI-Demo-Windows-20261116-1700.zip`。
