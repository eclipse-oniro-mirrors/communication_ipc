# TASK-9 整机镜像集成验证汇总（arm64_virt）

**结论：** 本轮 `arm64_virt` 完整镜像（`system.img` 20:51 重建，2,147,483,648 B）内已经是**拆分后的产物**：
`/system/lib64/platformsdk/libipc_single.z.so`（359,560 B）与同目录 `libipc_rpc.z.so`（169,544 B）与构建产物
**md5 逐条一致**；依赖方向为 `libdbinder → libipc_rpc → libipc_single`，核心库 `DT_NEEDED` 中**没有**反向依赖。
在**不推送任何核心库**的干净实例上：真实 `dlopen` 探针确认 RPC 库按核心库同目录解析为
`/system/lib64/platformsdk/libipc_rpc.z.so`（inode 4987 与镜像内一致）；7 个测试目标合计 **361 例全部通过**。
至此 TASK-8 遗留的「arm64 整机镜像与设备侧运行」未验证项关闭。

## 1. 口径

| 项 | 值 |
|----|-----|
| 产品/镜像 | `arm64_virt`，`out/arm64_virt/packages/phone/images`（全量重建，`build success`，Cost Time 1:43:32） |
| 模拟器 | `qemu-system-aarch64` 6.2.0，`-accel tcg,thread=multi -cpu cortex-a57 -smp 4 -m 4096`，实例 00，`--reset-before-run` |
| HDC | AArch64 `port_slot=1` → 实例 00 = `127.0.0.1:5655` |
| 客机 | `Linux localhost 6.6.101-gc0c10d57367a-dirty #1 SMP PREEMPT … aarch64`，`OpenHarmony 7.1.0.25`，`bootevent.boot.completed=true` |
| 被测库 | **镜像内** `/system/lib64/platformsdk/{libipc_single,libipc_rpc,libipc_common}.z.so`、`/system/lib64/libdbinder.z.so` |
| 唯一推送件 | 7 个测试可执行 + `libipc_single_test.z.so`（测试桩库）+ `realdl` 探针，全部落在 `/data/local/tmp/imgverify/` |
| 构建并发 | `--ninja-args=-j24`（容器 cgroup 内存上限 48.8 GB，默认 88 路并发会被 OOM kill） |

**零推送自证：** `raw/device_info.txt` 的 `ls -l $R` 只列出 9 个推送文件；
`ls $R | grep -E 'libipc_single.z.so|libipc_rpc.z.so|libipc_common.z.so|libdbinder.z.so'` 输出 `NONE`；
`sbinfo`/`maps` 中库路径均为 `/system/lib64/...`，没有 `/data/local/tmp` 副本。

## 2. 镜像内库指纹

四库 md5 与 `out/arm64_virt/communication/ipc/` 产物逐条一致，详见 `manifest.txt`、`raw/elf.txt`：

| 库 | 字节 | md5 | 镜像 inode | 导出定义符号 |
| --- | ---: | --- | ---: | ---: |
| `libipc_single.z.so` | 359,560 | `048197efed141efff0bc720000bad07f` | 4988 | 235 |
| `libipc_rpc.z.so` | 169,544 | `4fbf631d0a1b5146debc6da6941c1ee7` | 4987 | 11 |
| `libipc_common.z.so` | 71,280 | `0d966297efcccd2334b29595fd0a2608` | 4984 | 78 |
| `libdbinder.z.so` | 158,472 | `0c6734f9ad9ba0840c0664140d71c07b` | 2768 | 183 |

设备侧 `md5sum` 复核（`raw/device_info.txt`）与上表完全一致。

## 3. 依赖方向与符号边界（镜像内副本）

| 检查 | 结果 |
| --- | --- |
| `libipc_single` NEEDED | `libutils/libipc_common/libsec_shared/libbacktrace_local/libffrt/libhilog/libhitracechain/libselinux/libhisysevent/libc/libc++/libclang_rt.ubsan_minimal`；**无 `libipc_rpc`** |
| `libipc_single` 符号 | `IpcRpc*` 定义符号 0 个、未定义引用 0 个；`DBinderSoftbusClient`/`DBinderDatabusInvoker` 导入 0 个 |
| `libipc_rpc` NEEDED | 含 `libipc_single`、`libipc_common` 等；导出 11 个：`IpcRpcGetOps` + `DBinderSoftbusClient` 8 个 + `DBinderDatabusInvoker` C1/C2 |
| `libdbinder` NEEDED | 同时含 `libipc_rpc` 与 `libipc_single`；从 rpc 库导入 8 个 `DBinderSoftbusClient` 符号 |
| 硬化属性 | 两库均 `BIND_NOW` + `FLAGS_1 NOW` + `GNU_RELRO`，无 `TEXTREL`，**无 `RPATH`/`RUNPATH`** |

无 `RPATH/RUNPATH` 说明运行期解析不是靠搜索路径，而是加载器用「核心库自身所在目录」拼绝对路径 `dlopen`。

## 4. 运行期探针（零推送核心库）

| 场景 | 证据 | 结果 |
| --- | --- | --- |
| 仅 IPC（`IF_PROT_DEFAULT`） | `raw/realdl_ipc.txt` | `RPC_MAPPED_AFTER=0`、两次 invoker 同指针非空、`RESULT=PASS` |
| 首次 RPC（`IF_PROT_DATABUS`） | `raw/realdl_rpc.txt` | 4 段 `MAPS_HIT` 全为 `/system/lib64/platformsdk/libipc_rpc.z.so`、inode **4987** 与镜像一致、二次同指针、`RESULT=PASS` |
| 同目录缺 rpc 库 | `raw/realdl_rpc_missing_lib.txt`、`raw/realdl_rpc_missing_lib_hilog.txt` | 两次 invoker 均为 `0`、`RPC_MAPPED_AFTER=0`、不崩溃；hilog **恰好 1 条** `IpcRpcLoader: LoadOnce: load … failed, reason: dlopen failed: … No such file or directory` |

> 缺库场景的 `RESULT=FAIL` 是探针的**设计输出**：`rpc` 模式的判定要求 invoker 非空且已映射 rpc 库，
> 而本场景刻意让库缺失。x86（`../task-8-image/raw/realdl_rpc_missing_lib.txt`）同样是 `RESULT=FAIL`，
> 判定依据是「两次 `nullptr` + 0 映射 + 单条降级日志」。

## 5. 测试矩阵

| 目标 | 用例数 | 结果 | 证据 |
| --- | ---: | --- | --- |
| `IpcRpcLoaderUnitTest` | 16 | **16/16 PASS** | `raw/IpcRpcLoaderUnitTest.xml` |
| `IpcRpcStaticLinkUnitTest` | 4 | **4/4 PASS** | `raw/IpcRpcStaticLinkUnitTest.xml` |
| `IpcRpcStaticLinkFailureUnitTest` | 1 | **1/1 PASS** | `raw/IpcRpcStaticLinkFailureUnitTest.xml` |
| `IPCProcessSkeletonUnitTest` | 80 | **80/80 PASS** | `raw/IPCProcessSkeletonUnitTest.xml` |
| `IPCObjectProxyUnitTest` | 64 | **64/64 PASS** | `raw/IPCObjectProxyUnitTest.xml` |
| `DbinderDataBusInvokerUnitTest` | 117 | **117/117 PASS** | `raw/DbinderDataBusInvokerUnitTest.xml` |
| `IpcObjectStubUnitTest` | 79 | **79/79 PASS** | `raw/IpcObjectStubUnitTest.xml` |
| **合计** | **361** | **361/361 PASS，0 失败** | 7 个 `.log` 均为 `EXIT=0` |

与 x86（TASK-8）不同，本轮 `IPCProcessSkeletonUnitTest` 与 `IpcObjectStubUnitTest` **均一次通过**，
未出现那两个既有偶发 abort。

## 6. 与 TASK-8（x86_64_virt）的一致性

| 维度 | x86_64_virt（TASK-8） | arm64_virt（本 Task） |
| --- | --- | --- |
| 拆分件是否在镜像内 | 是（single 371,584 B + rpc 177,304 B） | 是（single 359,560 B + rpc 169,544 B） |
| 依赖方向 | `dbinder → rpc → single`，无反向 NEEDED | 完全相同 |
| 核心库 `IpcRpc` 符号 | 0 | 0 |
| 探针 IPC 模式映射段数 | 0 | 0 |
| 探针 RPC 模式映射路径 | `/system/lib64/platformsdk/libipc_rpc.z.so`，inode 5033 | 同路径，inode 4987 |
| 测试用例 | 282 + 79 | 361 |
| 偶发 abort | 2 项各出现 1 次 | 未出现 |

## 7. PSS 口径（沿用 T8-02 纠偏）

探针进程的 `Pss` 增量：IPC 模式 **279 KB**、RPC 模式 **295 KB**，差 **16 KB**。
这与 x86 的 267/279（差 12 KB）同样**不能**用来估算 rpc 库的真实占用：镜像内的 `libipc_single` 已被大量常驻进程映射，
文件页的 PSS 按映射进程数摊分，单进程读数被严重压低。arm64 的 rpc 库实际文件体积是 169,544 B。
**收益判据仍为「IPC-only 进程对 `libipc_rpc` 的映射段数 = 0」**，本 Task 已满足。

## 8. 本环境的仓外补丁（与变更无关，需披露）

arm64 全量构建在本容器需要三处**仓外**补丁，均不改变 `foundation/communication/ipc` 产物，也不进 PR：

1. `vendor/ohemu/arm64_virt` → `virt` 符号链接（产品目录名与 vendor 路径不一致）。
2. `device/qemu/arm_virt/linux_full/bundle.json` 补 `bounds_checking_function`、`openmax`、`libjpeg-turbo`。
3. `drivers/hdf_core/adapter/khdf/linux/manager/Makefile`：`KBUILD_CFLAGS += -mno-outline-atomics` →
   `ccflags-y += $(call cc-option,-mno-outline-atomics)`。预置交叉 gcc 为 Linaro 7.5.0（该选项需 ≥ 10）；
   不能写 `KBUILD_CFLAGS += $(call cc-option,…)`，否则 `cc-option` 展开 `KBUILD_CFLAGS` 造成自引用
   （`scripts/Makefile.lib:10: *** Recursive variable 'KBUILD_CFLAGS' references itself`）。

前两次构建失败与此相关：第 1 次是 cgroup 48.8 GB 内存上限被 88 路并发打满（ninja `ret_code: -9`），
第 2 次是上述 gcc 选项不识别；第 3 次才 `EXIT=0`。

## 9. 未验证项

- 真实系统服务进程（`samgr`、`softbus_server`）的 PSS；
- IPC 单次调用时延对比（AC-5.1）；
- 关闭 `ipc_feature_rpc_split_enabled` 的整机镜像；
- 双端跨设备（DBinder 真实远端）死亡通知与权限链路（AC-2.1/AC-2.2）；
- arm64 在真机（非 TCG 模拟）上的行为与性能。