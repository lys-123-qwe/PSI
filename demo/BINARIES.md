# Linux 编译产物说明

真实协议和论文 GS 基准都是 Linux x86-64 ELF，不能在 Windows 上直接运行。
仓库只保存源码；二进制需要时从服务器取回或在 Linux 上重新编译。

## 本地副本（不提交 Git）

为了在 IDEA 工程里直接查看/拷贝，以下文件已从服务器 `vm-lys` 复制到
`demo/bin/`，该目录已在 `.gitignore` 中忽略：

| 文件 | 来源 | 大小 | MD5 |
|---|---|---:|---|
| `demo/bin/gs20` | `/home/lys/psi-demo/paper-gs/gs20` | 69,848 B | `bc71efbce044b0de60e6cdf9e525a6bd` |
| `demo/bin/myPSI-frontend-patched` | `/home/lys/psi-demo/myPSI-frontend-patched` | 20,716,168 B | `9fc3b912dff86d2ebaf29482dc48c7f4` |

这两个文件只能在 Linux x86-64 上执行；Windows 的 IDEA 只能查看为二进制文件，
双击会报“版本不兼容”。真实模式仍然通过 `remote-backend.txt` 转发到 Linux。

## 服务器上的规范位置

| 产物 | 服务器路径 |
|---|---|
| `gs20` | `/home/lys/psi-demo/paper-gs/gs20` |
| `myPSI-frontend-patched` | `/home/lys/psi-demo/myPSI-frontend-patched` |
| 构建输出（同 MD5） | `/home/lys/psi-demo/mypsi-src/bin/frontend.exe` |
| CMake 构建目录 | `/home/lys/psi-demo/build-linux` |
| patched 源码 | `/home/lys/psi-demo/mypsi-src` |

## 重新编译

```bash
# GS 基准
cd ~/psi-demo/paper-gs
g++ -O3 -fopenmp gs20.cpp -o gs20 -lntl -lgmp -lgf2x

# myPSI patched
bash ~/psi-demo/demo/scripts/build_patched_linux.sh \
  --src ~/psi-demo/mypsi-src \
  --build-dir ~/psi-demo/build-linux \
  --jobs 32
cp ~/psi-demo/mypsi-src/bin/frontend.exe ~/psi-demo/myPSI-frontend-patched
```

## 校验

```bash
md5sum ~/psi-demo/paper-gs/gs20 ~/psi-demo/myPSI-frontend-patched
```