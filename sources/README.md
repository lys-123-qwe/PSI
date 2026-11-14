# sources/ — 老师给的 C++ 源码与 IDEA 索引副本

## myPSI-Qin-3（论文 1，CCS'23 pTPSI）

- `frontend/`、`libPSI/`：**项目内真实副本**，IDEA 可以直接展开、搜索、跳转。
- `frontend_link/`、`libPSI_link/`：指向桌面原始源码的目录联接，保留作为对照。
- `CMakeLists.txt`：构建配置副本。

IDEA 模块 `myPSI-Qin-3` 使用 `frontend/` 和 `libPSI/` 这两个真实目录；`*_link/` 已从模块索引中排除。

## libOTe（编译 myPSI-Qin-3 的必备同级依赖）

- `cryptoTools/` -> 桌面 `0915\libPSI\out\libOTe\cryptoTools`
- `libOTe/`      -> 桌面 `0915\libPSI\out\libOTe\libOTe`
- `CMakeLists.txt` 是复制的一份

版本 1.6.0。那棵 libOTe 总共 2.2 GB / 9.7 万文件（含整个 Boost 1.86 的 `out/`），
所以只挂了两个源码目录，`out/` 没挂。为了不让 IDEA 卡顿，`sources/libOTe` 没有加入项目索引。

## 同步注意

- 修改 `frontend/`、`libPSI/` 只影响项目内副本，不会改桌面原文件。
- 修改 `frontend_link/`、`libPSI_link/` 才会影响桌面原始源码。
- 如果要让项目内副本跟随桌面源码更新，重新复制 `frontend_link` / `libPSI_link` 的内容即可。
- 不要在这里编译；编译请在 Linux 上跑 `demo/scripts/setup_deps_linux.sh` 和
  `demo/scripts/bootstrap_linux.sh`。
