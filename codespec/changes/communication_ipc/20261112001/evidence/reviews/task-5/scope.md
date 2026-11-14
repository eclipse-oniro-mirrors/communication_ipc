---
task_id: TASK-5
risk: R2
seat: scope
verdict: PASS
blocking_findings: 0
---

# Task Review — 漏改/完整性（③）与外部接口影响（④）

评审对象：TASK-5「测试装配与四类场景用例」（`codespec/changes/ipc/draft-20261112-split-rpc-so/execution-plan.md:580-687`）。

## 结论

- **verdict: PASS**，**blocking_findings: 0**（无 Critical / High）。
- **③ 漏改/完整性：通过（3 Medium + 3 Low，全部非阻塞）**。TASK-5 的增量闭合了 TASK-4 S-02（测试 so 缺 entry/宏）与 SC-04（静态形态失败面）：`ipc_single_test` 与 `ipc_single_test_static` 都补齐了 `ipc_rpc_entry.cpp` 与 `IPC_RPC_STATIC_LINK`，宏没有进入任何 `public_configs`，也没有外泄到产品面。遗留问题是**证据口径**而非装配错误：AC-6.1 的「全部通过」只覆盖 6/59 个测试二进制、真实 `dlopen` 路径无运行证据、AC-2.2 会话关闭断言与 AC-2.2/AC-2.3 的 Verification 行缺登记。
- **④ 外部接口影响：通过（0 新增对外接口面）**。公开头文件、NAPI、C API、ANI、Taihe、Rust、`bundle.json` 零改动；transaction code / 错误码 / ioctl 零新增（不触发 AGENTS.md 的 ArkWeb seccomp 同步条款）；新增的 `IpcRpcGetOps` 只出现在 `install_enable = false` 的测试 so 与静态档案上，`out/x86_64_virt/packages` 中查无 `*ipc_single_test*`。唯一与 ABI 相邻的改动是 U-1（静态成员 linkage 由 header 内联改为单一定义，S-04）。
- 作者 6 个二进制的运行结果（4/4、1/1、16/16、80/80、64/64、79/79）经本席位独立复跑**逐项一致**；全量 59 个二进制中 12 个非零退出项**无一可归因本次拆分**（详见 R-3 与 S-01）。

## 评审方式与边界

| 项 | 内容 |
|----|------|
| 评审范围 | TASK-5 的 3 个 `BUILD.gn`、2 个新增用例文件、U-1 装配修正（`dbinder_databus_invoker.h/.cpp`） |
| 只读约束 | 只写本文件；未改动任何源码/构建文件；未执行 `git add/commit/checkout/stash`；**未执行 `./build.sh`** |
| 构建证据 | 复用磁盘上已有产物（`out/x86_64_virt`）与作者留存日志（`/tmp/opencode/task5_*.log`），只作时间戳/目标名交叉核对，不作为「构建成功」的唯一依据 |
| 运行证据 | 本席位对 `out/x86_64_virt` 中 `ipc_unittest` **全部 59 个测试二进制**独立重跑（脚本 `/tmp/opencode/scope_run_all2.sh`，日志 `/tmp/opencode/scope_full_run2/`，汇总 `summary.txt`） |
| 产物证据 | `out/x86_64_virt/communication/ipc/{libipc_single.z.so,libipc_rpc.z.so,libipc_single_test.z.so}`、`obj/foundation/communication/ipc/test/libipc_single_test_static.a` |
| 工具 | `code/prebuilts/clang/ohos/linux-x86_64/llvm/bin/{llvm-nm,llvm-readelf,llvm-ar}`；运行器为 `out/x86_64_virt/common/common/libc/ld-musl-x86_64.so.1` |
| 席位边界 | 只评审 ③ 漏改/完整性与 ④ 外部接口影响；① 重复造轮子/② 性能归 arch 席，⑤ 安全归 security 席。TASK-4 已接受的取舍（A-01/A-03/A-06、S-01/S-05）不重复计入本任务发现 |

## 维度结论

| 维度 | 结论 | 摘要 | 代表证据 |
|------|------|------|----------|
| ③ 漏改/完整性 | **通过**（3 Medium + 3 Low，非阻塞） | TASK-4 交接的两项测试装配缺口均已闭合：`ipc_single_test` 增 entry TU + `IPC_RPC_STATIC_LINK`（`:265-266`），`ipc_single_test_static` 增 entry/loader + 宏（`test/BUILD.gn:110-111,165`），3 个新 target 全部挂进 `ipc_nativecore_unittest`（`:256-258`）。生产面零改动，测试 so 不进安装面。缺口集中在证据口径（S-01/S-02/S-03）与 U-1 越界（S-04）。 | 见 R-1 / R-2 / R-3 |
| ④ 外部接口影响 | **通过**（1 Medium，非阻塞，S-04） | 对外可见面零新增：公开头/NAPI/C API/ANI/Taihe/Rust/`bundle.json` 全部无 diff；无新 transaction code、错误码、ioctl；`IPC_RPC_STATIC_LINK` 只出现在 8 个文件、无 `public_configs` 命中；`ipc_single_test` 的 `visibility` 限 `$SUBSYSTEM_DIR/*` 且 `install_enable = false`，仓外消费方 0。U-1 触及内部头 + 实现 TU 的静态成员 linkage，属 ABI 相邻但类布局未变。 | 见 R-4 |
| 漏改计数 | **0（生产）/ 2 项由 TASK-4 转入且已闭合 / 1 项越界已披露（S-04）** | 未发现 Files 表之外的新增改动；`git status` 中其余 `M`/`??` 均可归属 TASK-1~4 | R-1 |

## AC-6.1 四类场景映射核对

`spec.md:92` 要求「执行现有 IPC 测试与新增 RPC 测试，全部通过，且覆盖四类场景」。逐类核对作者映射（`execution-plan.md` Actual Result 段）与独立证据：

| # | 场景 | 作者映射的用例 | 独立核验（命令 + 输出摘要） | 是否成立 | 证据缺口 |
|---|------|----------------|------------------------------|----------|----------|
| 1 | 仅 IPC 不加载 RPC | `IpcRpcLoaderTest.PeekDoesNotLoad` | 读源：`ipc_rpc_loader_unittest.cpp:299-304` 断言 `Peek()==nullptr`、`g_openCount==0`、`g_symCount==0`；复跑 `IpcRpcLoaderUnitTest` 16/16 PASS（`summary.txt`） | **成立**（loader 单元层 + TASK-2 热路径静态守卫） | 无进程级 `/proc/<pid>/maps`/`dlopen` 追踪证据，按计划留 TASK-6 真机 |
| 2 | 首次 RPC 调用成功 | `IpcRpcStaticLinkTest.DatabusInvokerIsRegisteredInTheSameImage`（关闭态）+ `LoadSuccessIsCached`（开启态） | 读源：`ipc_rpc_static_link_unittest.cpp:110-116` 断言 `GetRemoteInvoker(IF_PROT_DATABUS)` 非空且 `IpcRpcOpsPeek()==IpcRpcOpsGet()`；`ipc_rpc_loader_unittest.cpp:224` 的 `LoadSuccessIsCached` 的 `open/sym` 全部来自 `IpcRpcTestSetHooks`（该文件 `dlopen` 命中仅 1 处注释）；复跑 4/4 + 16/16 PASS | **部分成立** | 真实 `dlopen("…/libipc_rpc.z.so")`+`dlsym("IpcRpcGetOps")` 无任何运行证据；真实跨设备调用（softbus 通路）无运行证据 |
| 3 | RPC 不可用降级 | `IpcRpcStaticLinkFailureTest.IncompleteTableIsRejectedAndNotRetried` + loader 的 6 个失败分支用例 | 读源：`ipc_rpc_static_link_failure_unittest.cpp:112-124` 断言残缺表 → `IpcRpcOpsGet()==nullptr`、`providerCalls==1`、二次调用不重试；复跑 1/1 PASS，loader 失败分支 16/16 内 PASS | **部分成立** | 只证到「表被缓存为空」；AC-3.1 要求的调用方返回 `ERR_NULL_OBJECT` 无断言 —— `grep -rn ERR_NULL_OBJECT --include=*.cpp test/` = **0** 命中 |
| 4 | 关闭开关后回退 | `IpcRpcStaticLinkTest` 4 例 + `ipc_single_test_static` 构建 | `test/BUILD.gn:89-180` 只读确认：静态 target 含 4 个 RPC TU + entry + loader，11,006,722 B / 32 成员，`ipc_rpc_entry.o` 定义 `IpcRpcGetOps`、`ipc_rpc_loader.o` 的 `dl*` 未定义符号 = **0**；TASK-4 关闭态构建日志 `/tmp/opencode/task4_green5_off.log`（EXIT=0，无 `libipc_rpc.z.so`） | **部分成立** | `ipc_single_test`/`ipc_single_test_static` **不受** `ipc_feature_rpc_split_enabled` 门控（`ipc_single/BUILD.gn:249-266` 无条件 `IPC_RPC_STATIC_LINK`），用例运行在「形态等价」而非「开关关闭构建产物」上；无关闭态端到端运行 |
## 独立核验记录

### 证据基线

| 项 | 值 |
|----|----|
| 仓根 | `code/foundation/communication/ipc`（工作区累计态，未提交） |
| 生产产物 | `libipc_single.z.so` 371,584 B @11-13 01:31（导出 235 条，`NEEDED` 12 条且无 `libipc_rpc`）；`libipc_rpc.z.so` 177,304 B @11-13 01:41（导出 11 条，含 `IpcRpcGetOps@@1.0`） |
| 测试产物 | `libipc_single_test.z.so` 606,944 B @11-13 01:41（导出 `IpcRpcGetOps`/`IpcRpcOpsGet`/`IpcRpcOpsPeek`，`NEEDED` 12 条且无 `libipc_rpc`）；`libipc_single_test_static.a` 11,006,722 B @11-13 01:49（32 成员） |
| 全量运行汇总 | `/tmp/opencode/scope_full_run2/summary.txt`（59 二进制 + `DONE`） |
| 失效/过期的证据 | `out/x86_64_virt/packages/phone/system/lib64/`：`libdbinder.z.so` 166,600 B @11-12 01:52，`libipc_single.z.so` 缺失，`libipc_rpc.z.so` 缺失 —— 不用于任何导出面/运行判定，只用于「测试 so 是否进安装面」与 S-07 |
| 未采用 | 作者日志中的 `EXIT=0` 文字（日志内无显式退出码），只用于时间戳与目标名核对；结论以本席位独立重跑为准 |

### R-1 变更集与 Files 表一致性

| Files 表行 | 实际（`git diff --numstat` / 只读文件） | 结论 |
|------------|------------------------------------------|------|
| `interfaces/innerkits/ipc_single/BUILD.gn`：`ipc_single_test` 追加 entry + 宏 | 累计 `119/37`；TASK-5 相关段 `:265` `sources = common_source + [ipc_rpc_entry.cpp]`、`:266` `defines = ["IPC_RPC_STATIC_LINK"]` | 一致 |
| `test/BUILD.gn`：静态 target 追加 source + config 宏 | `+6 −0`：`:110` entry、`:111` loader、`:162-164` 三行注释、`:165` `defines += ["IPC_RPC_STATIC_LINK"]` | 一致（loader 行与「TASK-3 计划外补入」不矛盾：工作区未提交，无法按任务拆分归属；累计 diff 只能看到该行存在） |
| `test/unittest/ipc/native/core/BUILD.gn`：新增 2 target + group | `+67 −0`；`IpcRpcLoaderUnitTest` `:172`、`IpcRpcStaticLinkUnitTest` `:199`、`IpcRpcStaticLinkFailureUnitTest` `:206`、group deps `:256-258` | 一致（纯新增，既有 target 零改动） |
| 新增 `ipc_rpc_static_link_unittest.cpp` | 4,164 B @11-13 01:32，4 个 `HWTEST_F`（`:62/:76/:89/:110`） | 一致 |
| 新增（计划外）`ipc_rpc_static_link_failure_unittest.cpp` | 1 个 `HWTEST_F`（`:112`），自带残缺表 provider | 一致（U-3 已登记） |
| 修改（计划外）`dbinder_databus_invoker.h` / `.cpp` | `10/1` + `9/0`：header 由 `static inline … = {…}` 改为 `static … ;` 声明 + 注释（`:132`），cpp 新增唯一定义（`:41-42`） | 一致但**越界**（触碰「禁止修改文件」清单，见 S-04） |
| 禁止修改文件（`ipc/native/src/core/framework/source/`、`test/unittest/ipc/native/core/` 既有用例） | `git diff -- test/unittest/ipc/native/core/ipc_object_stub_unittest.cpp` 只有 `+1` 行 `#include "ipc_rpc_ops.h"`，mtime 11-12 22:03（属 TASK-2）；`ipc_process_skeleton_unittest.cpp` 同为 22:03 | 未发现 TASK-5 对既有用例的改动 |

命令与输出摘要：`git diff --numstat -- interfaces/innerkits/ipc_single/BUILD.gn test/BUILD.gn test/unittest/ipc/native/core/BUILD.gn …` → `119 37 / 6 0 / 67 0 / 10 1 / 9 0`；`git status --short` → 23 `M` + 7 `??`，除上表外均可归属 TASK-1~4。

### R-2 测试装配与产品面隔离

| 检查 | 结论 | 证据（命令 + 输出摘要） |
|------|------|--------------------------|
| `ipc_single_test` 是全量 TU + entry + 宏 | 是 | `interfaces/innerkits/ipc_single/BUILD.gn:265-266`；`sources = common_source`（`common_source` 含 4 个迁出 TU + loader） |
| 静态档案含两份 TU | 是 | `llvm-ar t obj/foundation/communication/ipc/test/libipc_single_test_static.a` → `ipc_rpc_entry.o`、`ipc_rpc_loader.o`；成员 32 |
| 静态分支编译期生效 | 是 | 抽取对象后 `llvm-nm -u ipc_rpc_loader.o \| grep -cE 'dlopen\|dlsym\|dladdr\|dlerror'` → **0**；`llvm-nm ipc_rpc_entry.o` → `T IpcRpcGetOps` |
| 宏不外泄 | 是 | `grep -rn IPC_RPC_STATIC_LINK --include=*.gn --include=*.cpp --include=*.h test/ interfaces/ ipc/ utils/ config/` → 仅 `ipc_single/BUILD.gn`(3)、`test/BUILD.gn`(1)、`core/BUILD.gn`(2)、loader `.h`(3)/`.cpp`(7)/`ops.h`(1)、两个新用例各 1；**无 `public_configs` 命中** |
| 测试 so 不进安装面 | 是 | `ipc_single/BUILD.gn:288` `install_enable = false`、`:250` `visibility = ["$SUBSYSTEM_DIR/*"]`；`find packages -name "*ipc_single_test*"` → 空 |
| 静态档案不进产品 | 是 | `ohos_static_library("ipc_single_test_static")` 无 install 声明；消费方仅 `test/fuzztest/**`、`test/unittest/**`、`test/auxiliary/**` |
| 用例确实被构建 | 是 | `grep -n "IpcRpcStaticLinkUnitTest" core/BUILD.gn` → `:199` 定义、`:257` group；复跑 `summary.txt` 中 `ipc_core__IpcRpcStaticLinkUnitTest rc=0 ok=4`、`…FailureUnitTest rc=0 ok=1` |
| 仓外引用 | 0 | `grep -rn ipc_single_test --include=*.gn` 仅仓内 `test/` 与 `interfaces/innerkits/ipc_single/BUILD.gn`；仓外命中全部落在 `code/out/…/tests/res/`（构建生成副本） |

### R-3 全量用例运行（59 个二进制）

`/tmp/opencode/scope_run_all2.sh` 以部件产出目录优先的 `LD_LIBRARY_PATH` + `ld-musl-x86_64.so.1` 逐个运行 `out/x86_64_virt/tests/unittest/ipc/**` 下全部 59 个测试二进制。**47 个 `rc=0`，12 个非零**：

| 二进制 | 结果 | 失败/异常点 | 归因（与本次拆分无关的依据） |
|--------|------|-------------|------------------------------|
| `ipc_core__IpcRpcStaticLinkUnitTest` | **4/4 PASS** | — | 作者结果的独立复现 |
| `ipc_core__IpcRpcStaticLinkFailureUnitTest` | **1/1 PASS** | — | 同上 |
| `ipc_core__IpcRpcLoaderUnitTest` | **16/16 PASS** | — | 同上 |
| `ipc_core__IPCProcessSkeletonUnitTest` | **80/80 PASS** | — | TASK-2 登记的 2 例已转绿 |
| `ipc_core__IPCObjectProxyUnitTest` | **64/64 PASS** | — | TASK-2 登记的 3 例已转绿 |
| `ipc_core__IpcObjectStubUnitTest` | **79/79 PASS** | — | TASK-2 登记的 8 例已转绿 |
| `ipc_core__BinderInvokerUnitTest` | rc=1，113 ok / **21 failed** | `AddDeathRecipient`/`RemoveDeathRecipient`/`RefreshRecipient`/`SendRequest` 等 | 无 binder 驱动（`/dev/binder` 不可用），失败集全是驱动交互用例 |
| `ipc_ipc_common__BinderConnectorUnitTest` | **rc=139** | 崩在 `CloseDriverFd001` | 驱动 fd 生命周期，同上 |
| `ipc_ipc_common__BinderInvokerUnitTest` | **rc=139** | 运行早期 SIGSEGV | 同上 |
| `ipc_ipc_common__IPCNativeUnitTest` | **rc=139**，136 ok / 13 failed | 含 `DBinderSessionObjectTest.CloseDatabusSessionTest001` 等 136 例通过 | 同上（`/dev/binder`） |
| `ipc_ipc_common__IpcCApiUnitTest` | rc=1，50 ok / **3 failed** | `Skeleton_GetSelfTokenId_001`(id=0)、`Skeleton_GetCallingUid_001`(4092 vs 0)、`Skeleton_SetMaxWorkThreadNum_001`(1901007) | 无 access token service / SAMgr / 线程池上限 |
| `ipc_core__DbinderDataBusInvokerUnitTest` | rc=1，116 ok / **1 failed** | `GetSelfTokenIDTest002`（`:2671` `result != 0`） | 同上（token service） |
| `ipc_core__MessageParcelAddUnitTest` | rc=1，22 ok / **1 failed** | `WriteRawDataTest003` | ashmem/`/dev/ashmem` 或 fd 限制 |
| `ipc_ipc_common__IPCFileDescOpsUnitTest` | rc=1，8 ok / **1 failed** | `fd_parcelable_001`（fd < 0） | fd 限制 |
| `ipc_ipc_common__IPCMockUnitTest` | rc=1，86 ok / **1 failed** | `SetMaxWorkThread` | 线程池上限（环境） |
| `ipc_ipc_common__IPCNapiUnitTest` | rc=1，0 ok / **1 failed** | callingUid 4092 | 无 token service |
| `ipc_ipc_common__IPCCskeletonUnitTest` | rc=1，**17/17 PASS** | 仅 2 条 gmock leak 报告 | 测试自身 mock 未清理（TASK-5 前即存在） |
| `ipc_dbinder_service__DBinderServiceDeathRecipientUnitTest` | rc=1，5 ok / **1 failed** | `RemoveDbinderDeathRecipientTest002`：`dbinder_service_death_recipient_unittest.cpp:122` 得 **701**、`:125` 得 **706** | 701=`DBINDER_SERVICE_INVALID_DATA_ERR`、706=`DBINDER_SERVICE_REMOVE_DEATH_ERR`，均命中 `dbinder_service_stub.cpp:198-205` 的 `object == nullptr` 分支：`WriteRemoteObject`/`ReadRemoteObject` 依赖 binder 驱动，环境缺失导致回读为空。该文件与 `dbinder_service_stub.cpp` 在本变更集内 `git diff` **为空**，且路径不经过 `IpcRpcOps` |

补充交叉核对：

| 项 | 命令 + 输出摘要 |
|----|------------------|
| 作者 6 个二进制日志真实存在 | `ls /tmp/opencode/task5_testlogs/` → 6 个 `.log`；逐条 `tail` 与 `summary.txt` 的 ok 数一致 |
| `DBinderRemoteListenerUnitTest` 的 rc=127 已定位 | 按 `packages/phone/system/lib64` 在前的 LD 顺序运行时，`libdbinder.z.so`（11-12 01:52 旧件，`readelf -d` 无 `libipc_rpc` 依赖）报 `DBinderSoftbusClient::GetInstance` symbol not found；改为部件产出优先后 **50/50 PASS**（`summary.txt`） |
| 陈旧安装件现状 | `ls packages/phone/system/lib64/libdbinder.z.so` → 166,600 B @11-12 01:52；`find packages -name "libipc_rpc*"` → 空；`libipc_single.z.so` → 不存在 |

### R-4 外部接口影响（④）

| 检查 | 结论 | 证据（命令 + 输出摘要） |
|------|------|--------------------------|
| 公开头文件 | 零改动 | `git status --short -- interfaces/innerkits/ipc_core/include interfaces/innerkits/c_api/include interfaces/innerkits/libdbinder/include` → 空 |
| NAPI / C API / ANI / Taihe / Rust | 零改动 | 同上路径集合外加 `interfaces/kits`、`ipc/native/src/{c_api,ani,taihe}`、`interfaces/innerkits/rust` → 空 |
| `bundle.json` | 未改 | `git status --short -- bundle.json` → 空；`header_base` 仍只指向 `ipc_core/include` |
| transaction code / 错误码 | 0 新增 | `git diff \| grep -cE "ioctl\|TransactionCode"` → **0** |
| `ioctl` | 0 新增 | 同上 → 不触发 AGENTS.md 的 ArkWeb `baseline_policy_ohos.cc` 同步条款 |
| 测试 so 的导出面 | 有新增但不可达产品 | `llvm-nm -D --defined-only libipc_single_test.z.so` → `T IpcRpcGetOps`、`T …IpcRpcOpsGet`、`T …IpcRpcOpsPeek`；该 so `install_enable = false`、`visibility = ["$SUBSYSTEM_DIR/*"]`、`packages` 中不存在 |
| 测试 so 依赖方向 | 单向，无 dlopen 残留 | `llvm-readelf -d libipc_single_test.z.so \| grep -c libipc_rpc` → **0**；`NEEDED` 12 条与基线相同 |
| 生产库形态未被测试改动影响 | 是 | `libipc_single.z.so` 371,584 B / 235 导出 / `NEEDED` 无 `libipc_rpc`；`libipc_rpc.z.so` 177,304 B / 11 导出 / `NEEDED` 含 `libipc_single` |
| AC-2.2 静态属性（会话对象仍在核心库） | 是 | `llvm-nm -D --defined-only libipc_single.z.so \| grep -c DBinderSessionObject` → **19**；`libipc_rpc.z.so` → **0** |
| 类布局/ABI（U-1） | 未变 | `DBinderDatabusDelegator_` 是 `private static` 数据成员，不参与对象布局；`llvm-nm -D libipc_single.z.so \| grep -c DBinderDatabusInvoker` → **0**、`libipc_rpc.z.so` → **2**；两库均未导出 `DBinderDatabusDelegator_`（`local:*` 收口） |

## 发现

### 非阻塞发现

| ID | 严重度 | 维度 | 位置 | 问题与影响 | 证据（命令 + 输出摘要） | 建议 |
|----|--------|------|------|-----------|--------------------------|------|
| S-01 | **Medium** | ③ 完整性 / 证据口径 | `execution-plan.md:669-678`（Actual Result 表） | 「AC-6.1 全部通过」的实际口径只有作者跑的 **6/59** 个测试二进制。本席位全量重跑后 12 个非零退出；其中 `DBinderServiceDeathRecipientUnitTest` 属 AC-2.2 的死亡通知域、且与 `ipc_single_test` 同 so，1 例失败。这些失败可逐项归因环境（无 binder 驱动 / 无 token service / fd 限制 / gmock leak / 陈旧安装件），**无一可归因本次拆分**，因此不阻塞；但 AC-6.1 字面「全部通过」在证据层面未成立，且未披露「53 个二进制未运行 + 失败集合」。 | `wc -l /tmp/opencode/scope_full_run2/summary.txt` → 60（含 `DONE`）；`awk '$2!="rc=0"'` → 13（12 二进制 + `DONE`）；`grep RemoveDbinderDeathRecipientTest002 /tmp/opencode/scope_dbinderdeath.log` → `FAILED`（701/706） | 二选一：**(a)** 在 `summary.md` 登记「59 个二进制全量结果 + 环境豁免清单 + 每项归因」，把 AC-6.1 的口径从「全部通过」改为「除环境豁免项外全部通过」；**(b)** 把 AC-6.1 收窄为「6 个目标二进制 + 4 类场景用例」，并另立 TASK-6/真机条目承接全量。推荐 (a)。 |
| S-02 | **Medium** | ③ 完整性 / AC-6.1 场景 2、3 | `test/unittest/ipc/native/core/ipc_rpc_loader_unittest.cpp:169-186`；`ipc_rpc_static_link_unittest.cpp:110-116` | 拆分最核心的「真实动态加载」没有运行证据：loader 的 16 个用例全部通过 `IpcRpcTestSetHooks` 注入 `open/resolveSymbol/path`，`dlopen` 在该文件只出现在 1 处注释里；静态用例证明的是「同一映像内取表」。因此 AC-6.1 场景 2「首次 RPC 调用成功」只到「能力表可取」层，场景 3「降级」只到「表缓存为空」层——AC-3.1 的调用方返回 `ERR_NULL_OBJECT`（7）没有任何断言。**影响**：若真实 `dlopen`/`dlsym`/映像校验路径有缺陷（如路径解析、`dlerror` 处理、符号版本），现有测试无法发现。 | `grep -c HWTEST ipc_rpc_loader_unittest.cpp` → 16；`grep -c IpcRpcTestSetHooks` → 1（`SetUp` 内统一注入）；`grep -c dlopen` → 1（注释 `:327`）；`grep -rn ERR_NULL_OBJECT --include=*.cpp test/` → **0** | 在 TASK-6 或后续补 1 个「真实加载」用例：把真实 `libipc_rpc.z.so` 放到被测二进制同目录/绝对路径，不设 hooks，断言 `IpcRpcOpsGet()` 非空且 `abiVersion` 正确；再补 1 个「空表调用点返回 `ERR_NULL_OBJECT`」的用例。若确认留真机，须在 `summary.md` 显式登记为未验证项，不得表述为「场景 2/3 已验证」。 |
| S-03 | **Medium** | ③ 完整性 / AC-2.2 | `execution-plan.md:646-664`（Verification 表）；`dbinder_session_object.cpp:39-52`；`test/unittest/common/dbinder_session_object_unittest.cpp:253` | TASK-5 声明的 AC 映射含 AC-2.2/AC-2.3，但 Verification 表 5 行只覆盖构建、静态 so、用例接入、宏范围、断言完整性，**没有 AC-2.2/AC-2.3 行**；AC 追溯表两者仍记 `Blocked`。AC-2.2 要求的「会话关闭路径只读能力表做用例断言」实际只由通用 `PeekDoesNotLoad` 覆盖——既有 `CloseDatabusSessionTest001` 调用 `CloseDatabusSession()` 但**不断言「未触发加载」**。AC-2.3 的「权限与非法参数负向用例同套」已由既有用例满足（见「已排除」表），却没有被登记为证据。 | `grep -n "AC-2.2\|AC-2.3" execution-plan.md` → 仅出现在 AC 映射与追溯表，Verification 段无命中；`sed -n 253,260p dbinder_session_object_unittest.cpp` → 只 `object.CloseDatabusSession()`；`grep CloseDatabusSession /tmp/opencode/scope_full_run2/ipc_ipc_common__IPCNativeUnitTest.log` → `[ OK ]`（在 136 个通过项内） | 在 Verification 表补 AC-2.2/AC-2.3 两行：AC-2.2 用「`CloseDatabusSessionTest001` 通过 + `PeekDoesNotLoad` 断言 openCount==0」组合；AC-2.3 用「`DBinderRemoteListenerUnitTest` 的 `Return(1)` 负向例 50/50 + `DBinderServiceUnitTest` 的 `DBINDER_SERVICE_INVALID_DATA_ERR` 负向例 127/127」。并在 TASK-6 把追溯表状态从 `Blocked` 更新为对应的 Pass/Blocked 边界。 |
| S-04 | **Medium** | ④ ABI 相邻 / ③ 变更边界 | `ipc/native/src/core/dbinder/include/dbinder_databus_invoker.h:132`；`.../source/dbinder_databus_invoker.cpp:41-42` | U-1 修改了 TASK-5「禁止修改文件」清单内的 `ipc/native/src/core/dbinder/source/` 实现文件，且改动性质是**静态数据成员 linkage**：`static inline … = {…}` → `static … ;` + 单一 TU 定义。类对象布局不变（静态成员不进布局），`DBinderDatabusDelegator_` 两库均未导出；但它把「注册时机」从「任意包含该头的 TU 静态初始化」改成「`libipc_rpc` 加载时的静态初始化」，属 ABI/行为相邻面，且与 TASK-2/3/4 同批合入，归属需治理层确认。技术上是解决 `IpcCApiUnitTest` 链接期 `undefined symbol: DBinderDatabusInvoker::DBinderDatabusInvoker()` 的最小改动，作者已披露。 | `git diff -- ipc/native/src/core/dbinder/...` → header `+10/−1`、cpp `+9/−0`；`llvm-nm -D libipc_single.z.so \| grep -c DBinderDatabusInvoker` → **0**、`libipc_rpc.z.so` → **2**；`grep -rn DBinderDatabusDelegator_` → 仅声明 + 定义 2 处；`libipc_rpc.z.so` 173,160 → 177,304 B（+4,144 B） | 在 `verdict.md` 明确：(a) 批准 U-1 越界（记录为「装配必要修正」并归入 TASK-6 净增）；或 (b) 要求 U-1 独立成补丁提交，避免混入 TASK-5 的测试装配变更集。同时确认 `+4,144 B` 已计入 TASK-6 的拆分净增口径。 |
| S-05 | **Low** | ③ 完整性 / AC-6.1 场景 4 | `interfaces/innerkits/ipc_single/BUILD.gn:249-266`；`test/BUILD.gn:89-180` | 场景 4「关闭开关后回退」的用例运行形态与字面口径不同：`ipc_single_test` **无条件**使用 `common_source + entry` 与 `IPC_RPC_STATIC_LINK`，不随 `ipc_feature_rpc_split_enabled` 变化；`ipc_single_test_static` 同样不被开关门控。因此 4 个静态用例跑的是「单库形态等价物」，不是「开关关闭构建产物」；关闭态只有 TASK-4 的构建证据，没有用例运行证据。属口径差异，不构成装配错误。 | `grep -n "ipc_feature_rpc_split_enabled\|IPC_RPC_STATIC_LINK" interfaces/innerkits/ipc_single/BUILD.gn` → 开关只出现在 `ipc_single`(`:151/:170`) 与 `ipc_rpc`(`:204`)；`ipc_single_test`(`:249-266`) 无开关引用 | 在 `summary.md` 把 AC-4.1/场景 4 的证据写成「关闭态构建（TASK-4）+ 静态形态用例（TASK-5）」，不要写成「关闭开关后全部用例通过」；若治理层要求字面闭环，TASK-6 追加一次 `--gn-args ipc_feature_rpc_split_enabled=false` 构建后的用例运行。 |
| S-06 | **Low** | ③ 证据口径 | `execution-plan.md:635`（Step 1 RED-1） | RED-1 没有实跑「预期失败」这一轮，改用冻结 so 的符号缺失作等价证据。该方法本身成立（新用例链接的正是该 so，无 provider 即必失败），作者也已披露；但严格 TDD 口径下缺少一次真实 RED 运行记录。 | `llvm-nm -D --defined-only /tmp/opencode/pre_task5_libipc_single_test.z.so \| grep -c IpcRpcGetOps` → **0**；当前 so → **1** | 保留等价证据即可，无需补跑；在 `summary.md` 标注为「等价 RED」以免后续读者误判为未执行 TDD。 |
| S-07 | **Low** | ③ 证据可复现性 | `out/x86_64_virt/packages/phone/system/lib64/`；承接 TASK-4 S-04 | 安装目录是陈旧态：`libdbinder.z.so` 为 11-12 01:52 旧件（`DT_NEEDED` 无 `libipc_rpc`），`libipc_single.z.so` / `libipc_rpc.z.so` 均缺失。若按默认冒号顺序把 `packages/.../lib64` 放在前面运行，`DBinderRemoteListenerUnitTest` 会因旧 `libdbinder.z.so` 找不到 `DBinderSoftbusClient::GetInstance` 而 rc=127。作者与本席位的「全绿」都依赖「部件产出优先」的 `LD_LIBRARY_PATH`，该前提未写入证据。 | `ls -l packages/phone/system/lib64/libdbinder.z.so` → 166,600 B @11-12 01:52；`find packages -name "libipc_rpc*"` → 空；`ls packages/phone/system/lib64/libipc_single.z.so` → No such file；按两种 LD 顺序复跑差异 → 127 vs 50/50 | 在 `summary.md` 固化「运行需 `sodirs2.txt` 中部件产出目录优先」的命令原文；`make_packages` 环境修复后（TASK-4 S-04）再取一次安装态证据。 |

### 已排除的疑似问题（避免误导）

| 疑点 | 排除依据 |
|------|----------|
| AC-2.3「权限/非法参数负向用例」缺失 | **不成立**：既有负向用例存在且全绿 —— `dbinder_remote_listener_unittest.cpp:331` `WillOnce(Return(1))`（授权失败分支）、`dbinder_service_unittest.cpp:2466` `WillOnce(Return(DBINDER_SERVICE_INVALID_DATA_ERR))`；本席位复跑 `DBinderRemoteListenerUnitTest` 50/50、`DBinderServiceUnitTest` 127/127，且两者都挂在 `ipc_unittest` → `dbinder_service:ipc_dbinder_unittest` 下 |
| 既有用例被 TASK-5 改动 | **不成立**：`ipc_object_stub_unittest.cpp` 只有 `+1` 行 `#include "ipc_rpc_ops.h"`（mtime 11-12 22:03，属 TASK-2）；`ipc_process_skeleton_unittest.cpp` 同为 22:03；`ipc_object_proxy_unittest.cpp` 未改 |
| 新增宏外泄到产品/消费者 | **不成立**：`grep -rn IPC_RPC_STATIC_LINK` 的 8 个命中文件中无 `public_configs`；`ipc_single_test` 的 `defines` 是私有的，`visibility` 限 `$SUBSYSTEM_DIR/*`，`install_enable = false` |
| 测试 so 会被装进产品 | **不成立**：`find packages -name "*ipc_single_test*"` 为空；静态档案无 install 声明 |
| TASK-4 S-03（loader 宏门控不对称）仍存在 | **已闭环**：`ipc_rpc_loader.h:75` 与 `ipc_rpc_loader.cpp:366` 均为 `#if defined(IPC_RPC_UNIT_TEST) && !defined(IPC_RPC_STATIC_LINK)`；hook 调用点（`:174/:233/:244`）都在 `:93-263` 的 `#ifndef IPC_RPC_STATIC_LINK` 内 |
| 新用例「只挂 group 不构建」或「绕过 `IpcRpcOpsGet()` 直接 extern」 | **不成立**：Anti-Fake 两条都满足 —— `core/BUILD.gn:256-258` 三个 target 全在 group；`ipc_rpc_static_link_unittest.cpp:62-74` 先 `IpcRpcOpsGet()` 取表再断言，未直接 `extern` provider |
| 静态档案缺 loader 或 entry | **不成立**：`llvm-ar t libipc_single_test_static.a` → 同时含 `ipc_rpc_loader.o` 与 `ipc_rpc_entry.o`；loader 的 `dl*` 未定义符号为 0，entry 定义 `IpcRpcGetOps` |
| `DBinderServiceDeathRecipientUnitTest` 失败由拆分引起 | **不成立**：701/706 命中 `dbinder_service_stub.cpp` 的 `object == nullptr` 分支，取决于 binder 驱动的 parcel 读写；该实现文件与测试文件在本变更集 `git diff` 为空，路径不经过 `IpcRpcOps` |

## 阻塞判定

| 判定项 | 结果 |
|--------|------|
| 是否存在 Critical | 否 |
| 是否存在 High | **否** |
| Medium 项是否阻塞 | 否：S-01/S-03/S-05/S-06 属证据口径与追溯表登记，产物与用例本体已独立验证；S-02 属「设计上不做 dlopen 测试」（`design.md` D-07）+ 运行期证据留 TASK-6，未见功能缺陷；S-04 已披露、技术正确、类布局未变，仅需治理层确认归属 |
| 是否发现生产面漏改 | 否：生产库 235 导出 / 12 `NEEDED` / 无 `libipc_rpc` 依赖，与 TASK-4 记录逐字节一致 |
| 阻塞发现合计 | **0** |
| 结论 | **PASS** |

## 未验证项与证据边界

| 项 | 原因 | 需人工补做 |
|----|------|-----------|
| 真实 `dlopen`/`dlsym` 加载 `libipc_rpc.z.so` | 测试设计按 D-07 走 hook / 同映像取表；本机无产品安装态（`packages` 缺两份库） | TASK-6 或真机补真实加载用例，见 S-02 |
| 关闭开关构建产物上的用例运行 | `ipc_single_test*` 不受开关门控；关闭态只有 TASK-4 构建日志 | `--gn-args ipc_feature_rpc_split_enabled=false` 构建后运行 `ipc_unittest`，见 S-05 |
| 设备/模拟器端到端（softbus 跨设备、`/proc/<pid>/maps`） | 无 `hdc`、无 `qemu-system-x86_64` | 按计划留 TASK-6 |
| 12 个非零退出二进制的「拆分前基线」对照 | 未保存拆分前同环境的全量运行日志 | 可在关闭态构建后复跑同 59 个二进制，作为环境豁免的直接对照，见 S-01 |
| `make_packages` 实盘安装 | 仍被 preheat 遗留 CMakeCache 阻塞（TASK-4 S-04） | 环境修复后重取安装态证据 |

> 本报告所有判定均引用本席位执行的命令输出或 `file:line`；作者日志（`task5_unittest_build3.log`、`task5_components_build.log`、`task5_static_test_build.log`、`task5_testlogs/*.log`）仅用于交叉核对产物时间戳与用例数，不作为「通过」的唯一依据。
