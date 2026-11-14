# IDEA 使用说明

这个项目可以直接用 IntelliJ IDEA 打开，也可以用普通终端启动。下面的路径都以“项目根目录”为基准。

## 1. 打开项目

在 IDEA 中选择：

```text
File -> Open -> <项目根目录>
```

打开后如果看不到运行配置，执行 `File -> Reload from Disk`；仍然看不到时重启 IDEA。

## 2. 项目结构

```text
demo/server       Python 服务端：HTTP、参数校验、三方进程编排、远程桥接
demo/web          HTML/CSS/JS 前端：首页、参数、运行、结果、性能、/real
demo/patches      C++ 工程补丁：门限与共同元素解耦、真实文件输入输出
demo/scripts      Linux 编译、依赖安装、benchmark、验证脚本
demo/data         bench.json 和样例生成脚本
sources/          myPSI-Qin-3 与 libOTe 源码/依赖副本
```

## 3. Windows：演示模式（不需要后端）

在项目根目录执行：

```powershell
cd demo
.\start.ps1
```

如果 PowerShell 执行策略不方便，双击 `demo\start.cmd` 也可以。

脚本会自动寻找 Python 3.10+，然后启动：

```text
python server/app.py --host 127.0.0.1 --port 8787
```

浏览器打开 <http://127.0.0.1:8787>。演示模式不需要 Linux 二进制，适合讲三张业务场景卡、A/B/C 三方数据、门限 k 和结果页。

如果 IDEA 里使用运行配置，选择 `PSI Demo Server (Windows - Demo)` 即可。配置里不写死远程地址，服务会按下面的顺序寻找真实后端：

1. `PSI_REMOTE` 环境变量；
2. `demo/remote-backend.txt` 第一行非注释内容；
3. 都没有时只使用演示模式。

## 4. Windows/macOS：远程真实后端桥接

Windows 不能直接运行 Linux ELF，因此需要一台 Linux 机器先启动本项目。Linux 端设置：

```bash
cd demo
export MYPSI_BIN=/path/to/myPSI-frontend-patched
export MYPSI_PATCHED=1
export MYPSI_TIMEOUT=300
python3 server/app.py --host 0.0.0.0 --port 8787
```

在本地电脑复制配置模板：

```powershell
cd demo
Copy-Item remote-backend.example.txt remote-backend.txt
notepad remote-backend.txt
```

把 Linux 服务地址写进去，例如：

```text
http://192.0.2.10:8787
```

然后重新运行 `.\start.ps1`。本地服务会把 `live` 和 `/real` 请求转发到 Linux 后端，浏览器仍然只访问本机 8787。远程不可达时演示模式仍可用，`/real` 顶部会提示原因。

`remote-backend.txt` 已在 `.gitignore` 中，不能提交到 GitHub。也可以不用文件，直接设置：

```powershell
$env:PSI_REMOTE = "http://192.0.2.10:8787"
.\start.ps1
```

## 5. Linux：真实数据模式

在 Linux 上编译并自检真实后端后：

```bash
cd demo
export MYPSI_BIN=/path/to/myPSI-frontend-patched
export MYPSI_PATCHED=1
export MYPSI_TIMEOUT=300
export MYPSI_LD_LIBRARY_PATH=/path/to/relic/build-linux/lib
python3 server/app.py --host 0.0.0.0 --port 8787
```

IDEA Remote Development 的 Linux 运行配置对应：

```text
Script:       <项目根目录>/demo/server/app.py
Interpreter:  /usr/bin/python3
Working dir:  <项目根目录>/demo
Options:      --host 0.0.0.0 --port 8787
Env:          MYPSI_BIN=<真实二进制路径>; MYPSI_PATCHED=1; MYPSI_TIMEOUT=300
```

如果使用远程开发，请把上面的 `<...>` 替换成服务器上的实际路径。不要把自己的服务器地址或路径写进公开 README。

## 6. 两个浏览器入口

```text
http://<地址>:8787/       首页、场景、参数、运行、结果、性能
http://<地址>:8787/real   三方真实文件模式（需要真实后端）
```

两个入口由同一个 Python 服务提供。服务启动时会自动检测本地 `MYPSI_BIN` 或 `remote-backend.txt`：可用则首页默认真实模式；不可用则停在演示模式，不会让页面变成死路。

## 7. 常见问题

- `python` 指向 3.7：使用 `.\start.ps1`，它会优先找 Python 3.10+；或设置 `$env:PYTHON`。
- 8787 被占用：设置 `$env:PORT=8788` 后重新启动，或结束占用进程。
- IDEA 报 `PYTHONUNBUFFERED=1` 不是命令：这是 Linux shell 的写法，Windows PowerShell 不支持。改选 `PSI Demo Server (Windows - Demo)`，并不要在 Windows 的 Shell Script 运行配置里添加 `变量=值` 形式的环境变量；直接运行 `.\start.ps1` 最稳。
- Linux 真实模式报 `MYPSI_BIN` 不存在：检查二进制路径、执行权限和动态库路径。
- `/real` 报远程后端不可达：先访问 `http://<Linux-IP>:8787/api/health`，再检查防火墙和安全组。
- 只想做动画/口头演示：不要配置 `MYPSI_BIN` 和 `PSI_REMOTE`，直接使用演示模式。
