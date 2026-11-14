# TASK-9 整机镜像集成验证（arm64_virt）：复现方法

本目录把 TASK-8 在 `x86_64_virt` 上已完成的「镜像内零推送库」口径复制到 **AArch64** 目标，
用于证明拆分不是 x86 专属：arm64 全量镜像内同样是 `libipc_single` + 同目录 `libipc_rpc`，
依赖方向 `libdbinder → libipc_rpc → libipc_single`，且 IPC-only 路径不映射 rpc 库。

## 1. 环境（与变更无关的仓外补丁，必须披露）

本容器内 `arm64_virt` 全量构建需要三处**仓外环境补丁**，均不影响 `foundation/communication/ipc` 的产物：

| # | 位置 | 补丁 | 原因 |
|---|------|------|------|
| 1 | `vendor/ohemu/arm64_virt` | 符号链接到 `virt` | 产品目录名与 vendor 路径不一致 |
| 2 | `device/qemu/arm_virt/linux_full/bundle.json` | 补 `bounds_checking_function`、`openmax`、`libjpeg-turbo` | 该产品 bundle 缺少依赖声明 |
| 3 | `drivers/hdf_core/adapter/khdf/linux/manager/Makefile:73` | `KBUILD_CFLAGS += -mno-outline-atomics` → `ccflags-y += $(call cc-option,-mno-outline-atomics)` | 预置交叉 gcc 为 Linaro 7.5.0，该选项需 gcc ≥ 10，内核编译直接报 `unrecognized command line option` |

第 3 项是本轮新加的：它是**内核侧**编译选项探测问题，`drivers/hdf_core` 不在本次变更范围，
也不会进入 PR；不改则 arm64 内核无法在本环境编译。这里不能写 `KBUILD_CFLAGS += $(call cc-option,…)`——
`cc-option` 内部会展开 `KBUILD_CFLAGS`，`+=` 的递归展开会造成自引用
（`scripts/Makefile.lib:10: *** Recursive variable 'KBUILD_CFLAGS' references itself`）；
改用按目录生效的 `ccflags-y` 后，`drivers/hdf/khdf/manager` 单独 `make` 已能编出 `built-in.a`。

## 2. 构建

```sh
cd <code>
# 容器 cgroup 内存上限 48.8 GB，默认按 88 核并发会被 OOM kill（ret_code -9），必须限并发
./build.sh --product-name arm64_virt --ccache --deps-guard=false --ninja-args=-j24
```

## 3. 镜像内核抽库与比对

```sh
/tmp/opencode/arm64_verify/extract_and_compare.sh    # debugfs dump + md5/ELF/NEEDED/导出数/inode
/tmp/opencode/arm64_verify/build_tests.sh            # 增量构建 7 个测试目标 + ipc_single_test
```

## 4. 模拟器与设备侧

AArch64 的 `port_slot = 1`，实例 `00` 的 HDC 端口为 **5655**；宿主为 x86_64，arm64 客机走 TCG，启动明显慢于 KVM。

```sh
/tmp/opencode/arm64_verify/start_emulator.sh         # qemu_run.sh run --instance 00 --reset-before-run + 等 boot
/tmp/opencode/arm64_verify/run_device.sh             # 推送探针与测试二进制，核心库一律用镜像内
```

`run_device.sh` 的推送内容与 TASK-8 一致：**不推送任何核心库**，只推 `realdl`、`libipc_single_test.z.so`
与 7 个测试可执行，随后用 `ls`/`md5sum` 自证 `/data/local/tmp/imgverify` 内无核心库副本。

库搜索路径：

```sh
export LD_LIBRARY_PATH=$R:/system/lib64/chipset-sdk-sp:/system/lib64/ndk:/system/lib64/platformsdk:/system/lib64
```