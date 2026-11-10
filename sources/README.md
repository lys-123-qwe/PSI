# sources/ — 指向老师给的 C++ 源码（目录联接，不是副本）

## myPSI-Qin-3（论文 1，CCS'23 pTPSI）

- `frontend/`  -> 桌面 myPSI-Qin-3\frontend
- `libPSI/`    -> 桌面 myPSI-Qin-3\libPSI
- `CMakeLists.txt` 是复制的一份，只用于看构建配置

## libOTe（编译 myPSI-Qin-3 的必备同级依赖）

- `cryptoTools/` -> 桌面 0915\libPSI\out\libOTe\cryptoTools
- `libOTe/`      -> 桌面 0915\libPSI\out\libOTe\libOTe
- `CMakeLists.txt` 是复制的一份

版本 1.6.0。那棵 libOTe 总共 2.2 GB / 9.7 万文件（含整个 Boost 1.86 的 `out/`），
所以只挂了两个源码目录，`out/` 没挂。

## 注意

改这里的文件 = 改桌面上的原文件。IDEA 里能直接看、能搜索，
但不要在这里编译；编译请在 Linux 上跑 `demo/scripts/setup_deps_linux.sh` 和
`demo/scripts/bootstrap_linux.sh`。

删掉 `sources/` 不会影响原代码。