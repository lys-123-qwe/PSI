# PSI Lab Demo

把 CCS '23 pTPSI（Qin-3 三方概率门限隐私求交）包成可点击、可讲解、可现场演示的 Web Demo。

同一个 Python 服务同时提供：

- **演示模式**：不需要 C++ 后端，浏览器打开即可讲场景、门限 k、通过/拒绝和性能口径。
- **真实数据模式**：浏览器读取 A/B/C 三份文件并在本地哈希，服务端调用 Linux 上的 Qin-3 二进制，返回真实交集或 `⊥`。

> 重要：GitHub Pages 只能托管静态网页，不能运行 `/api/run`、`/real` 和 Qin-3 进程。别人要使用真实数据模式，必须在他自己的电脑/服务器上启动本项目的 Python 服务，或把 `demo/remote-backend.txt` 指向一台已经部署好的 Linux 真实后端。

## 快速开始（演示模式，跨平台）

要求 Python 3.10+，不需要 `pip install`，不需要 C++ 后端。

Windows PowerShell：

```powershell
cd demo
.\start.ps1
```

也可以双击 `demo\start.cmd`（它只负责用 PowerShell 执行同一个脚本）。

Linux / macOS：

```bash
cd demo
bash start.sh
```

也可以手动启动：

```bash
python3 demo/server/app.py --host 127.0.0.1 --port 8787
```

浏览器打开 <http://127.0.0.1:8787>。首页右上角可以切换“演示模式 / 真实后端”，真实文件入口是 <http://127.0.0.1:8787/real>。

如果希望同一局域网的其他电脑访问，启动时把监听地址设为 `0.0.0.0`：Windows 先执行 `$env:HOST="0.0.0.0"`，Linux 使用 `HOST=0.0.0.0 bash start.sh`；再用本机 LAN 地址访问。

## 真实数据模式

真实模式需要 Linux 上的 myPSI-Qin-3 `frontend` 二进制。Windows 不能直接执行 Linux ELF，所以有两种部署方式。

### 方式 A：Linux 本机直接运行

在 Linux 上准备好项目、编译依赖和二进制，然后：

```bash
cd demo
export MYPSI_BIN=/path/to/myPSI-frontend-patched
export MYPSI_PATCHED=1
export MYPSI_TIMEOUT=300
python3 server/app.py --host 0.0.0.0 --port 8787
```

如果二进制依赖 RELIC/libOTe 动态库：

```bash
export MYPSI_LD_LIBRARY_PATH=/path/to/relic/build-linux/lib
```

部署到服务器时，防火墙/安全组只放行你实际需要的端口，并建议放在内网或反向代理之后。

### 方式 B：Windows/macOS 通过远程 Linux 后端桥接

1. 在 Linux 上按方式 A 启动同一个 Demo 服务，确认 `http://<Linux-IP>:8787/api/health` 返回 `ok: true`。
2. 在本地电脑复制配置模板：

```powershell
cd demo
Copy-Item remote-backend.example.txt remote-backend.txt
notepad remote-backend.txt
```

3. 在 `remote-backend.txt` 写入一行 Linux 服务地址，例如：

```text
http://192.0.2.10:8787
```

4. 启动本地服务：

```powershell
.\start.ps1
```

浏览器仍然只访问本机 `http://127.0.0.1:8787`。本机 Python 服务会把 `live` 和 `/real` 请求转发给 Linux 后端；远程不可达时演示模式仍然可用。

`remote-backend.txt` 已加入 `.gitignore`，不要把真实服务器地址提交到公开仓库。也可以不建文件，直接设置环境变量：

```powershell
$env:PSI_REMOTE = "http://192.0.2.10:8787"
.\start.ps1
```

### 真实模式的前置条件

- Linux x86_64 和可执行的 myPSI `frontend`；
- `MYPSI_BIN` 指向该二进制，必要时设置 `MYPSI_LD_LIBRARY_PATH`；
- 当前真实二进制单方最多 `2^17 = 131072` 条，三方任务串行执行；
- 编译/自检脚本见 `demo/scripts/bootstrap_linux.sh` 和 `demo/scripts/verify_backend.sh`。

论文中的 `2^20 / 7.36s` 是 CCS '23 的 32 线程 LAN 口径，不是本仓库默认机器的实测值；页面已单独标注。

## 配置项

| 配置 | 用途 |
| --- | --- |
| `MYPSI_BIN` | Linux 真实后端二进制路径；不设置就只跑演示模式 |
| `MYPSI_PATCHED` | 是否为支持 `common != threshold` 和真实文件输入的补丁版 |
| `MYPSI_TIMEOUT` | 单次真实协议超时时间，默认 300 秒 |
| `MYPSI_LD_LIBRARY_PATH` | 真实二进制依赖的动态库目录 |
| `PSI_REMOTE` | 远程 Linux Demo 地址；也可以写入 `demo/remote-backend.txt` |
| `PSI_PUBLIC_URL` | 页面上展示给现场观众的远程服务地址 |
| `PSI_REMOTE_RUN_TIMEOUT` | 本地桥接等待远程真实任务的超时，默认 420 秒 |

## 安全与隐私边界

- 浏览器只上传元素的 SHA-256 截断值，不上传 `/real` 页面的明文文件；服务端临时目录只写哈希并在任务结束后删除。
- 当前演示使用裸 SHA-256 + 截断，**不等价于生产级 OPRF**。手机号、身份证号等低熵数据不能直接这样处理。
- 不要提交真实用户、订单、患者或未成年人数据；基因场景只使用合成数据。
- 当前服务没有登录、限流和公网多租户隔离，不建议直接把实验室 8787 端口暴露到互联网。
- 当前是 Qin-3 半诚实/论文路径的演示实现；不要把它宣传成完整的恶意模型或抗量子生产系统。

## 仓库结构

```text
demo/server/       Python 零依赖 HTTP 服务、参数校验、三方进程编排、远程桥接
demo/web/          首页、参数页、运行页、结果页、性能页、/real 真实文件页
demo/patches/      Qin-3 C++ 工程补丁：门限/共同元素解耦、真实文件输入输出
demo/scripts/      Linux 依赖安装、编译、自检、benchmark 脚本
demo/data/         bench.json 和合成数据生成脚本
sources/           myPSI-Qin-3 与 libOTe 源码/依赖副本
```

更细的页面流程、API、论文映射和现场讲解脚本见 `demo/README.md`。

## 上传 GitHub 前检查

```powershell
git status --short
git check-ignore demo/remote-backend.txt
python -m py_compile demo/server/app.py demo/server/backends.py demo/server/remote.py
node --check demo/web/app.js
node --check demo/web/real.js
```

以下内容不应提交：`demo/remote-backend.txt`、`.idea/`、`outputs/`、`work/`、真实数据文件、服务器日志和任何私钥。仓库中的 `sources/libOTe` 体积较大，如果只需要运行 Web Demo，可以只克隆 `demo/` 目录；需要编译真实后端时再准备完整依赖。

> 如果旧仓库的历史提交里曾经包含服务器 IP、用户名或本机绝对路径，清理当前工作区并不会删除历史中的副本。要公开时，建议新建一个干净仓库并上传当前文件，或使用 `git filter-repo` 清理历史后再公开。

## 已验证的两种启动方式

- 只有 Python 3.10+：演示模式可用，真实模式显示“未配置真实后端”提示，不会卡死。
- 配置 `MYPSI_BIN` 或 `PSI_REMOTE`：一次启动同时提供演示模式和真实数据入口；远程不可达时自动退回演示模式。

## 许可证与第三方代码

本仓库包含/引用了 `myPSI-Qin-3`、`libOTe`、`ptpsi-ccs23` 等第三方代码和论文实现。公开发布前请分别核对其许可证和引用要求；论文中的性能数字与代码实现不能替代真实生产安全评估。
