---
task_id: TASK-2
risk: R2
seat: arch
verdict: PASS
blocking_findings: 0
---

# Task Review — 架构与性能（① 重复造轮子 / ② 性能）

## 审查对象

| 项 | 值 |
|----|----|
| Task | TASK-2 调用点改造与门控（`execution-plan.md:120`、`execution-plan.md`「TASK-2 详情」） |
| 风险等级 | R2（IPC 核心路径、并发、权限判定）；本次评审席位 = arch（①②） |
| 审查席位 | arch（① 重复造轮子 / ② 性能），只读评审；除本文件外未修改仓库任何文件 |
| Diff 范围 | 未提交工作区：`ipc/native/src/core/framework/source/{ipc_thread_skeleton.cpp, ipc_object_proxy.cpp, ipc_object_stub.cpp, ipc_process_skeleton.cpp}`、`ipc/native/src/core/framework/include/ipc_process_skeleton.h`、`ipc/native/src/core/dbinder/{include,source}/dbinder_session_object.{h,cpp}`、`ipc/native/src/core/dbinder/include/databus_socket_listener.h`、`test/unittest/ipc/native/core/{ipc_object_stub_unittest.cpp,dbinder_databus_invoker_unittest.cpp,BUILD.gn}`、`test/fuzztest/ipc/native/mock/ipcprocessskeletonmock_fuzzer/include/ipcprocessskeletonmock_fuzzer.h` |
| 前置结论 | TASK-1（`ipc_rpc_ops.h` / loader / 加载器单测）已评审通过，本报告仅作为上下文，不重复评审 |
| 对照契约 | `design.md` D-01（`design.md:222`）/ D-03（`:224`）/ D-09（`:230`）/ 「状态归属与不变量」（`:241-258`、`:269-270`）/ 「实现入口」表（`:193-205`）；`execution-plan.md:96-100`（禁止项）、`:120`（TASK-2 完成判据） |
| 验证方式 | 只读代码走查 + 符号/宏/头文件检索 + 既有构建产物时戳核对 + host 侧一次性微基准（仅写 `/tmp`）。按指令**未执行** `./build.sh`，未在设备上运行；设备时延对比未做（见 RR-A3） |
| 关键证据来源 | `git diff`（工作区）、`grep -rn`、`git show HEAD:<file>`（改造前基线）、`out/x86_64_virt/{build.log,build.1794493*.log,error.log}` 与产物时戳、`/tmp/opencode/once_bench.cpp` 运行输出 |

## 检查项-结论-证据

| # | 检查项 | 结论 | 证据（File:Line / 命令输出） |
|---|--------|------|-----------------------------|
| 1 | 能力表是否与既有 `InvokerFactory` / `IRemoteInvoker` 重复造轮子 | 通过（复用为主，新增有据） | (a) 9 个表项中 6 项在原实现里就是 rpc 侧静态能力，`IRemoteInvoker` 没有对应虚函数：`grep -n "UpdateClientSession\|GrantPermission\|GetClientFd" ipc/native/src/core/invoker/include/iremote_invoker.h` 无命中，`iremote_invoker.h:43-143` 仅列 `GetCaller*`/`Flatten*`/`SendRequest` 等；(b) 已在 `IRemoteInvoker` 上的能力被直接复用而未进表：`ipc_object_stub.cpp:249-252` 用 `invoker->GetCallerPid()/GetCallerUid()/GetCallerTokenID()/GetCallerDeviceID()`（`iremote_invoker.h:83-107` 既有虚函数，未改签名）；(c) `design.md:222` 已评估备选 (b)「跨库抽象基类虚函数调用」并否决（跨 DSO vtable/CFI 复杂度、9 次 `dlsym` 分散失败处理）；(d) 核心库确已解除类型依赖：`grep -rn "dbinder_databus_invoker.h\|DBinderDatabusInvoker" ipc/native/src/core` 仅剩 2 个待迁出 TU 及其自身头文件（`dbinder_databus_invoker.cpp:16`、`databus_socket_listener.cpp:19`、`dbinder_databus_invoker.h:28`），属 TASK-3 范围 |
| 2 | 「空表守卫」是否已收敛为无意义的模板化重复 | 通过但保留（NB-A3） | 9 处守卫同形（`const IpcRpcOps *ops = IpcRpcOpsGet(); if (ops == nullptr) { ZLOGE(...); return X; }`），但 `X` 有 5 种且均为改造前既有失败语义：`false`（`ipc_object_proxy.cpp:1077,1137`、`ipc_process_skeleton.cpp:1321`）、`ERR_INVALID_DATA`（`ipc_object_proxy.cpp:1177`）、`IPC_STUB_INVALID_DATA_ERR`（`ipc_object_stub.cpp:246,285,787`）、`IPC_STUB_REMOVE_SESSION_NAME_ERR`（`ipc_object_stub.cpp:807`）、空串（`ipc_object_stub.cpp:789`、`ipc_process_skeleton.cpp:551`）。抽取 helper 只能省掉「取表」一行，且会隐藏各点返回码，收益为负。全仓核对未新增对外错误码，符合 `execution-plan.md:98` 禁止项 |
| 3 | `Get()` / `Peek()` 双访问器是否都必要 | 必要，用点分布正确 | `Peek()` 仅 2 处且都在关闭路径：`ipc_process_skeleton.cpp:115`（`~IPCProcessSkeleton`，位于 `listenSocketId_ > 0` 分支内）、`dbinder_session_object.cpp:44`（`CloseDatabusSession`）；`Get()` 10 处全在业务入口（清单见检查项 7）。若只保留 `Get()`，则析构期会 `dlopen`（`design.md:230` D-09 明确要避免）；若只保留 `Peek()`，首次 RPC 永不加载。双访问器是 D-09 的最小实现，非过度设计 |
| 4 | `DBINDER_*` 常量是否留有重复定义 | 已收敛为单一定义 | `grep -rn "DBINDER_PKG_NAME\s*=\|DBINDER_SOCKET_NAME_PREFIX\s*=\|DBINDER_UID_PID_SEPARATOR\s*="` 全仓仅 `ipc_rpc_ops.h:44-46` 命中；改造前 `databus_socket_listener.h:34-36` 的 `static const std::string` 三行已在本次 diff 删除（`git diff ipc/native/src/core/dbinder/include/databus_socket_listener.h` 显示 `-static const std::string ...` ×3）。由 `static const`（每 TU 一份）改为 `inline const`（进程一份）顺带消除重复构造。遗留一处字面量见 NB-A4 |
| 5 | `usingFlag_` 重试状态机是否成对、有无状态泄漏 | 通过（3 条出口全部复位） | `ipc_thread_skeleton.cpp:226` 置 USE；正常命中缓存走 `:261` 置 IDLE；首次 miss 路径 `:242` 置 IDLE → `:243` 若 `Get()` 非空则 `:244` 置 USE 并重试 `newInstance` → 重试成功在 `:258` 落表后经 `:261` 置 IDLE，重试失败在 `:250` 置 IDLE 后返回 `nullptr`。4 个提前 `return nullptr`（`:209`、`:215`、`:219`、`:223`）均发生在 `:226` 之前，不存在「置 USE 未复位」。该配对是**载荷性**的：`~IPCThreadSkeleton`（`:161-172`）在 `usingFlag_ == INVOKER_USE_MAGIC` 时 `usleep(1)` 自旋等待，漏复位会导致析构挂死。`usingFlag_` 为每线程 TLS 实例（`GetCurrent` 走 `pthread_getspecific`，`:126-146`），且 `GetVaildInstance`（`:92-113`）在 FFRT 跨线程迁移时直接换新实例，因此函数中途的 IDLE 窗口不可能被其它线程观察到 |
| 6 | 并发首次是否可能双 `newInstance` 或状态泄漏 | 无新增竞态（既有 TLS 语义，与 security NB-07 同源） | `invokers_` 为 `IPCThreadSkeleton` 成员（`ipc_thread_skeleton.h:88`），实例经 TLS 获取（`:126-146`），故两个线程各自 miss、各自 `newInstance` 属「各自数组各存一份」，不构成同一数组的竞争；`InvokerFactory::newInstance` 的 `factoryMutex_` 只在函数体内生效（`invoker_factory.cpp:78-90`），对 `creators_` 的读写是串行的；加载侧由 `std::call_once`（`ipc_rpc_loader.cpp:141-144`）串行化，且失败结果 `store(nullptr)` 后不再重试。重试分支带 `proto == IRemoteObject::IF_PROT_DATABUS` 判定（`:243`），Binder 协议不会误触发加载 |
| 7 | 每次 RPC 入口 `call_once` + atomic 的成本是否可接受 | 可接受（量级 ns，且均非设备内 IPC 热路径） | 10 处 `Get()` 的分布与频度：`ipc_object_proxy.cpp:1073`（`MakeDBinderTransSession`，会话建立）、`:1135`/`:1175`（两个 `GetDBinderNegotiationData` 重载，握手期）、`ipc_object_stub.cpp:244`/`:283`（`DBINDER_INC/DEC_REFS` transaction，且在 `IsLocalCalling()` 早退之后）、`:785`/`:805`（session name 事务）、`ipc_process_skeleton.cpp:547`（`GetLocalDeviceID`）、`:1319`（`CreateSoftbusServer`）、`ipc_thread_skeleton.cpp:243`（仅首次 miss）。其中 9 处为「会话建立/鉴权/session name」类低频路径，均不在 `SendRequestInner`/`BinderInvoker`/`MessageParcel` 序列化路径上。实测（host，`/tmp/opencode/once_bench.cpp`，`g++ -O2 -std=c++17 -lpthread`，N=2e7）：`call_once+atomic` 2.660 ns/op，`atomic load` 0.744 ns/op → 相对 4.4 ns 级 `Get()` 与 `Peek()` 的差额，对比一次 RPC 往返（ms 级）可忽略；结论 Keep。注意 host 为 libstdc++/glibc，设备为 clang+libc++/musl，此处只作量级判断。可达性非新增：改造前 `CreateSessionName`（HEAD `ipc_object_stub.cpp:775-777`）走 `DBinderSoftbusClient::DBinderGrantPermission` → `OpenSoftbusClientSo()`（HEAD `dbinder_softbus_client.cpp:165-181`）同样会 `dlopen`。契约冲突见 NB-A1 |
| 8 | 析构/关闭路径是否只用 `Peek()` | 通过 | 全仓 `IpcRpcOpsPeek()` 仅 2 处（检查项 3），`IpcRpcOpsGet()` 的 10 处无一位于任何 `~` 函数或 `ReleaseDatabusProto`/`RemoveSoftbusServer` 等释放路径；`~IPCProcessSkeleton`（`ipc_process_skeleton.cpp:96-124`）与 `DBinderSessionObject::CloseDatabusSession`（`dbinder_session_object.cpp:39-49`）均先 `Peek()` 判空、非空才调用，未加载时静默跳过，符合 `execution-plan.md:99` 禁止项。附带修正：改造前 `CloseDatabusSession` 在 listener 为空时提前 return、未把 `socket_` 置 `SOCKET_ID_INVALID`；新代码无条件置无效（`:48`），状态更一致 |
| 9 | `LOG_LABEL` → `LABEL` 的副作用 | 通过（编译依赖变化的必要跟随，无控制流改变） | `LOG_LABEL` 定义在被移除的 `dbinder_base_invoker_define.h:40`（域 `LOG_ID_RPC_DBINDER_INVOKER` / "DBinderBaseInvoker"），原先经 `dbinder_databus_invoker.h` 传递进 `ipc_object_proxy.cpp`。改造前该文件 3 处 `LOG_LABEL`（HEAD `:1134`、`:1169`、`:1220`），现为 0 处、统一改用本文件 `LABEL`（`ipc_object_proxy.cpp:66`，`LOG_ID_IPC_PROXY`）；`ipc_object_stub.cpp` 改造前 `LOG_LABEL` 计数即为 0（已用 `LABEL`，`:56`），本次仅新增守卫日志。`git diff` 逐行核对：仅日志宏标签与新增判空语句变化，无返回值/分支条件变化。日志域迁移影响见 NB-A5 与 security NB-06 |
| 10 | 特查：`IpcRpcOpsGet()` 是否在 `InvokerFactory::factoryMutex_` 持锁期间执行 | 否，锁已在返回前释放 | `newInstance` 的 `std::lock_guard<std::mutex>` 作用域为函数体（`invoker_factory.cpp:78-90`），`return it->second();` 执行后即解锁；调用点 `ipc_thread_skeleton.cpp:241`（`factory.newInstance(proto)`）返回后，`:243` 才执行 `IpcRpcOpsGet()`。因此 `dlopen` 与 `libipc_rpc` 外来静态初始化期间不持 `factoryMutex_`。唯一理论锁序：A 线程持 `databusProcMutex_`（`ipc_process_skeleton.cpp:542`）→ `dlopen` → 静态初始化取 `factoryMutex_`；B 线程持 `factoryMutex_` → creator。creator 为 `InvokerDelegator<DBinderDatabusInvoker>`（`dbinder_databus_invoker.h:123`）中的 `new DBinderDatabusInvoker()`，其构造函数为空实现（`dbinder_databus_invoker.cpp:35-39`，仅取 `getpid()/getappuid()`），不取任何框架锁 → 无环，不形成死锁，仅短暂串行。持锁 `dlopen` 本身的性能问题见 NB-A2 |
| 11 | 头文件解耦的编译期收益（性能另一面） | 正向 | `ipc_process_skeleton.h` 以 `ipc_rpc_ops.h` 替换 `dbinder_softbus_client.h`（`git diff` 该行），后者 `#include "dsoftbus_interface.h"`（`dbinder_softbus_client.h:24`，`wc -l dl_deps/dsoftbus_interface.h` = 1253 行）；`git grep -ln "ipc_process_skeleton.h"` = 56 个 TU → 每个 TU 少解析约 1250 行外部头。`ipc_object_proxy.cpp` 同样以 `ipc_rpc_loader.h`（仅 cstdint/memory/string）替换 `dbinder_databus_invoker.h` |
| 12 | 构建产物是否支撑「`ipc_components` 构建通过」的声明 | 与声明一致（未重跑，按既有产物） | `out/x86_64_virt/communication/ipc/libipc_single.z.so` 475344 B、mtime 22:07，晚于最后一次源文件修改 22:06（`ipc_object_proxy.cpp`），其所在构建日志 `build.1794492834.2151887.log` 含 `[113/714] SOLINK communication/ipc/libipc_single.z.so` 且 `grep -c "^FAILED"` = 0。`out/x86_64_virt/error.log`（22:15）记录的 2 条 `use of undeclared identifier 'DBinderSoftbusClient'/'DatabusSocketListener'` 属**中间态**，同文件随即在 22:15 补入这两个头（`git diff test/unittest/ipc/native/core/dbinder_databus_invoker_unittest.cpp` 的 `+#include "databus_socket_listener.h"` / `+#include "dbinder_softbus_client.h"`），22:17 重编产物 `DbinderDataBusInvokerUnitTest` 存在、`build.1794493073.8501704.log` 的 `FAILED` 计数为 0 |

**总体判定：PASS**。12 项检查中 10 项直接通过、2 项通过但带非阻塞建议（#2 守卫形态、#7 文档契约）；未发现 Critical/High，阻塞发现数 0。①② 两个维度未发现「有现成实现却另起炉灶」，也未发现本任务引入的按次 `dlopen`、新增锁或热路径放大。

## 阻塞发现表

| ID | 严重度 | 位置 | 问题 | 影响 | 建议 |
|----|--------|------|------|------|------|
| — | — | — | 无阻塞发现 | — | — |

说明：按评审规则仅 Critical / High 计入阻塞。持锁 `dlopen`（NB-A2）虽违反 `design.md:256` 的字面不变量，但改造前同一路径同样在持锁状态下 `dlopen`（证据见 NB-A2），无新增死锁环与新增阻塞等级，故按 Medium 记录；不影响本任务 PASS。

## 非阻塞建议表

| ID | 严重度 | 位置 | 问题 | 建议 |
|----|--------|------|------|------|
| NB-A1 | Medium | `ipc_rpc_loader.h:27-29` 与 `ipc_object_stub.cpp:244,283,785,805`、`ipc_object_proxy.cpp:1073,1135,1175`、`ipc_process_skeleton.cpp:547,1319` | 头文件契约写「Per-transaction paths must reuse the pointer they already hold instead of calling `Get()` again, otherwise every transaction pays one call_once invocation」，但实现上 9 个业务点都重新 `Get()`，文档与代码互相矛盾。实测代价仅 ~2.66 ns/次（检查项 7），真正需要守住的约束是「设备内 IPC 热路径不调用 `Get()`」（`design.md:269`），当前实现满足后者。 | 优先改注释而非改代码：把 `ipc_rpc_loader.h:27-29` 改为「允许在真实 RPC 入口重复调用 `Get()`（热读为一次已完成 `call_once` + 一次 acquire load）；仅设备内 IPC 热路径与析构路径禁止调用 `Get()`」。若坚持原契约，则在这些点改为「先 `Peek()` 命中则用缓存、只在该点确属首次入口时才 `Get()`」——但这会让「哪一处负责首次加载」变得不可静态判定，不建议 |
| NB-A2 | Medium | `ipc_process_skeleton.cpp:539-552` | `GetLocalDeviceID()` 在 `:542` 取 `databusProcMutex_` 后于 `:547` 调用 `Get()`，即 `dlopen` + 外来静态初始化在持框架锁状态下发生，违反 `design.md:256`「进入 `dlopen` 时不得持有 `usingFlag_` 之外的框架锁」。同文件 `CreateSoftbusServer` 已是正确写法（`:1310` 作用域结束释放 `sessionNameMutex_` 后才在 `:1319` 取表），两处风格不一致。无死锁环（检查项 10），但最坏情况会在 `dlopen` 的全过程阻塞同进程其它 RPC 会话操作。改造前基线同样在持该锁时 `dlopen`（HEAD `ipc_process_skeleton.cpp:534-547` 调 `DBinderSoftbusClient::GetInstance().GetLocalNodeDeviceId`，其内部 `OpenSoftbusClientSo()` 于 HEAD `dbinder_softbus_client.cpp:165-181`/`:198-215` 触发 `dlopen`），故非回归。 | 把取表移到锁外：函数开头先 `const IpcRpcOps *ops = IpcRpcOpsGet(); if (ops == nullptr) { ...; return networkId; }`，再进 `lock_guard` 执行 `getLocalNodeDeviceId`（`pkgName` 只依赖 `getpid()`，无需锁）。与 security NB-01 合并处理 |
| NB-A3 | Low | `ipc_object_stub.cpp:244,283,785,805`、`ipc_object_proxy.cpp:1073,1135,1175`、`ipc_process_skeleton.cpp:547,1319` | 9 处 4 行空表守卫形态完全一致，属计划授权的 fail-closed 模板（`execution-plan.md:84` 要求「调用点必须先取 ops、判空、再使用 invoker」），但重复度较高。由于各点返回码有 5 种（见检查项 2），抽取 helper 的实际收益小于可读性损失。 | 保持现状。建议在 `ipc_rpc_loader.h` 的 `IpcRpcOpsGet()` 注释中显式登记该约定（「每个调用点负责判空并返回自身既有错误码，不提供抛异常/兜底默认值的变体」），使 9 处重复被识别为契约而非疏漏 |
| NB-A4 | Low | `ipc_object_stub.cpp:784` | 会话名拼装仍硬编码 `"_"`：`DBINDER_SOCKET_NAME_PREFIX + std::to_string(uid) + std::string("_") + std::to_string(pid)`，而同名分隔符已由 `ipc_rpc_ops.h:46` `DBINDER_UID_PID_SEPARATOR` 统一，rpc 侧解析用常量（`databus_socket_listener.cpp:286`）。格式化与解析分处两个 DSO，字面量与常量易漂移。 | 同一 Task 内改为 `+ DBINDER_UID_PID_SEPARATOR +`（该 TU 已 include `ipc_rpc_loader.h` → `ipc_rpc_ops.h`，无新增依赖） |
| NB-A5 | Low | `ipc_rpc_ops.h:37-38` | 注释称 `IPC_RPC_SOFTBUS_SUCCESS`「Mirrors softbus `SOFTBUS_OK`（`dl_deps/dsoftbus_interface.h`）」，但唯一消费点为 `ipc_process_skeleton.cpp:552` 的 `ops->getLocalNodeDeviceId(...)`，其实现返回值域是 `DBinderSoftbusClient::SOFTBUS_CLIENT_SUCCESS`（`dbinder_softbus_client.h:29`，同为 0），与 `SOFTBUS_OK`（`dsoftbus_interface.h:1071`，亦为 0）只是数值巧合。 | 注释改为「对齐 `SOFTBUS_CLIENT_SUCCESS`/`SOFTBUS_OK`（两者均为 0）」，或直接引用 `dbinder_softbus_client.h:29`，避免后续按错误来源同步常量 |
| NB-A6 | Low | `ipc_rpc_ops.h:44-46` | 三个常量声明为 `inline const std::string`，引入 3 个进程级动态初始化与静态析构（`inline` 已消除「每 TU 一份」的旧问题，但仍是 `std::string` 而非字面量）。目前使用点只做拼接与 `find`（`ipc_process_skeleton.cpp:546`、`ipc_object_stub.cpp:784`、`databus_socket_listener.cpp:280-286`），无生命周期风险。 | 若关注启动期成本，可改为 `inline constexpr char DBINDER_PKG_NAME[] = "DBinderBus";` 等，并调整使用点为 `std::string(DBINDER_PKG_NAME)`；否则保持现状即可，无需为 3 次构造改接口 |

## 残余风险/转下游项

| ID | 内容 | 承接 Task | 说明 |
|----|------|-----------|------|
| RR-A1 | `libipc_rpc` entry 的静态初始化不得取 `IPCProcessSkeleton` 的锁（`databusProcMutex_` / `sessionNameMutex_`），也不得回调 `GetRemoteInvoker(IF_PROT_DATABUS)` 或 `IpcRpcOpsGet()` | TASK-3 | 当前 diff 的安全前提正是「静态初始化只做 `InvokerFactory::Register` 且 `DBinderDatabusInvoker` 构造为空」（检查项 10）。同源项见 security NB-01/NB-02 |
| RR-A2 | `getClientFd(invoker)` 在 rpc 侧必须只接受 `IF_PROT_DATABUS` 来源并做精确下转 | TASK-3 | 核心侧两处调用点传入的都是 `GetRemoteInvoker(IF_PROT_DATABUS)` 的返回值（`ipc_object_stub.cpp:240,279`），前提成立；待迁出的 `databus_socket_listener.cpp:79-80` 现用 `reinterpret_cast<DBinderDatabusInvoker *>`，实现侧建议改 `static_cast` 以保留编译期类型检查 |
| RR-A3 | 未执行构建与性能测量 | TASK-5 / TASK-6 / CI | 按指令未运行 `./build.sh`；本报告的性能结论来自静态分布分析 + host 微基准，**设备侧时延对比未做**。AC-5.1 的「时延在测量噪声内」仍需 TASK-6 用基线/改后对照补齐 |
| RR-A4 | `Peek()==nullptr` 语义在 `libdbinder` 建立 `DT_NEEDED` 后发生变化 | TASK-4 | 与 security NB-04 同源，本席不重复判定；仅提示该变更会影响「关闭路径零代价」这一性能前提的成立区间 |