# TASK-8 整机镜像集成验证汇总（x86_64_virt）

**结论：** 本轮 `x86_64_virt` 完整镜像（`system.img` 18:18 重建）内已经是**拆分后的产物**：
`/system/lib64/platformsdk/libipc_single.z.so`（371,584 B）与同目录 `libipc_rpc.z.so`（177,304 B）与构建产物
**md5 逐条一致**；依赖方向为 `libdbinder → libipc_rpc → libipc_single`，核心库 `DT_NEEDED` 中**没有**反向依赖。
在**不推送任何核心库**的干净实例上：真实 `dlopen` 探针确认 RPC 库按核心库同目录解析为
`/system/lib64/platformsdk/libipc_rpc.z.so`；7 个测试目标合计 **282 例 + 79 例**在镜像库口径下通过。
至此 TASK-7 遗留的「整机镜像集成未验证」关闭。

## 1. 口径

| 项 | 值 |
|----|-----|
| 产品/镜像 | `x86_64_virt`，`out/x86_64_virt/packages/phone/images`（全量重建，`build success`，1:40:07） |
| 模拟器 | `qemu-system-x86_64` 6.2.0 + KVM，实例 00，`--reset-before-run`（base image 变了，必须重开) |
| 客机 | `Linux localhost 6.6.101-… x86_64`，`OpenHarmony 7.1.0.25`，`bootevent.boot.completed=true` |
| 被测库 | **镜像内** `/system/lib64/platformsdk/{libipc_single,libipc_rpc,libipc_common}.z.so`、`/system/lib64/libdbinder.z.so` |
| 唯一推送件 | 7 个测试可执行 + `libipc_single_test.z.so`（测试桩库）+ `realdl` 探针 |
| 库搜索顺序 | `$R:/system/lib64/chipset-sdk-sp:/system/lib64/ndk:/system/lib64/platformsdk:/system/lib64`，`$R` 内无核心库 |

指纹见 `manifest.txt`，原始信息见 `raw/device_info.txt`、`raw/elf.txt`。

## 2. 镜像内容与依赖方向

| 库 | 字节 | md5 | 与构建产物 |
|----|-----:|-----|:----------:|
| `libipc_single.z.so` | 371,584 | `b0b3dc07…` | 一致 |
| `libipc_rpc.z.so` | 177,304 | `0ae4812c…` | 一致 |
| `libipc_common.z.so` | 75,192 | `6b273e87…` | 一致 |
| `libdbinder.z.so` | 166,624 | `68c4062a…` | 一致 |

- `libipc_single` NEEDED：`libutils libipc_common libsec_shared libbacktrace_local libffrt libhilog libhitracechain libselinux libhisysevent libc libc++ …`，**无 `libipc_rpc`**；导出 235。
- `libipc_rpc` NEEDED：`libutils libipc_single libipc_common …`；导出 11。
- `libdbinder` NEEDED：同时含 `libipc_single` 与 `libipc_rpc`。

对照拆分前基线（TASK-7 时的镜像）：`libipc_single` 为 492,056 B / `d2ee5266…`，且镜像中不存在 `libipc_rpc.z.so`。

## 3. 真实 `dlopen`（AC-1.1 / AC-5.1 运行期、镜像口径）

| 模式 | `RPC_MAPPED_BEFORE` | `RPC_MAPPED_AFTER` | 映射来源 | `INVOKER_FIRST` | 缓存复用 | ΔPSS (KB) | 结果 |
|------|--------------------:|-------------------:|----------|-----------------|:--------:|----------:|------|
| `ipc`（`IF_PROT_DEFAULT`） | 0 | **0** | — | 非空 | 1 | +267 | **PASS** |
| `rpc`（`IF_PROT_DATABUS`） | 0 | **1** | `/system/lib64/platformsdk/libipc_rpc.z.so` | 非空 | 1 | +279 | **PASS** |
| `rpc`（目录缺 `libipc_rpc`） | 0 | **0** | — | **空** | — | +588 | 降级正确 |

RPC 模式的 `maps` 命中 4 段（`r--p`/`r-xp`/`r--p`/`rw-p`），**路径是镜像内目录**，且 `inode 5033` 与
`system.img` 中 `libipc_rpc.z.so` 的 inode（`raw/elf.txt`）一致 —— 这排除了「跑到推送副本」的可能，
证明加载器按「自身 so 所在目录」解析生效。

> **PSS 口径提醒：** 本轮 ΔPSS 明显小于 TASK-7（662/831 KB），原因是 TASK-7 测的是 `/data/local/tmp` 下单映射的推送副本，
> 而镜像库被 init/samgr 等众多常驻进程共享，`smaps_rollup` 的 PSS 会把共享物理页按映射进程数摊分。
> 因此在整机环境里 PSS 增量**不适合**作为「省了多少」的估算；可信的运行期结论是「IPC-only 进程对 `libipc_rpc`
> 的映射段数 = 0（整库不加载）」。内存量级仍以 TASK-6 的静态 ALLOC 口径为准。

## 4. 测试目标结果（镜像库口径，无核心库推送）

| 目标 | 用例数 | 结果 | 原始记录 |
|------|-------:|------|----------|
| `IpcRpcLoaderUnitTest` | 16 | **16/16 PASS** | `raw/IpcRpcLoaderUnitTest.xml` |
| `IpcRpcStaticLinkUnitTest` | 4 | **4/4 PASS** | `raw/IpcRpcStaticLinkUnitTest.xml` |
| `IpcRpcStaticLinkFailureUnitTest` | 1 | **1/1 PASS** | `raw/IpcRpcStaticLinkFailureUnitTest.xml` |
| `IPCProcessSkeletonUnitTest` | 80 | **80/80 PASS** | `raw/ps_1.xml`、`raw/ps_1.log` |
| `IPCObjectProxyUnitTest` | 64 | **64/64 PASS** | `raw/IPCObjectProxyUnitTest.xml` |
| `DbinderDataBusInvokerUnitTest` | 117 | **117/117 PASS** | `raw/DbinderDataBusInvokerUnitTest.xml` |
| `IpcObjectStubUnitTest` | 79 | **79/79 PASS**（3 次中 2 次通过，1 次既有偶发 abort） | `raw/stub_1.xml`、`raw/stub_1.log` |

合计 **282 例**（前 6 个目标）+ **79 例**（第 7 个目标）在镜像库上通过。

### 既有偶发项（与本改动无关）

| 目标 | 现象 | 归因 | 记录 |
|------|------|------|------|
| `IPCProcessSkeletonUnitTest` | 首轮 `EXIT=134`，停在第 15 例 `GetRegistryObjectTest001`，gmock 报 `Name() must not be called before SetOwnerAndName()` | 与 TASK-7 观察到的 gmock/线程竞态同一类；同二进制随后连续 2 次 80/80 通过 | `raw/IPCProcessSkeletonUnitTest_first_abort.log` |
| `IpcObjectStubUnitTest` | 3 次中 1 次 `EXIT=134`（停在 `DBinderDumpTransactionTest001`），另 2 次 79/79 | TASK-7 已归因到「真实 binder 驱动下测试桩跨线程竞态」，10 次中 9 次通过 | `raw/IpcObjectStubUnitTest_flaky_abort.log` |

两项目标在本轮镜像口径下**均能通过**，且失败形态与拆分前一致，不属于本变更引入的回归；但复跑时需接受偶发重试。

## 5. 加载失败降级（AC-3.1 镜像口径）

`/data/local/tmp/imgverify/ipc_only_dir/` 只放核心库副本、不放 `libipc_rpc.z.so`：

- `INVOKER_FIRST=0`、`INVOKER_SECOND=0`（`call_once` 缓存空表，不重试）；
- `RPC_MAPPED_AFTER=0`，进程不崩溃；
- hilog **仅 1 条**：

```
E C057c1/IpcRpcLoader: LoadOnce: load /data/local/tmp/imgverify/ipc_only_dir/libipc_rpc.z.so failed, reason:
dlopen failed: Error loading shared library /data/local/tmp/imgverify/ipc_only_dir/libipc_rpc.z.so: No such file or directory
```

## 6. 与 TASK-7 的关系

| 项 | TASK-7 | TASK-8（本目录） |
|----|--------|------------------|
| 被测库来源 | 推送 + `LD_LIBRARY_PATH` 优先 | **镜像内 `/system/lib64`** |
| 镜像是否含拆分件 | 否（基线 492,056 B） | **是（371,584 B + `libipc_rpc` 177,304 B）** |
| RPC 库解析路径 | `/data/local/tmp/ipc_rpc_test/…` | `/system/lib64/platformsdk/…` |
| 用例结论 | 282 + 79 通过 | 282 + 79 通过（同集合复现） |

TASK-7 的证据仍有效（它证明「库本身在设备栈上行为正确」），TASK-8 补的是「**装进镜像后仍然正确**」。

## 7. 未验证项

- 真实系统服务进程（`samgr`、`softbus_server`）的 PSS；
- IPC 单次调用时延对比（AC-5.1）；
- 关闭 `ipc_feature_rpc_split_enabled` 的整机镜像；
- 双端跨设备（DBinder 真实远端）死亡通知与权限链路（AC-2.1/AC-2.2）；
- **arm64 整机镜像与设备侧运行**——已由 `../task-9-arm64-image/` 关闭（TASK-9：arm64 全量镜像 `EXIT=0`、四库 md5 一致、361/361 通过）。