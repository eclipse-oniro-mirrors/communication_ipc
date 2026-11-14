---
task_id: TASK-2
risk: R2
seat: scope
verdict: PASS
blocking_findings: 0
---

# Task Review — 漏改/完整性（③）与外部接口影响（④）

## 审查对象与边界

| 项 | 值 |
|----|----|
| Task | TASK-2 核心库调用点收敛与头文件解耦（`execution-plan.md:222-325`） |
| 风险等级 | R2（IPC 核心路径、并发、权限判定） |
| 审查席位 | scope（③ 漏改/完整性、④ 外部接口影响），独立只读评审 |
| 写入边界 | 仅本文件；除本文件外未修改仓库任何文件 |
| 未评审产物 | TASK-1 的 `ipc_rpc_ops.h`、`ipc_rpc_loader.{h,cpp}`、`ipc_rpc_loader_unittest.cpp`、`test/unittest/ipc/native/core/BUILD.gn` 新增部分（已由其他席位评审，本报告只作为上下文引用其最终形态） |
| 对照契约 | `design.md` D-01/D-02/D-03/D-09、调用点表（`:193-205`）、状态归属与不变量（`:241-270`）；`execution-plan.md` TASK-2 全节、TASK-5（`:500-580`，D-07 静态链接恢复）、Gate-2 与代码范围映射（`:639-680`） |
| 变更面 | 工作区 15 个 `M` + 4 个 `??`：TASK-1 6 项、TASK-2 计划内 10 项、TASK-2「计划外必需」2 项、越界 1 项（`.gitignore`） |
| 验证方式 | 只读走查 + `git diff` / `git show HEAD:` 对照 + `grep -rn` 静态守卫 + `readelf`/`llvm-nm` 产物核对 + host 侧实测单测（musl loader 直接运行构建产物） |
| 未执行 | 按指令**未执行** `./build.sh`；未在设备/模拟器运行跨设备 RPC（本机无 `hdc`），AC 的运行期半边仍归 TASK-6 |

命令基线：构建产物位于 `code/out/x86_64_virt/`（注意：不在仓根 `foundation/communication/ipc` 之下）；单测运行命令为

```sh
OUT=out/x86_64_virt
LIBS=$(find $OUT/lib.unstripped -name "*.so" -printf "%h\n" | sort -u | tr '\n' ':')
LIBS="$LIBS$OUT/system/lib64:$OUT/tests/unittest/ipc/ipc/core/resource/communication/ipc"
$OUT/common/common/libc/ld-musl-x86_64.so.1 --library-path "$LIBS" \
  $OUT/tests/unittest/ipc/ipc/core/<TestBinary>
```

## A. ③ 漏改/完整性

| # | 检查项 | 结论 | 证据（File:Line / 命令输出） |
|---|--------|------|-----------------------------|
| S-1 | TASK-2 改动是否覆盖 `execution-plan.md` 声明的全部文件 | 通过 | 映射表 TASK-2 行声明 10 个文件（`execution-plan.md:656-665`），工作区全部命中且无遗漏：`ipc_thread_skeleton.cpp`、`ipc_object_proxy.cpp`、`ipc_object_stub.cpp`、`ipc_process_skeleton.cpp`、`ipc_process_skeleton.h`、`dbinder_session_object.h`、`dbinder_session_object.cpp`、`databus_socket_listener.h`、`ipc_object_stub_unittest.cpp`、`dbinder_databus_invoker_unittest.cpp`。另按 Files 表「计划外必需」改了 2 个测试文件（见 S-13） |
| S-2 | 是否残留对已迁出 RPC 实现类型的直接引用（守卫 G1） | 通过，0 命中 | `grep -rn "DBinderDatabusInvoker\|dbinder_databus_invoker.h" ipc/native/src/core/framework ipc/native/src/core/dbinder/source/dbinder_session_object.cpp ipc/native/src/core/dbinder/include/dbinder_session_object.h` → 无输出，`exit=1` |
| S-3 | 是否残留 `DatabusSocketListener` 直接引用（守卫 G2） | 通过，0 命中 | `grep -rn "DatabusSocketListener\|databus_socket_listener.h" <同 S-2 范围>` → 无输出，`exit=1` |
| S-4 | 是否残留 `DBinderSoftbusClient` 直接引用（守卫 G3） | 通过，0 命中 | `grep -rn "DBinderSoftbusClient\|dbinder_softbus_client.h" ipc/native/src/core/framework ipc/native/src/core/dbinder/source/dbinder_session_object.cpp ipc/native/src/core/dbinder/include/dbinder_session_object.h ipc/native/src/core/dbinder/include/ipc_rpc_ops.h` → 无输出，`exit=1`（`ipc_process_skeleton.h` 已不再 include `dbinder_softbus_client.h`，见 `git diff` 该文件仅一行 include 替换） |
| S-5 | 全 `ipc/native/src/core` 内 RPC 实现符号是否只剩待迁出 TU | 通过（符合 TASK-3 预期） | `grep -rln "DBinderDatabusInvoker\|DatabusSocketListener\|DBinderSoftbusClient" ipc/native/src/core` 仅 7 个文件：4 个待迁 TU（`databus_socket_listener.cpp`、`dbinder_databus_invoker.cpp`、`dbinder_softbus_client.cpp`、`dbinder_softbus_client_death_recipient.cpp`）+ 其 3 个头文件。核心框架目录已 0 命中 |
| S-6 | `IpcRpcOps` 的 9 个字段是否全部落到正确入口 | 通过，9/9 | 逐字段核对（`ipc_rpc_ops.h:48-69`）：`updateClientSession` → `ipc_object_proxy.cpp:1102`；`startServerListener` → `ipc_process_skeleton.cpp:1325`；`shutdownSocket` → `dbinder_session_object.cpp:46`；`grantPermission` → `ipc_object_stub.cpp:790`；`removePermission` → `ipc_object_stub.cpp:810`；`getPidAndUidFromServiceName` → `ipc_object_proxy.cpp:1142,1182`；`getLocalNodeDeviceId` → `ipc_process_skeleton.cpp:552`；`shutdownListenSocket` → `ipc_process_skeleton.cpp:117`；`getClientFd` → `ipc_object_stub.cpp:260,298`。合计 11 个 `ops->` 调用点，无遗漏、无错配 |
| S-7 | `Get()` / `Peek()` 使用分布是否与 D-09 一致 | 通过 | `IpcRpcOpsGet()` 共 10 处：`ipc_object_proxy.cpp:1073/1135/1175`、`ipc_object_stub.cpp:244/283/785/805`、`ipc_process_skeleton.cpp:547/1319`、`ipc_thread_skeleton.cpp:243`；`IpcRpcOpsPeek()` 恰好 2 处且都在关闭/析构路径：`ipc_process_skeleton.cpp:115`、`dbinder_session_object.cpp:44`。析构/关闭路径 0 处 `Get()` |
| S-8 | 懒加载挂载点是否唯一且不违反热路径约束（D-02/D-03） | 通过，但有偏差（见 M-2） | `ipc_thread_skeleton.cpp:241-248`：仅当 `invoker == nullptr && proto == IF_PROT_DATABUS` 才 `IpcRpcOpsGet()`；`binder_invoker.cpp`、`message_parcel.cpp`、`SendRequestInner` 内 `grep -n "IpcRpcOpsGet"` 均为 0 命中。`InvokerFactory::newInstance` 的 `factoryMutex_` 在 `invoker_factory.cpp:75-96` 内析构后才返回，加载（含外来静态初始化）不在锁内 |
| S-9 | 空表分支的返回码/返回值是否与改造前（HEAD）一致 | 通过，逐点对照 | `git show HEAD:` 对照：`MakeDBinderTransSession` 原「invoker 为空」返回 `false` → 空表 `false`（`ipc_object_proxy.cpp:1075-1077`）；两处 `GetDBinderNegotiationData` 原校验失败返回 `ERR_INVALID_DATA` → 空表同值（`:1137-1139`、`:1177-1179`）；`DBinderIncRefs/DecRefs` 原 `invoker == nullptr` 返回 `IPC_STUB_INVALID_DATA_ERR` → 空表同值（`ipc_object_stub.cpp:245-247`、`:284-286`）；`CreateSessionName` 原失败返回 `""` → 空表 `""`（`:786-789`）；`RemoveSessionName` 原失败返回 `IPC_STUB_REMOVE_SESSION_NAME_ERR` → 空表同值（`:806-808`）；`CreateSoftbusServer` HEAD 在 `listener == nullptr` 时 `return false` → 空表同样 `false`（`ipc_process_skeleton.cpp:1320-1322`，HEAD `:1311-1314`）；`GetLocalDeviceID` HEAD 调用失败仅记日志并返回空 `networkId` → 空表同样返回空串（`:547-553`，HEAD `:541-547`）；`CloseDatabusSession` 空表跳过 shutdown 后仍置 `SOCKET_ID_INVALID`（`dbinder_session_object.cpp:44-49`） |
| S-10 | 头文件依赖方向是否已改为「正向依赖 `ipc_rpc_ops.h`」且无环 | 通过 | `git diff ipc_process_skeleton.h`：`dbinder_softbus_client.h` → `ipc_rpc_ops.h`（单行替换）；`git diff dbinder_session_object.h`：删除 `databus_socket_listener.h`，新增 `ipc_rpc_ops.h`、`ipc_types.h`；`git diff databus_socket_listener.h`：新增 `ipc_rpc_ops.h`、删除 3 个 `static const`。`ipc_rpc_ops.h` 自身只依赖 `<cstdint>/<memory>/<string>` 与 `IRemoteInvoker`/`DBinderSessionObject` 前向声明（`:19-26`），未反向 include 任何 core 头 |
| S-11 | `DBINDER_*` 三常量是否只在唯一位置定义、是否有重复定义/ODR 风险 | 通过 | `grep -rn "DBINDER_PKG_NAME\s*=\|DBINDER_SOCKET_NAME_PREFIX\s*=\|DBINDER_UID_PID_SEPARATOR\s*=" --include=*.h --include=*.cpp .` 仅 `ipc_rpc_ops.h:44-46` 三行（`inline const std::string`）；`databus_socket_listener.h` 原有 3 个 `static const` 已删除。产物侧 `readelf -sW libipc_single.z.so` 显示 3 个符号均为 `LOCAL`，且每个仅 1 份（`:125,127,129`），未因 inline 变成每 TU 一份 |
| S-12 | 既有单测在 TASK-2 单独状态下是否保持通过 | **不通过（13 例）**，根因明确、由 TASK-5 恢复（见 M-1） | 实测：`IPCProcessSkeletonUnitTest` 78/80 通过，失败 `CreateSoftbusServerTest003/004`；`IPCObjectProxyUnitTest` 61/64，失败 `MakeDBinderTransSessionTest002/003/004`；`IpcObjectStubUnitTest` 71/79，失败 `DBinderIncRefsTransactionTest003`、`DBinderDecRefsTransactionTest002`、`GetGrantedSessionNameTest003/004`、`GetSessionNameForPidUidTest003`、`CreateSessionNameTest001`、`RemoveSessionNameTest001`、`DBinderRemoveSessionNameTest003`；对照全绿：`IPCThreadSkeletonUnitTest` 17/17、`IPCSkeletonUnitTest` 23/23、`ProcessSkeletonUnitTest` 20/20、`DBinderCallbackStubUnitTest` 32/32。失败原因统一为 `IpcRpcOpsGet()` 返回 `nullptr`（本阶段无 `libipc_rpc.z.so`），mock 的 `EXPECT_CALL` 未被触发（如 `ipc_process_skeleton_unittest.cpp:428,449`、`ipc_object_proxy_unittest.cpp:1093,1124,1156,1157`、`ipc_object_stub_unittest.cpp:575,581,625,627,945,968,971,1427,1444,1465,1580`）。`DbinderDataBusInvokerUnitTest` 116/117，唯一失败 `GetSelfTokenIDTest002`（`dbinder_databus_invoker_unittest.cpp:2671`，`GetSelfTokenID()` 在宿主返回 0）与本次改动无关（该文件仅补 3 个 include），按环境噪声排除 |
| S-13 | 「计划外必需」的 2 个测试文件改动是否仅为 include 补齐、是否已在 Files 表登记 | 通过（登记完整） | `git diff test/unittest/ipc/native/core/ipc_process_skeleton_unittest.cpp`：仅 `+#include "databus_socket_listener.h"`；`git diff test/fuzztest/.../ipcprocessskeletonmock_fuzzer.h`：仅 `+#include "databus_socket_listener.h"`。两者均已列入 `execution-plan.md:270-271` Files 表，但未同步进「代码范围映射」（见 L-2） |
| S-14 | 是否存在计划外/越界文件 | 存在 1 项越界（见 L-2） | `git status --short` 中 `.gitignore` 新增 `codespec/`、`docs/superpowers/`、`matspec/`、`.matspec-cli/` 四行，不属于 TASK-1~TASK-6 任何一行映射 |
| S-15 | 构建产物是否体现 TASK-2 的预期依赖结构 | 通过（TASK-3 未做，符合预期） | `readelf -d out/x86_64_virt/lib.unstripped/communication/ipc/libipc_single.z.so \| grep NEEDED` 无 `libipc_rpc.z.so`；`readelf --dyn-syms` 无 `IpcRpc*`，符合 AC-1.1 的「仅 IPC 进程不加载 rpc 库」。注：产物仍含 `DBinderSoftbusClient::*`/`DBinderDatabusInvoker` 全局符号，这是 TASK-3 前 4 个 TU 仍在 core 库的正常中间态 |

## B. ④ 外部接口影响

| # | 检查项 | 结论 | 证据（File:Line / 命令输出） |
|---|--------|------|-----------------------------|
| E-1 | 公开头文件/公开接口是否有任何改动 | 通过，零改动 | `git diff --stat interfaces/` → `interfaces/innerkits/ipc_single/BUILD.gn \| 1 +`（仅 TASK-1 的 loader 源文件入编）；`ipc_core/include/`、`c_api/include/`、`libdbinder/include/`、`interfaces/kits/js/` 均无 diff。`git diff --stat -- ipc/native/src/core/framework/include/ipc_object_proxy.h .../ipc_object_stub.h ipc/native/src/core/invoker/include/iremote_invoker.h ipc/native/src/core/dbinder/include/dbinder_base_invoker_define.h interfaces/innerkits/ipc_core/include` → 空输出，`exit=0` |
| E-2 | 是否引入 ABI 不兼容（类布局、虚函数、成员顺序） | 通过，无变化 | 三个被改头文件的 `git diff -U0` 仅 include 行与 3 个 `static const` 删除：`databus_socket_listener.h`（`@@ -31,10 +32,6` 只删常量）、`dbinder_session_object.h`（只改 include 块）、`ipc_process_skeleton.h`（只换一行 include）。未新增/删除成员、未调整成员顺序、未改虚函数签名或顺序、未改构造/析构语义；`ipc_object_proxy.h`/`ipc_object_stub.h` 完全未触碰（AGENTS.md ABI 约束项） |
| E-3 | 导出符号面是否变化 | 通过，无新增可见符号 | `readelf --dyn-syms -W libipc_single.z.so \| grep -i "IpcRpc"` 无命中（loader 符号保持 `local: *` 收口，`libipc_single_map:202` 未改，`git diff --stat` 为空）；`DBINDER_*` 为 `LOCAL`（`readelf -sW :125-130`），未把 inline 常量泄漏为外部可见数据符号。既有 `DBinderSoftbusClient::*` 导出仍保留（`dynsym :285/292/331/353/374/409/422/447`），未在 TASK-2 提前削减 `libdbinder` 依赖面 |
| E-4 | 删除头文件常量是否破坏仓内外 include 者（源码兼容） | 通过，向后兼容 | `databus_socket_listener.h` 新增 `#include "ipc_rpc_ops.h"`（`git diff @@ -20,6 +20,7`），因此所有既有 include 该头并引用 `DBINDER_PKG_NAME` 等常量的 TU（`test/fuzztest/.../databussocketlistener*_fuzzer.cpp`、`test/unittest/**` 共 14 处）无需改动即可继续编译；且该头不在 `ipc_all_deps_config` 的 `include_dirs`（`interfaces/innerkits/ipc_single/BUILD.gn:25-27` 只暴露 `ipc_core/include`），不属于对外安装头文件 |
| E-5 | 成功判定语义（`!= ERR_NONE` → `!= IPC_RPC_SOFTBUS_SUCCESS`）是否等价 | 通过，等价 | `IPC_RPC_SOFTBUS_SUCCESS = 0`（`ipc_rpc_ops.h:38`）；`ERR_NONE = 0`（`interfaces/innerkits/ipc_core/include/ipc_types.h:94`）；`SOFTBUS_CLIENT_SUCCESS = 0`（`ipc/native/src/core/dbinder/include/dbinder_softbus_client.h:29`）。`ipc_object_stub.cpp:790/810` 仍与 `ERR_NONE` 比较，`ipc_process_skeleton.cpp:552` 与 `IPC_RPC_SOFTBUS_SUCCESS` 比较，`CreateSessionName` 空表返回 `""` 与原实现一致 |
| E-6 | 返回值/错误码是否新增或改变 | 通过 | `git diff` 中新增代码引用的返回值只有既有 5 种：`false`、`ERR_INVALID_DATA`、`IPC_STUB_INVALID_DATA_ERR`、`IPC_STUB_REMOVE_SESSION_NAME_ERR`、`""`；未新增错误码符号或数值。返回点计数增加仅因新增空表守卫（`execution-plan.md:305` 已登记），语义遵循 AC-3.1 |
| E-7 | 跨库调用接口方向是否引入新的对外依赖 | 通过（TASK-2 阶段无新 so 依赖） | `readelf -d` 的 NEEDED 列表与改造前一致，无 `libipc_rpc.z.so`；`libipc_single` 仍自包含 4 个 TU，故 TASK-2 未新增/削减任何 DT_NEEDED。真正的依赖反转（`libdbinder → libipc_rpc`）在 TASK-3/TASK-4，TASK-2 不作承诺 |
| E-8 | 日志域变化是否影响外部可观测契约 | 通过，仅诊断归类（见 L-1） | `ipc_object_proxy.cpp` 共 3 处真实转换：`git show HEAD:ipc_object_proxy.cpp \| grep -n LOG_LABEL` → `1134/1169/1220`，对应工作区 `1143/1183/1234`；日志域由 `LOG_ID_RPC_DBINDER_INVOKER`("DBinderBaseInvoker") 变为 `LOG_ID_IPC_PROXY`("IPCObjectProxy"，`:66`)。无日志格式串/级别/触发条件变化，无控制流与返回值变化。注：`execution-plan.md:323` 记为「5 处」，与 HEAD 实际 3 处不符（见 L-1） |
| E-9 | 删除间接 include 是否可能影响依赖「传递可见性」的下游 TU | 已检查，影响面已闭合 | 全仓 `grep -rln "databus_socket_listener.h"` 的 14 个 include 者中，core 侧 2 个随 TASK-3 迁出；其余 12 个测试/fuzz TU 要么已显式 include（S-13），要么经 `databus_socket_listener.h` 自身新增的 `ipc_rpc_ops.h` 继续可见常量。`test:ipc_single_test_static` 保持全量 TU（`test/fuzztest/.../databussocketlistener001_fuzzer/BUILD.gn:35`），fuzz 目标不因类型迁出而断链 |

## 阻塞发现表

| ID | 严重度 | 位置 | 问题 | 证据 | 建议 | 状态 |
|----|--------|------|------|------|------|------|
| — | — | — | 无阻塞发现（0 项 Critical / 0 项 High） | 见 A/B 两表全部「通过」结论；13 例单测失败已定性为 TASK-2 中间态并由 TASK-5 恢复，见 M-1 | — | — |

## 非阻塞建议表

| ID | 严重度 | 类别 | 位置 | 问题与影响 | 证据 | 建议 |
|----|--------|------|------|-----------|------|------|
| M-1 | Medium | ③ 完整性 | `ipc_rpc_loader.cpp:141-145`、`ipc_thread_skeleton.cpp:243` | TASK-2 单独状态下 `libipc_single` 仍编入 4 个 RPC TU，但 `IpcRpcOpsGet()` 因无 `libipc_rpc.z.so` 必返回 `nullptr`，所有 RPC 入口降级为空表分支，导致 13 个既有单测运行期失败。**若 TASK-2 被单独合入，本条应升级为 High/blocking**；按计划由 TASK-5 的 `IPC_RPC_STATIC_LINK`（`ipc_rpc_entry.cpp` + loader 静态分支，D-07）恢复，故在本 Task 内不阻塞 | 实测见 S-12（2+3+8=13 例），失败断言处 `ipc_process_skeleton_unittest.cpp:428,449`、`ipc_object_proxy_unittest.cpp:1093,1124,1156,1157`、`ipc_object_stub_unittest.cpp:575,581,625,627,945,968,971,1427,1444,1465,1580` 均因 `ops == nullptr` 提前返回而未命中 mock | 在 TASK-2 `Actual Result` / `Review Handoff` 与 Gate-2 记录「TASK-2 不可单独合入，须与 TASK-5 同批」；TASK-5 完成后必须复跑这 13 例并回填通过结果 |
| M-2 | Medium | ③ 完整性 | `design.md:223`（D-02）vs `ipc_object_proxy.cpp:1073,1135,1175`、`ipc_object_stub.cpp:244,283,785,805`、`ipc_process_skeleton.cpp:547,1319` | D-02 字面声称「加载触发点唯一（仅 `GetRemoteInvoker(IF_PROT_DATABUS)`）」，实际 `IpcRpcOpsGet()` 有 10 个入口，`GetDBinderNegotiationData`/`CreateSessionName`/`RemoveSessionName`/`GetLocalDeviceID`/`CreateSoftbusServer` 均可在 `GetRemoteInvoker` 之前独立触发加载。功能等价（都是 RPC-only 路径），但与设计文字不符，后续维护者按 D-02 阅读会误判加载时机 | `grep -rn "IpcRpcOpsGet()" ipc/native/src/core` 共 10 处（S-7 清单）；`ipc_object_stub.cpp:785`、`:805` 的函数入口即在 `IpcRpcOpsGet()`，无 `GetRemoteInvoker` 前置 | 回写 design D-02 为「首个 RPC 入口触发，`call_once` 去重」，或在 `execution-plan.md` 该 Task 记录偏差与理由（不改代码） |
| M-3 | Medium | ③ 完整性 | TASK-2 与 TASK-3 之间 | TASK-2 完成后到 TASK-3 产出 `libipc_rpc.z.so` 之前存在功能性倒退窗口：core 库内 4 个 TU 的实现不可达（能力表恒空），RPC 链路整体不可用。提交拆分若按 Task 逐条推送，中间提交会破坏可运行性 | 同 M-1 实测；`readelf -d` 无 `libipc_rpc.z.so`（S-15）且 `IpcRpcOpsGet()` 恒 `nullptr` | 提交/PR 拆分时把 TASK-2 与 TASK-3 合并为同一 commit 或同一 PR；若必须分开，TASK-2 提交信息注明「依赖 TASK-3，不可单独构建发版」 |
| L-1 | Low | ④ 接口影响 | `ipc_object_proxy.cpp:1143,1183,1234` | 3 处 `LOG_LABEL`→`LABEL` 使日志域从 `DBinderBaseInvoker` 变为 `IPCObjectProxy`。仅诊断归类变化，不影响控制流/返回值，但会改变按 domain 过滤的 DFX 规则命中结果。另：`execution-plan.md:323` 的 Actual Result 记为「5 处」，与 HEAD 实际 3 处（`:1134/1169/1220`）不符，属证据笔误 | `git show HEAD:ipc_object_proxy.cpp \| grep -c LOG_LABEL` → `3`；`git diff -U1 ipc_object_proxy.cpp` 显示 3 处 `ZLOGx(LOG_LABEL, ...)` → `ZLOGx(LABEL, ...)`；`ipc_object_proxy.cpp:66` 的 `LABEL` 定义为 `LOG_ID_IPC_PROXY` | 校正 Actual Result 的「5 处」为 3 处；在 TASK-6 的 DFX/日志核对项中同步该 domain 变化，若现网规则按 `DBinderBaseInvoker` 检索这些事件需同步 |
| L-2 | Low | ③ 完整性 | `.gitignore:22-25`、`test/unittest/ipc/native/core/ipc_process_skeleton_unittest.cpp`、`test/fuzztest/ipc/native/mock/ipcprocessskeletonmock_fuzzer/include/ipcprocessskeletonmock_fuzzer.h` | (a) `.gitignore` 新增 `codespec/`、`docs/superpowers/`、`matspec/`、`.matspec-cli/` 属流程工具产物，未出现在任何 Task 的 Files 表/代码范围映射中；(b) 2 个「计划外必需」测试文件已在 TASK-2 Files 表登记，但未同步进「代码范围映射」（`execution-plan.md:656-665` 仅 10 行）。Gate-Final 的「实际文件与映射一致」检查会判为不一致 | `git diff .gitignore`（+4 行）；`execution-plan.md:656-665` 无这 2 个测试文件；S-13 的 `git diff` 证明两者仅补 include | (a) 把 `.gitignore` 变更拆成独立 chore 提交并登记为「流程配置」，或从本变更中移除；(b) 在代码范围映射 TASK-2 行补登这 2 个测试文件（标注「计划外必需」） |

## 结论

- **verdict: PASS**，**blocking_findings: 0**。TASK-2 的调用点收敛、头文件解耦、空表返回码保持、公开接口零改动均已在源码、产物与运行态三个层面独立复核通过。
- 最重要的约束不是「改错了」，而是「中间态不可交付」：M-1 的 13 例单测失败是 TASK-2 单独存在的必然结果，计划内由 TASK-5 恢复；M-3 要求 TASK-2 与 TASK-3 同批提交。评审通过不代表 TASK-2 可独立合入。
- 对外接口（公开头文件、ABI、导出符号面、错误码、成功判定）在 TASK-2 阶段均无变化；唯一可观测差异是 L-1 的日志 domain 归类。