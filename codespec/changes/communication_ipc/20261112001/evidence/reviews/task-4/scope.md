---
task_id: TASK-4
risk: R2
seat: scope
verdict: PASS
blocking_findings: 0
---

# Task Review — 漏改/完整性（③）与外部接口影响（④）

## 结论

- **verdict: PASS**，**blocking_findings: 0**。
- 开关覆盖完整：`ipc_feature_rpc_split_enabled` 只出现在 **6 个构建文件、9 处命中**（`config.gni` + 5 个 `BUILD.gn`），`*.json`/`*.py`/`*.sh`/`*.cpp`/`*.h` **零命中**；4 个使用点所在 `BUILD.gn` 全部 `import(config.gni)`，不存在「用了参数却没引入」的隐式失效。目标定义、`sources`、`defines`、deps 四种 gate 双向对称，两侧都没有单边 gate。
- 关闭态产物（`/tmp/opencode/split_off_*`，11-13 01:16 冻结）相对 pre-change 基线：**`DT_NEEDED` 12 条 diff 为空**、无 `libipc_rpc` 字符串残留（0 命中）、只剩 `<static link>` 一条加载文案、`IpcRpcGetOps` 无未定义引用且不导出（`local: *` 收口）。AC-4.1 的「产物依赖结构与拆分前一致」在产物上成立。
- 关闭态导出面 **167 → 245（+78 / −0）**，未退回 167，与 `BR-6`/`EX-5` 的「产物与拆分前一致」字面不符，登记为 **S-01（Medium，非阻塞）**；该 78 条分块属 TASK-3 的 D-04 设计（版本脚本无法按开关分支），不是 TASK-4 的越界改动。
- 测试装配缺 D-07 要求（`ipc_single_test`/`ipc_single_test_static` 无 entry TU、无 `IPC_RPC_STATIC_LINK`）登记为 **S-02（Medium，非阻塞）**：TASK-5 Files 表明确承接，作者已在 Actual Result 登记批次约束。若评审门禁要求 TASK-4 **独立**满足 AC-4.1 的「全部用例通过」，该项须升级为阻塞。
- 外部接口面零影响：公开头文件、NAPI、C API、ANI、Taihe、Rust 零改动；transaction code、错误码、ioctl 零新增（AGENTS.md 的 ArkWeb seccomp 同步条款未触发）；`bundle.json` 未改。

## 维度结论

| 维度 | 结论 | 摘要 |
|------|------|------|
| ③ 漏改/完整性 | **通过**（2 Medium + 2 Low，非阻塞） | 生产侧无漏改；开关只落在 6 个构建文件，未渗入源码；关闭态 NEEDED 与基线逐条一致，`libdbinder` 8 个强未定义符号在核心库内可解析。关闭态导出面未回退（S-01）、测试装配未跟上（S-02）均登记非阻塞。 |
| ④ 外部接口影响 | **通过**（1 Low） | 公开头/NAPI/C API/ArkTS 声明零改动；无新 transaction code、错误码、ioctl；`bundle.json` 未改；新库安装标签为 `platformsdk_indirect`（TASK-3 已修正）。唯一遗留是私有 loader/ops 头位于对外暴露的 include 目录（S-05）。 |
| 漏改计数 | **0（生产）/ 1 类（测试装配，S-02，由 TASK-5 承接）** | 无 Files 表外的 TASK-4 改动。 |

## 独立核验记录

### 证据基线

| 项 | 值 |
|----|----|
| 仓根 | `code/foundation/communication/ipc` |
| 关闭态产物 | `/tmp/opencode/split_off_libipc_single.z.so`（488,464 B @11-13 01:16）、`split_off_libdbinder.z.so`（166,600 B） |
| 拆分前基线 | `code/out/x86_64_virt/packages/phone/system/lib64/platformsdk/libipc_single.z.so`（492,056 B @11-12 01:29，pre-change 归档） |
| 开启态导出 | `/tmp/opencode/cur_single.txt`（235 行，全部 `@@1.0`；交叉核对用） |
| 工具 | `code/prebuilts/clang/ohos/linux-x86_64/llvm/bin/{llvm-nm,llvm-readelf}` |
| 约束 | 未执行 `./build.sh`；未执行任何改写工作区的 git 命令（无 add/commit/checkout）；`out/` 当前构建态（正被后台重链）不作为证据 |

### R-1 开关覆盖与构建装配

| 检查 | 结论 | 证据 |
|------|------|------|
| 开关定义与默认值 | `true`，带两形态注释 | `config.gni:20-25`（`declare_args()` 内 `:25` 赋值） |
| GN 引用点 | 9 处 / 6 文件，全部构建层 | `config.gni:25`；`BUILD.gn:30`；`ipc_core/BUILD.gn:73-75(注释)+76`；`ipc_single/BUILD.gn:122/125-130(注释)/141/188`；`libdbinder/BUILD.gn:87` |
| 非构建文件渗入 | 0 | `grep -rln ipc_feature_rpc_split_enabled --include=*.json --include=*.py --include=*.sh` = 0；`--include=*.cpp --include=*.h` = 0 |
| import 完整性 | 4/4 | 根 `BUILD.gn:16`、`ipc_core:15`、`ipc_single:15`、`libdbinder:15` 均 import `config.gni` |
| 关闭态 sources | `common_source + ipc_rpc_entry.cpp` | `ipc_single/BUILD.gn:126-130`；`common_source = core_source + rpc_source`（`:88`），已含 4 个 TU 与 loader（`:61`） |
| 关闭态 defines | 仅关闭态追加 `IPC_RPC_STATIC_LINK` | `ipc_single/BUILD.gn:141-143` |
| 目标定义 gate | 3 处，方向一致 | `ipc_single/BUILD.gn:188` 包 `ipc_rpc`；`ipc_core/BUILD.gn:76` 包 `ipc_rpc_core`；`BUILD.gn:30` 包 `:ipc_rpc_core` |
| deps gate | 对称，关闭态回落到 `ipc_core` | `libdbinder/BUILD.gn:83-93` |
| 仓外目标名引用 | 0 | `grep -rn ipc_rpc_core` 仅 2 处（定义 + 根 deps）；`build/ vendor/ productdefine/ device/` 无命中 |
| 同名干扰 | 已排除 | `ipc_rpc_interface`（`c/dbinder`、`c/ipc` 的 config 名）、`ipc_rpcfeatureset_fuzztest`（fuzz group）与 `:ipc_rpc` 目标无关 |
| Anti-Fake「不得靠 #ifdef 删实现」 | 未违反 | 关闭态 `sources` 全量编入 4 TU + entry + loader；`:93-263` 的 `#ifndef IPC_RPC_STATIC_LINK` 收掉的是 `dlopen`/路径解析/`dlsym`/映像校验，`:272-347` 的静态分支只切换取表方式，两者都不含 RPC 实现 |

### R-2 关闭态导出面与依赖结构（产物实测）

| 检查 | 结论 | 证据 |
|------|------|------|
| 导出总数 | 167 → **245** | 基线 `llvm-nm -D --defined-only` = 167；`split_off_libipc_single.z.so` = 245 |
| 差集 | **+78 / −0** | `comm` 比对 → `added78.txt` 78 行，丢失 0 |
| `DT_NEEDED` | **12 条，diff 为空** | 两侧 `llvm-readelf -d \| grep NEEDED` 各 12 条，`diff n_base.txt n_off.txt` 无输出；关闭态 `grep -c ipc_rpc` = **0** |
| `libdbinder` 关闭态 | 9 条 NEEDED，无 rpc | `grep -c ipc_rpc` = 0；`llvm-nm -D -u` 中 `DBinderSoftbusClient\|DBinderDatabusInvoker` = **8 条**，全部能在关闭态核心库解析（核心库导出 10 条 = 8 + `DBinderDatabusInvoker` C1/C2） |
| 无加载路径残留 | 是 | `strings \| grep -c libipc_rpc` = **0**；`grep -c '<static link>'` = **1**；`llvm-nm -D -u \| grep -c IpcRpcGetOps` = **0** |
| `IpcRpcGetOps` 可见性 | 不导出（正确形态） | 关闭态 `llvm-nm -D --defined-only \| grep -c IpcRpcGetOps` = 0；`IpcRpcOps*` 导出 0；`local: *` 保留；能力表由 TU 内直接调用（`ipc_rpc_loader.cpp:276`） |
| 78 条构成 | 7 个私有类，无 vtable/typeinfo/thunk/guard | IPCProcessSkeleton 39、DBinderSessionObject 17、BufferObject 7、InvokerRawData 6、HitraceInvoker 4、IPCObjectProxy 3、MessageParcel 2 = 78；`grep -c 'vtable\|typeinfo\|thunk\|guard variable'` = 0 |
| 与公开头交集 | 5 条 | `message_parcel.h:114,121`；`ipc_object_proxy.h:210,234,242`（`ClearDBinderServiceState` 在 `#ifndef CONFIG_IPC_SINGLE` 内，该宏无任何构建文件定义，声明生效） |
| 两形态导出面 | 同集合，差异仅 10 条迁出符号 | 开启态核心库 235 = 167 − 10 + 78，`libipc_rpc` 11 条；关闭态 245 = 167 + 78 |
| 是否需要拆版本脚本 | 不需要 | 78 条全部进既有 `1.0` 节点；`ipc_rpc_ops.h:48-52` 已记录「版本节点隔离被否决」的理由；`libipc_rpc_map` 依赖 `@@1.0` |

### R-3 变更集归属与 Files 表一致性

| Files 表行 | 实际（`git diff --numstat` / 文件行） | 结论 |
|------------|--------------------------------------|------|
| `config.gni` 新增开关 | +5 | 一致 |
| 根 `BUILD.gn` dep 加 gate | +3 | 一致 |
| `ipc_core/BUILD.gn` 目标 gate | +40（含 TASK-3 的 `ipc_rpc_core` 定义与注释） | 一致（累计态） |
| `ipc_single/BUILD.gn` if/else + 宏 + rpc gate | +100 / −6 | 一致 |
| `libdbinder/BUILD.gn` deps gate | +7 | 一致 |
| `libipc_single_map` **仅注释** | +81 = 5 行注释（hunk `@@ -17,4 +17,9 @@`）+ 76 行 TASK-3 符号块（`@@ -199,4 +204,80 @@`） | 一致，导出条目零增删 |
| `ipc_rpc_loader.cpp` 静态分支 | `:272-283` 静态、`:93-263` 动态包在 `#ifndef`、`:366` 双宏门控 | 一致 |
| `ipc_rpc_ops.h` 入口声明 | `:108` `extern "C" const IpcRpcOps *IpcRpcGetOps();` | 一致 |

工作区累计 23 `M` + 6 `??`，除上表 8 个文件外均可归属：`test/BUILD.gn`(+1)、2 个 fuzz `BUILD.gn`、`ipcprocessskeletonmock_fuzzer.h` 属 TASK-3 计划外补入（TASK-5 Files 表已注明承接）；`test/unittest/ipc/native/core/BUILD.gn`、`ipc_rpc_loader_unittest.cpp` 属 TASK-1；3 个 `*_unittest.cpp` 属 TASK-2；`.gitignore` 属 TASK-1。**未发现 TASK-4 的 Files 表外改动。**

两个 fuzz 目标的关闭态依赖已复核：`dbindergrantpermission_fuzzer/BUILD.gn:33`、`getlocalnodedeviceid_fuzzer/BUILD.gn:33` 均为 `test:ipc_single_test_static`（源集合，非 `:ipc_rpc` 目标），关闭态不会引用未定义目标。

### R-4 外部接口影响（④）

| 检查 | 结论 | 证据 |
|------|------|------|
| 公开头文件 | 零改动 | `git status --short` 无 `interfaces/innerkits/ipc_core/include/`、`c_api/include/`、`libdbinder/include/`、`interfaces/kits/` |
| NAPI / C API / ANI / Taihe / Rust | 零改动 | 同上，相关目录无 `M`/`??` |
| transaction code / 错误码 | 0 新增定义 | 本次 `git diff` 无 `TransactionCode` 新增；新增行仅**复用**既有 `ERR_INVALID_DATA`/`ERR_NONE`（`ipc_rpc_entry.cpp` 能力表校验与权限转发），未定义新错误码 |
| `ioctl` | 0 新增 | `git diff \| grep -c ioctl` = 0 → 不触发 AGENTS.md 的 ArkWeb seccomp 同步条款 |
| `bundle.json` | 未改 | `git status` 无该文件；两处 `header_base` 仍只指向 `ipc_core/include`（`:98`、`:129`） |
| 新库安装标签 | `platformsdk_indirect`（内部 companion） | `obj/.../ipc_rpc_core_module_info.json` → `innerapi_tags:["platformsdk_indirect"]`、`install_enable:true`、`part_name:ipc` |
| 安装路径 | 与 `ipc_core` 完全一致，无悬空引用 | module_info `dest` = `system/lib64/platformsdk/libipc_rpc.z.so` + `updater/...`；`ipc_core/BUILD.gn:54-57` 的 `install_images` 与 `:93-96` 同为 system+updater、`relative_install_dir` 同为 `platformsdk`；关闭态该目标整体不定义（`ipc_core/BUILD.gn:76-102`），且仓内唯一引用（`BUILD.gn:31`）同处 gate 内 |
| `libipc_single_map` 语义 | 未改变导出约束 | `local: *` 仍在文末；`DBinderSoftbusClient` 8 条在关闭态变为有效定义 |

## 发现

### 非阻塞发现

| ID | 严重度 | 类别 | 位置 | 问题与影响 | 证据 | 建议 |
|----|--------|------|------|-----------|------|------|
| S-01 | **Medium** | ③ 完整性 / ④ 接口面 | `interfaces/innerkits/ipc_single/libipc_single_map:207-283`；`spec.md:103,114` | 关闭态导出面 **245 = 基线 167 + 78**，未回退到 167。`BR-6`「产物结构与运行行为回退到拆分前状态」、`EX-5`「产物与拆分前一致」在字面上不成立；`AC-4.1` 只要求「依赖结构一致 + 用例通过」，故不阻塞。78 条中 5 条（`MessageParcel::GetRawData/RestoreRawData`、`IPCObjectProxy::GetProto/SetObjectDied/ClearDBinderServiceState`）对应公开头声明，其余 73 条为私有类内部符号；两形态导出集合相同（map 无条件分块），TASK-4 未额外扩大面。 | 基线 167 / 关闭态 245 / `comm` 差集 +78 −0；NEEDED diff 为空；开启态 235 = 167 − 10 + 78；关闭态核心库 `llvm-nm -D --defined-only \| grep -c 'IpcRpcOps'` = 0 | 二选一：**(a)**（推荐）在 `design.md` D-06 与 `spec.md` BR-6/EX-5 把回退粒度明确为「依赖结构 + 运行行为，不含导出集合」，并在 TASK-6 证据中登记 78 条为已知偏离；**(b)** 为关闭态复制一份 190 行版本脚本（双表长期漂移风险更高，不建议）。另建议在 `ipc_rpc_ops.h:38` 的「167 -> 235」后补一句「关闭态为 245（含本条分块）」，避免读者把拆分态口径套到回退态。 |
| S-02 | **Medium** | ③ 漏改（测试装配） | `interfaces/innerkits/ipc_single/BUILD.gn:256-266`；`test/BUILD.gn:89-160` | 关闭态测试 so 不满足 D-07：`ipc_single_test` 的 `sources = common_source`（含 loader，**不含 entry TU**，无 `IPC_RPC_STATIC_LINK`）；`ipc_single_test_static` 同样无 entry、无宏。结果：两种形态下测试 so 的 loader 都走动态分支，关闭态 `libipc_rpc.z.so` 不存在 → ops 表为 null，依赖 RPC 路径的既有用例走「RPC 不可用」分支（作者记录 13 个）。**不构成链接失败**：loader 的 `IpcRpcGetOps` 只经 `dlsym` 字符串引用（`ipc_rpc_ops.h:60`），编译期无强引用。 | `grep -n 'ipc_rpc_entry\|IPC_RPC_STATIC_LINK' test/BUILD.gn` = 0 命中；`ipc_single/BUILD.gn:265` 仅 `sources = common_source`；独立目标 `test/unittest/ipc/native/core/BUILD.gn:172-194` 只编 loader（`defines = ["IPC_RPC_UNIT_TEST"]`），靠「无 `IPC_RPC_STATIC_LINK`」走动态分支 | TASK-5 按计划补 entry TU + 宏（Files 表已列）；在此之前 TASK-2/3/4/5 必须同批合入。**条件性**：若评审门禁把 AC-4.1 的「执行 IPC 与 RPC 用例」压给 TASK-4 单任务，则本项升级为阻塞。 |
| S-03 | Low | ③ 一致性 | `ipc/native/src/core/dbinder/include/ipc_rpc_loader.h:62` vs `ipc_rpc_loader.cpp:366` | 宏门控不对称：头文件只在 `#ifdef IPC_RPC_UNIT_TEST` 下声明测试钩子，定义却要求 `#if defined(IPC_RPC_UNIT_TEST) && !defined(IPC_RPC_STATIC_LINK)`。**当前未触发**（`IpcRpcLoaderUnitTest` 只定义前者，`ipc_single` 只定义后者）；但若 TASK-5 给测试 so 同时加 `IPC_RPC_STATIC_LINK` 并保留 `IPC_RPC_UNIT_TEST`，会出现「声明存在、定义缺失」的链接错误。 | 两处宏行号如上；`grep -rn 'IPC_RPC_STATIC_LINK\|IPC_RPC_UNIT_TEST' --include=*.gn` 仅 `test/unittest/.../BUILD.gn:179`（UNIT_TEST）与 `ipc_single/BUILD.gn:142`（STATIC_LINK） | 头文件同步为 `#if defined(IPC_RPC_UNIT_TEST) && !defined(IPC_RPC_STATIC_LINK)`，或加注释说明钩子只在动态形态可用。 |
| S-04 | Low | ③ 证据完整性 | `execution-plan.md:551`；`out/x86_64_virt/packages/phone/{system,updater}/lib64/platformsdk/` | `make_packages` 仍被 preheat 遗留 CMakeCache 阻塞，实盘安装未验证：两份 `libipc_single.z.so` 仍是 pre-change 492,056 B @11-12 01:29，`libipc_rpc.z.so` 不存在。声明层已核对一致（`dest`/`install_images`/`innerapi_tags`/`install_enable`），且关闭态目标整体不定义、无悬空引用，故不影响 TASK-4 的开关结论。另：`ipc_core/BUILD.gn:62-75` 已注明「改安装目录须镜像到 `ipc_rpc_core`」，属正确约束。 | `ls -l packages/.../libipc_single.z.so` → 492,056 B @11-12 01:29 ×2；`ls packages/.../libipc_rpc.z.so` → No such file；module_info 存在且字段一致 | 在 TASK-6 重取安装证据；在此之前任何「已安装」表述都应标注为环境阻塞。 |
| S-05 | Low | ④ 接口面卫生 | `ipc/native/src/core/dbinder/include/ipc_rpc_ops.h:108`、`ipc_rpc_loader.h` | 两个私有头位于 `src/core/dbinder/include/`，该目录经 `config/BUILD.gn:27` 的 `ipc_util_config`（`ipc_core` 的 public_config）对全部消费者可见，因此 `extern "C" IpcRpcGetOps()` 与 ops 结构体对下游可 include。**不构成对外承诺**：不在 `bundle.json` 的 `header_base`（未安装到 SDK）、关闭态不导出、开启态仅 `libipc_rpc_map` 导出 1 条、且与核心库同目录强绑定（loader 按自身映像目录解析，`ipc_rpc_loader.cpp:120,180`）。属仓内既有「私有头与公开头同 include dir」惯例的延续。 | `config/BUILD.gn:22-30`；`ipc_core/BUILD.gn:37-42` 的 `public_configs`；`bundle.json:98,129` 两个 `header_base` 均不含 dbinder include；`ipc_rpc_ops.h:33-40` 已写 "Registered private ABI (not a public interface)" | 保留即可；若要更严格，可迁到不进入 `ipc_util_config` 的私有目录，或在头顶部加一行 `// Internal header, not an installed API.`。不要在本 Task 扩大改动面。 |

### 已排除的疑似问题（避免误导）

| 疑点 | 排除依据 |
|------|----------|
| 关闭态 `IpcRpcGetOps` 未导出 = 漏配置 | 该符号只在核心库内部被直接调用，`libipc_single_map` 用 `local: *` 收口是正确形态；若导出反而扩大私有 ABI 面。作者已按实测修正原验收口径（`execution-plan.md:520,545`） |
| `ipc_rpc_ops.h` 的「167 -> 235」是数字错误 | 235 是**拆分态**核心库口径（167 − 10 迁出 + 78 分块），与开启态实测 `cur_single.txt` 235 行一致；关闭态 245 是另一形态，见 S-01 建议补注 |
| `libdbinder` 关闭态缺 `DBinderSoftbusClient` 定义 | 关闭态核心库导出 10 条（8 softbus + 2 databus invoker ctor），`libdbinder` 的 8 个强未定义符号全部可解析；NEEDED 无 `libipc_rpc` |
| 关闭态残留 `dlopen`/`dlsym` 未定义符号 | 冻结产物 `llvm-nm -D -u` 中 `dlopen/dlsym/dladdr/dlerror` 与基线**完全相同**，来自其他既有 TU，不是 loader 残留（`strings` 中 `libipc_rpc` = 0） |
| 开关需要同步 `bundle.json` / 产品配置 | 该参数仅影响构建装配，默认 `true` 与设计一致；`bundle.json` 无目标名引用，`build/vendor/productdefine/device` 无 `ipc_rpc_core` 引用，无需登记 |
| 两个 fuzz 目标在关闭态会断链 | 依赖已改为 `ipc_single_test_static`（源集合），关闭态仍编入 4 个 TU + loader，不引用 `:ipc_rpc` 目标 |

## 阻塞判定

| 判定项 | 结果 |
|--------|------|
| 是否存在 Critical | 否 |
| 是否存在 High | **否** |
| Medium 项是否阻塞 | 否：S-01 属设计/文档口径（AC-4.1 只约束依赖结构，产物已逐条一致）；S-02 由 TASK-5 明确承接且已登记批次约束 |
| 阻塞发现合计 | **0** |
| 结论 | **PASS** |

## 未验证项与证据边界

| 项 | 原因 | 需人工补做 |
|----|------|-----------|
| 关闭态 ninja 装配（`-DIPC_RPC_STATIC_LINK`、`ipc_rpc_entry.o` 是否在中） | `out/` 已恢复开启态并正被后台重链，`obj/.../ipc_single/ipc_rpc_entry.o` 不可见 | 重新关闭开关构建后，`grep -c ipc_rpc_entry .../ipc_single.ninja` 与编译行 `-DIPC_RPC_STATIC_LINK` |
| `make_packages` 实盘安装 | preheat 遗留 CMakeCache（环境阻塞，见 `execution-plan.md:551`） | 修复环境后重跑，核对 `packages/.../platformsdk/{libipc_single.z.so,libipc_rpc.z.so}` 与 `dest` 一致 |
| 关闭态用例执行（AC-4.1 后半句） | 同上，且测试装配归 TASK-5 | TASK-5 补 entry + 宏后执行 `ipc_unittest`；在此之前不得声称「关闭态全部用例通过」 |
| 设备/模拟器运行期 | 无 `hdc`、无 `qemu-system-x86_64` | 按计划留 TASK-6 |

> 本报告所有判定均引用本地命令输出或 `file:line`；作者构建日志（`task4_green4_open.log`、`task4_green5_off.log` 均含 `x86_64_virt build success`）仅用于交叉核对时间戳与目标名，不作为结论依据。