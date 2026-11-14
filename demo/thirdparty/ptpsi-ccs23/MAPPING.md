# CCS'23 pTPSI 官方代码（论文第 4 篇）— 现状与用法

来源：<https://github.com/Frankhtu/pTPSI.git>（main，代码提交 2023-08-03）
论文：Feng-Hao Liu, En Zhang, Leiyong Qin. *Efficient Multiparty Probabilistic
Threshold Private Set Intersection.* CCS '23.
本目录内容来自上述官方仓库，并保留了来源说明；如需复核，请以官方仓库对应提交为准。

## 它是什么

论文的**原型 / 测量代码**，7 个文件、16 KB。每个 `.cpp` 自带 `main()`、参数硬编码、
读写固定文件名（`p1_20.txt`、`f20.txt`、`a20.txt` …），**不是**能被 Demo 直接调用的
PSI 命令行程序，也不输出交集大小或门限判定。

## 文件 ↔ 论文阶段

| 文件 | 干什么 | 硬编码参数 |
|---|---|---|
| `balls and bins model test.cpp` | 哈希桶装载（balls into bins）溢出概率实验 | `N=2^18, Round=100, m=64` |
| `create_hashtable.cpp` | 哈希表构建开销 | `N=2^19, Round=100, m=64` |
| `FalsePositiveTest.cpp` | 假阳性率 | `N=2^18, Round=5000` |
| `gs.cpp` | GS 阶段（GF(2^m) 多项式求值 + 索引对齐） | `N=2^22, Round=8000` |
| `gs_hashtable.cpp` | GS + 哈希表 | 同上 |
| `F-count-SPDZ.mpc` | 多方计数（MP-SPDZ 的 `.mpc`） | `P1_count=6540, Round=3881` |

## 依赖（缺一个就编不过）

- `cryptoTools`（来自 libOTe 源码树）：`balls and bins`、`create_hashtable`、`gs_hashtable`
- `sha1.hpp`：**仓库里没有**，需要自行补（前三个文件都 include 了它）
- NTL（`ZZ_pX`、`GF2X`）、Boost、OpenMP
- `F-count-SPDZ.mpc`：需要 MP-SPDZ 框架，不是 g++ 能编的

## 和 `myPSI-Qin-3` 的关系

`myPSI-Qin-3` 的 "Qin" 就是本文作者 Leiyong Qin，是基于 libPSI/libOTe 的完整协议实现
（有 sender/receiver、1212/1213 端口、`mIntersection's size`），**Demo 实际调用的是它**。
本目录这份是论文里的原型实验代码，两者不是同一个工程。

## 怎么验证能不能编

```bash
bash demo/scripts/probe_paper4.sh                 # 逐文件 -fsyntax-only
LIBOTE_DIR=/path/to/libOTe bash demo/scripts/probe_paper4.sh
```

输出每个文件的 `OK` / `FAIL + 首个错误`，以及 `sha1.hpp` 是否存在。
这一步只做"能不能编"的判定，不跑实验。

## 要真正跑出数据，需要

1. 补 `sha1.hpp`，写一个 Makefile（每个 cpp 一个可执行文件）
2. 把硬编码的 `N` / `Round` 改成命令行参数，否则改一次实验要重编一次
3. 用 MP-SPDZ 跑 `F-count-SPDZ.mpc`，`P1_count` / `Round` 要按 GS 阶段真实输出填

工作量估计：第 1-2 步 2~3 天；第 3 步需要先装 MP-SPDZ，1~2 周，且是独立于本次
两周交付的另一条线。
