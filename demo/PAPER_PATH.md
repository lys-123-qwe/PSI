# 完整 2^20 pTPSI 路径（2-party）落地方案

> 目标：回答老师“百万级到底能不能跑完整协议”。
> 结论先说：**当前 Demo 不能把 2^20 / 7.36s 说成本机完整协议成绩。**
> 现有 Qin-3 是三方多项式 TPSI，2^12 已 183MB 通信；官方 pTPSI 仓库只提供
> GS 和 F-count 原型，缺 GS→F-count 的胶水代码，也缺最后的 PSI 阶段。
> 完整 2^20 必须单独做一条 2-party 流水线，预计 1–2 周。

## 1. 论文口径拆解

论文 Table 7.7：2-party、LAN 1000Mbps、32 线程、`n=2^20`、`k=8000`、
半诚实在线阶段 **7.36s**；恶意 **9.74s**。

| 阶段 | 论文在线耗时 | 代码现状 | 本机现状 |
|---|---:|---|---|
| GS 集合大小测试 | 3.52s（Table 7.4） | 官方 `gs.cpp` 可编译 | `gs20` 实测约 2.3s（2.32–2.44s） |
| F∩-count（MP-SPDZ） | 1.62s（Table 7.5） | 官方 `F-count-SPDZ.mpc`，硬编码小样例 | MP-SPDZ 已装，未接 GS 输出 |
| pSize-Test 小计 | 7.02s（Table 7.6） | GS + F-count + 胶水 | 未打通 |
| 最终 PSI | 约 0.34s（7.36 - 7.02） | 官方仓库没有 | 未接 |
| 完整 pTPSI | 7.36s | 缺 PSI 与胶水 | 不能宣称 |

注意：
- 约 2.3s 只是 GS 阶段，不是 pSize-Test，更不是完整 pTPSI。
- Qin-3 的 2^12 / 2.31s 是三方多项式协议，与论文 2-party 7.36s 不可横比。
- 不要把“GS 2.3s + F-count 1.62s = 3.9s”当完整成绩；中间还有协议开销和 PSI。

## 2. 目标架构

```text
浏览器
  └─ GET  /api/paper-path          # 查三个阶段状态，不跑重任务
  └─ POST /api/paper-path/run      # 跑指定阶段，返回真实耗时
        └─ demo/server/paper_path.py
              ├─ GS stage        : official gs.cpp / gs20
              ├─ F-count stage   : MP-SPDZ F-count-SPDZ.mpc
              └─ PSI stage       : 2-party OPRF PSI（论文 [16] 或兼容实现）
```

三个阶段都返回同一种结构，前端只认 `status` 和 `scope`：

```json
{
  "mode": "paper-2party",
  "n_log": 20,
  "n": 1048576,
  "k": 8000,
  "paper_total_s": 7.36,
  "local_total_s": null,
  "comparable": false,
  "stages": [
    {
      "id": "gs",
      "name": "GS 集合大小测试",
      "status": "measured",
      "scope": "仅 GS 阶段",
      "local_s": 2.44,
      "paper_s": 3.52,
      "engine": "~/psi-demo/paper-gs/gs20"
    },
    {
      "id": "fcount",
      "name": "F∩-count（MP-SPDZ）",
      "status": "not_integrated",
      "scope": "未接 GS 输出",
      "paper_s": 1.62,
      "engine": "~/TPSST/MP-SPDZ"
    },
    {
      "id": "psi",
      "name": "OPRF PSI",
      "status": "unavailable",
      "scope": "官方仓库未提供",
      "paper_s": 0.34
    }
  ],
  "verdict": "当前只复现 GS 阶段；完整 7.36s 尚未打通。"
}
```

`status` 只允许：
- `available`：二进制/源码在，但本次没有跑；
- `measured`：本机真实跑过，有 `local_s`；
- `not_integrated`：代码/环境在，但没接上；
- `unavailable`：缺二进制或缺源码；
- `failed`：跑失败，带 `error` 和 `hint`。

**前端永远不把 `paper_s` 和 `local_s` 相加，也不把不同 `scope` 的阶段拼成总数。**

## 3. 分阶段实施

### S1：GS 阶段 + 状态接口（1–2 天）

改动：
- 新增 `demo/server/paper_path.py`；
- `demo/server/app.py` 增加 `GET /api/paper-path`、`POST /api/paper-path/run`；
- `demo/server/remote.py` 增加 `proxy_paper_path()`，Windows 本地服务转发到 Linux；
- 性能页增加“论文完整路径（2 方）”卡片，显示三行阶段状态；
- `demo/thirdparty/ptpsi-ccs23/` 保留官方 `gs.cpp`，不改协议。

验收：
- `GET /api/paper-path` 在 1 秒内返回三阶段状态；
- 点“跑 GS”只跑 GS，返回 `local_s≈2.3s`，`scope` 明确写“仅 GS 阶段”；
- 页面不出现“完整 7.36s 已复现”这类文案；
- 无二进制时返回 `unavailable` 和安装提示，不 500。

### S2：GS → F-count 胶水（3–5 天）

官方 `gs.cpp` 目前只打印 `Total time`，`P1_correct_set` / 计数都被丢弃。
需要新增 `gs_emit.cpp`（或给 `gs.cpp` 加 `--emit`）：

输出：
- `p1_indexarr.txt`：每个非空桶的大小；
- `p1_data.txt` / `p2_data.txt`：桶内元素（十进制）；
- `gs_summary.json`：`n`、`k`、`m`、`rounds`、GS 耗时、非空桶数。

然后：
- 写 `demo/thirdparty/ptpsi-ccs23/fcount/gen_inputs.py`，把上面三个文件转成 MP-SPDZ 输入；
- 写 `run_fcount.sh`：`compile.py` → `emulate.x`（正确性）→ `Player-Online.x`（2-party 计时）；
- 官方 `F-count-SPDZ.mpc` 是硬编码 3231 轮、`sfix`、朴素双重循环；2^20 不要直接跑这份。
  服务器已有 `F-count-CHUNK-SINT-0.bc` / `F-count-FIX-SINT-0.bc`，优先用 `sint` 版本；
- 阈值改成命令行/生成参数，输出 `compare_result` 和 `count`。

验收：
- 同一份输入，明文计数和 MPC 输出一致；
- `k=369` 通过、`k=500` 拒绝（小规模）；2^20 / k=8000 记录真实在线/离线耗时；
- 页面显示 `pSize-Test 本机 xxs` 与 `论文 7.02s` 两列，不合并。

### S3：2-party PSI 阶段（3–5 天）

论文 [16] 是 Raghuraman & Rindal, CCS 2022 的 OPRF PSI；官方 pTPSI 仓库没有它。
两条路：
1. 接该论文的 OPRF PSI 实现（最贴近论文，但要先拿到/编译对应仓库）；
2. 先用 libOTe 的 OPRF/OKVS 写一个 2-party PSI CLI，明确标注“半诚实、兼容实现”，
   不冒充论文 [16] 的恶意安全实现。

建议接口：
```bash
psi2 --receiver --in a.txt --out intersection.txt --threads 32
psi2 --sender   --in b.txt --threads 32
```

验收：
- 2^12 / 2^16 先对齐明文交集；
- 2^20 在 32 线程、LAN 下跑通，记录墙钟和通信量；
- 页面显示 `完整路径本机 xxs`，并注明“PSI 阶段实现/安全模型”；
- 若 PSI 不可用，页面必须停在 `unavailable`，不能显示假总数。

### S4：工程化（2 天）

- 所有路径走环境变量：`PAPER_GS_BIN`、`PAPER_MP_SPDZ_DIR`、`PAPER_FCOUNT_MPC`、`PAPER_PSI_BIN`；
- 临时输入跑完即删，不落日志；
- `GET /api/selfcheck` 增加三阶段检查；
- README 增加“如何在一台 32 线程 Linux 上复现”；
- Docker/一键脚本放到 P2，不要阻塞两周演示。

## 4. 现在就能对老师说的话

- “三方 Qin-3 真实协议已经能现场跑，2^12 约 2.3s 协议时间；真实文件走 `/real`。”
- “2^20 的 GS 阶段已经用官方 `gs.cpp` 跑通，本机约 2.3s；论文 GS 是 3.52s。”
- “完整 7.36s 还差 F-count 胶水和 PSI 阶段；官方仓库只给原型，我们正在按 1–2 周补。”
- “我们不会把 GS 2.3s 或 Qin-3 2^12 的成绩说成 7.36s；页面上论文口径和本机实测分开。”

## 5. 风险与依赖

- 论文机器是 32 线程、32GB、LAN 1000Mbps；本机 VM 核数/内存不足时，2^20 会更慢。
- MP-SPDZ 需要能编译 `emulate.x` / `Player-Online.x`；当前服务器只有 `Fake-Offline.x`，
  需要先 `make -j$(nproc) emulate.x Player-Online.x`。
- 官方 `F-count-SPDZ.mpc` 是固定小样例，不是 2^20 参数化电路；必须生成输入/改电路。
- 官方仓库没有 PSI 阶段；这是最大的外部依赖，不能用 Qin-3 冒充。
- 2^20 的集合、哈希表、MP-SPDZ 输入会占数百 MB 到 GB 级内存；演示机要先压测。
