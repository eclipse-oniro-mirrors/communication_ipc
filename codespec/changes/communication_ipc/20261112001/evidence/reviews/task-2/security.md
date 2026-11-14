---
task_id: TASK-2
risk: R2
seat: security
verdict: PASS
blocking_findings: 0
---

# Task Review — 安全与攻击面（⑤）

## 审查对象

| 项 | 值 |
|----|----|
| Task | TASK-2 调用点改造与门控（`execution-plan.md`「TASK-2」） |
| 风险等级 | R2（跨库 ABI、并发、生命周期、权限门控） |
| 审查席位 | security（⑤），只读评审；除本文件外未修改仓库任何文件 |
| Diff 范围 | 未提交工作区：`ipc/native/src/core/framework/source/{ipc_thread_skeleton.cpp, ipc_object_proxy.cpp, ipc_object_stub.cpp, ipc_process_skeleton.cpp}`、`ipc/native/src/core/dbinder/source/dbinder_session_object.cpp`、`test/unittest/ipc/native/core/BUILD.gn`、`.gitignore` 等 |
| 对照契约 | `design.md` D-01 / D-03 / D-09、「状态归属与不变量」（5 条不变量）、性能与 DFX 章节（`design.md:271` 日志约束）；`spec.md` AC-3.2 / AC-5.1 / BR-1；`execution-plan.md` TASK-2 Scope 与 TASK-3 义务 |
| 前置结论 | TASK-1（loader / ops 表）已由本席位评审通过，本报告仅将其作为上下文，不重复评审 |
| 验证方式 | 只读代码走查 + 符号/宏检索；按指令**未执行** `./build.sh`，未在设备或 host 运行单测（见「残余风险/转下游项」） |

## 检查项-结论-证据

| # | 检查项 | 结论 | 证据（File:Line） |
|---|--------|------|-------------------|
| 1 | 空表语义：`ops != nullptr` 是否确实是唯一可用性门控，且所有解引用都被保护 | 基本通过（1 处语义不一致，见 NB-03） | 全部 `Get()` 站点 10 处、`Peek()` 站点 2 处均先判空后解引用：`ipc_object_proxy.cpp:1073,1135,1175`、`ipc_object_stub.cpp:244,283,785,805`、`ipc_process_skeleton.cpp:547,1319`、`ipc_thread_skeleton.cpp:243`、`ipc_process_skeleton.cpp:115`、`dbinder_session_object.cpp:44`。所有 `ops->` 调用前均有 `if (ops == nullptr)` 分支并返回既有错误值（`IPC_STUB_INVALID_DATA_ERR` / `false` / `""`），无空表穿透。唯一例外：`ipc_object_stub.cpp:315` `DBinderAddCommAuth` 使用 invoker 虚函数而无 ops 门控，但**不解引用 `ops`**，不构成 fail-open。 |
| 2 | `CreateSessionName` / `RemoveSessionName` 的权限完整性（不得因 rpc 缺失而放宽校验） | 通过 | `ipc_object_stub.cpp:782-797`：表为空时返回 `""`，调用方 `GetGrantedSessionName`（`741-758`，判定在 `747-751`）与 `GetSessionNameForPidUid`（`760-780`，判定在 `769-773`）据此返回 `IPC_STUB_INVALID_DATA_ERR`。`RemoveSessionName`（`798-817`）表为空时 `805` 判空并在 `807` 打日志后返回 `IPC_STUB_REMOVE_SESSION_NAME_ERR`，`data` 入参未被消费、未被改写，调用方对 `ERR_NONE` 的比较逻辑不变。授权判断（uid/tokenId 校验）位于 `244-260` 与 `315-320` 的 transaction 入口，**先于**任何 ops 调用，rpc 缺失不会跳过鉴权，属 fail-closed。 |
| 3 | 静态初始化 / `call_once` 重入安全 | 不通过（不变量未强制，见 NB-02） | `ipc_thread_skeleton.cpp:241-243` 在首次 DATABUS miss 时调用 `IpcRpcOpsGet()`，即触发 `libipc_rpc` 的 `dlopen` 与静态初始化。`ipc_thread_skeleton.cpp:242` 的 `usingFlag_ = INVOKER_IDLE_MAGIC` 只让本函数状态机回到可重入点，而 `218` 仅检查 `exitFlag_`、`222` 接受 `INVOKER_IDLE_MAGIC`，因此 rpc 静态初始化若再次调用 `GetRemoteInvoker(IF_PROT_DATABUS)`，会再次抵达 `243` → `std::call_once` 自重入（C++ 标准为未定义行为，TASK-1 探针实测为确定性死锁）。当前仅由 `ipc_rpc_loader.cpp` 注释文字约束，无运行时保护；`design.md` 不变量「不得在入口静态初始化期间调用 `Get()`」无强制手段。**不阻塞**：现有 rpc 侧静态初始化仅做 `InvokerFactory::Register`（取 `factoryMutex_`），`DBinderDatabusInvoker` 构造函数不取框架锁，故本次 diff 不引入可达死锁；风险转 TASK-3 落实。 |
| 4 | 加载时机与攻击者可达性 / 单次失败缓存 | 通过 | 加载仅由 6 个业务入口触发，且全部位于 DBinder/RPC 语义路径：`ipc_object_proxy.cpp:1073,1135,1175`（DBinder 协商/会话）、`ipc_object_stub.cpp:244,283`（`DBINDER_INC/DEC_REFS` transaction）、`785/805`（session name）、`ipc_process_skeleton.cpp:547`（`GetLocalDeviceID`）、`1319`（`CreateSoftbusServer`）。纯设备内 IPC 不会加载：`IPCSkeleton::GetLocalDeviceID` → `BinderInvoker::GetLocalDeviceID()`（`binder_invoker.cpp:1704`）返回空串，不进入 `IPCProcessSkeleton::GetLocalDeviceID`；`WriteDBinderProxy`（`message_parcel.cpp:138`）与 `AddCommAuth`（`binder_invoker.cpp:362`）均为 DBinder 专用。攻击面等价于「已被允许发起 RPC 的调用方」，未因拆分新增外部可达入口。失败缓存：`ipc_rpc_loader.cpp:85-137` 中 `dlopen` 仅 1 次（`94`）、`dlsym` 仅 1 次（`111`）、失败日志仅 1 条（`132`），失败经 `g_ops.store(nullptr)`（`135` 附近）在 `call_once` 正常返回后固化，后续 `Get()` 不再重试。 |
| 5 | 生命周期与 `CloseDatabusSession` 的 `SOCKET_ID_INVALID` 语义 | 部分通过（见 NB-04） | `dbinder_session_object.cpp:42-49`：先 `Peek()`（`44`，不触发加载），非空才 `shutdownSocket`（`45-46`），随后**无条件** `socket_ = SOCKET_ID_INVALID`（`48`）。`~IPCProcessSkeleton` 路径安全：`ipc_process_skeleton.cpp:112-121` 的 `Peek()` 位于 `listenSocketId_ > 0` 分支内，而 `listenSocketId_` 只能由 `CreateSoftbusServer` 中 `Get()` 成功后的 `startServerListener`（`1319-1325`）赋值，故该路径上表必非空。风险仅在「rpc 库已随 libdbinder 的 DT_NEEDED 载入、但表尚未 `Get()` 过」时关闭会话：fd 不被 shutdown 却被标记为无效，无法再关闭。 |
| 6 | 线程安全：`g_ops` release/acquire、`usingFlag_`、`invokers_[proto]` | 通过（1 处既有缺陷被放大，见 NB-07） | `g_ops` 写端为 `store(..., release)`、读端 `Peek()` 为 `load(acquire)`（`ipc_rpc_loader.cpp:117-126`，TASK-1 已验）。`usingFlag_` 为 `std::atomic` 且 `218` 有损坏值 `ZLOGF` 兜底。`invokers_[proto] = invoker`（`ipc_thread_skeleton.cpp:258`）为非原子写，代码注释自述 "non-thread safe, add lock to protect it"——属既有技术债，本次拆分使首次 miss 后的「建实例 + 落表」窗口从「仅 CONFIG_IPC_SINGLE 关闭时的首次」扩大为「rpc 加载后的首次」，但并发双建只导致重复构造与后者覆盖，不产生空指针或越界。 |
| 7 | 日志安全与 `LOG_LABEL`→`LABEL` 域变更 | 通过（域变更需备案，见 NB-06） | 新增 9 处 `"rpc capability is unavailable"`（`ipc_object_stub.cpp:246,285,787,807`、`ipc_object_proxy.cpp:1075,1137,1177`、`ipc_process_skeleton.cpp:549,1321`）均无格式化参数，不含 deviceId / sessionName / uid / pid / 路径。`dbinder_session_object.cpp:41-42` 保留 `ConvertToSecureString(GetDeviceId())`，未退化为明文。`ipc_object_proxy.cpp:66` 定义 `LABEL = {LOG_CORE, LOG_ID_IPC_PROXY, "IPCObjectProxy"}`；改造前该文件 RPC 路径使用 `LOG_LABEL`（域 `LOG_ID_RPC_DBINDER_INVOKER` / "DBinderBaseInvoker"），现统一为 `IPCObjectProxy`，属日志域迁移而非敏感信息扩散。 |
| 8 | `IPC_RPC_UNIT_TEST` 不得泄漏进生产目标 | 通过 | 全仓检索：宏定义仅 `test/unittest/ipc/native/core/BUILD.gn:179`（`ohos_unittest("IpcRpcLoaderUnitTest")`，`testonly` 语义的 `ohos_unittest`）；条件编译仅 `ipc_rpc_loader.cpp:25,34,49,85,102,152` 与 `ipc_rpc_loader.h:39`。生产目标 `interfaces/innerkits/ipc_single/BUILD.gn` 未定义该宏，`IpcRpcTestReset()` 与 open hook 不进入设备镜像。`test/unittest/ipc/native/core/BUILD.gn:170-171` 注释亦明确该目标「must not link ipc_single_test」。 |

**总体判定：PASS**。8 项检查中 6 项直接通过、2 项通过但带非阻塞项（#1 语义一致性、#5 生命周期边界、#3 不变量未强制）；未发现 Critical 或 High 级问题，阻塞发现数为 0。

## 阻塞发现表

| ID | 严重度 | 位置 | 问题 | 影响 | 建议 |
|----|--------|------|------|------|------|
| — | — | — | 无阻塞发现 | — | — |

说明：按评审规则仅 Critical / High 计入阻塞。本次未发现可被低权限本地应用直接利用、可造成越权或进程级拒绝服务的缺陷；#3 的重入风险属「当前不可达、后续 TASK 可能引入」，按规则不阻塞本任务。

## 非阻塞建议表

| ID | 严重度 | 位置 | 问题 | 建议 |
|----|--------|------|------|------|
| NB-01 | Medium | `ipc_process_skeleton.cpp:542-552` | `GetLocalDeviceID()` 在持有 `databusProcMutex_`（`542` 的 `std::lock_guard`）期间调用 `IpcRpcOpsGet()`（`547`），违反 `design.md` 不变量「加载（dlopen / 外来静态初始化）期间不持框架锁」。当前不死锁（rpc 静态初始化仅取 `factoryMutex_`），但任何后续在 rpc 静态初始化中访问 `IPCProcessSkeleton` 的改动都会形成锁序反转或长阻塞。 | 将 ops 获取移到锁外（先取表、再取锁），或先 `IpcRpcOpsGet()` 预热后再进入临界区；至少在 `547` 上方加注释标注「此处持锁调用加载，禁止在 rpc 静态初始化中回调 `IPCProcessSkeleton`」。 |
| NB-02 | Medium | `ipc_thread_skeleton.cpp:218,222,241-243` | 不变量「不得在入口静态初始化期间调用 `Get()`」只靠 `ipc_rpc_loader.cpp` 注释约束，无运行时保护；`usingFlag_ = INVOKER_IDLE_MAGIC`（`242`）不等于重入锁，重入将以 `call_once` 自重入（UB / 实测死锁）收场。 | 由 TASK-3 在 loader 侧增加「加载中」状态（如 `std::atomic<bool> loading_`，加载中 `Get()` 直接返回 `nullptr` 并可 `ZLOGW` 一次），或提供 `IpcRpcOpsGetDuringInit()` 显式禁入变体；在 `execution-plan.md` TASK-3 验证项中补一条重入用例。 |
| NB-03 | Medium | `ipc_object_stub.cpp:315` | `DBinderAddCommAuth` 走 `GetRemoteInvoker(IF_PROT_DATABUS)` 并使用 `GetCallerUid/GetCallerTokenID` 虚函数，却无 `ops` 门控，与「`ops != nullptr` 为唯一可用性门控」的字面契约不一致（不会 fail-open，但语义例外未备案）。 | 在 `315` 前后加 ops 判空并返回 `IPC_STUB_INVALID_DATA_ERR`，或在 `design.md` 中显式登记「该 transaction 仅依赖 invoker，不依赖 ops 表」的例外及理由。 |
| NB-04 | Medium | `dbinder_session_object.cpp:44-48` | `Peek()==nullptr` 不等于「rpc 未加载」：TASK-4/D-08 后 `libdbinder` 将以 DT_NEEDED 载入 `libipc_rpc`，rpc TU 内创建的 session（`dbinder_databus_invoker.cpp:133,253,544,917`）在表尚未 `Get()` 的场景下关闭，会出现「fd 未 shutdown 却已 `SOCKET_ID_INVALID`」，fd 泄漏至进程退出。 | `Peek()==nullptr` 时不要改写 `socket_`（保留原值供后续重试），或在该路径 `ZLOGW` 记录「table not ready, socket leaked」以便定位；也可断言该路径不可能到达。 |
| NB-05 | Low | `ipc_object_stub.cpp:246,285`、`ipc_object_proxy.cpp:1075` | 新增 9 处 `ZLOGE(..., "rpc capability is unavailable")`，其中 `DBinderIncRefsTransaction`/`DBinderDecRefsTransaction`/`MakeDBinderTransSession` 属按 transaction 触发的路径；在 rpc 库缺失的镜像上可被对端持续触发，与 `design.md:271`「调用点不重复输出同类错误，避免日志刷屏」冲突（日志放大噪声，非敏感信息泄漏）。 | 按 transaction 路径降为 `ZLOGW` 并做进程内限频（或仅首次 `ZLOGE`），保持与设计文档一致。 |
| NB-06 | Low | `ipc_object_proxy.cpp:1139,1179,1234` 等 | `LOG_LABEL` → `LABEL` 使日志域由 `LOG_ID_RPC_DBINDER_INVOKER`（"DBinderBaseInvoker"）变为 `LOG_ID_IPC_PROXY`（"IPCObjectProxy"），安全影响中性，但会打断现网按域检索/告警基线。 | 在变更说明（PR 描述或 `design.md` DFX 章节）中列出日志域迁移清单，便于运维侧同步规则。 |
| NB-07 | Low | `ipc_thread_skeleton.cpp:258` | `invokers_[proto] = invoker` 非原子写，注释自述非线程安全；拆分后首次 miss 的「建实例 + 落表」窗口在 rpc 加载完成后再次出现，并发首调可能重复构造并覆盖（泄漏一个 invoker）。既有缺陷，非本任务引入。 | 后续以 `std::call_once` 或 `invokers_` 加锁修复；本任务至少在 `258` 处保留注释并登记技术债。 |
| NB-08 | Low | `ipc_rpc_loader.cpp:85-137` | 失败缓存对异常路径不成立：若 `DescribeLoaderError` 抛 `bad_alloc`，或注入的 `getOps` 抛异常，`once_flag` 停在未完成态，下次调用会重跑 `dlopen`。 | 在 `LoadOnce` 内以 `try/catch` 包裹并以 `atomic` 标志兜底固化失败态；仅在 OOM/恶意库场景触发，优先级低。 |

## 残余风险/转下游项

| ID | 内容 | 承接 Task / 责任人 | 说明 |
|----|------|-------------------|------|
| RR-01 | 加载路径信任边界（搜索路径、同名库替换、`/system/lib64` 优先于 `platformsdk`） | TASK-3（SEC-01，TASK-1 已转出） | 与 TASK-2 的门控改造正交；本次 diff 未改变加载实现，故不在本任务重复判定。 |
| RR-02 | 重入保护缺失（NB-02）与持锁加载（NB-01） | TASK-3 | 两项均需在 loader 侧或调用侧增加运行时不变量强制，属 TASK-3「不变量落地」范围。 |
| RR-03 | `libdbinder` 对 `libipc_rpc` 的 DT_NEEDED 依赖建立后，`Peek()==nullptr` 的真实含义变化（NB-04） | TASK-4（D-08 落地时） | 依赖关系变更后需重新核验 `CloseDatabusSession` 与 `~IPCProcessSkeleton` 两条关闭路径的前提。 |
| RR-04 | 未执行构建与测试 | 本席 / 交付验证 | 按指令未运行 `./build.sh`，未执行 `ipc_unittest`、`IpcRpcLoaderUnitTest` 及 DBinder moduletest。建议由 TASK-5 或 CI 补齐：`./build.sh --product-name rk3568 --ccache --build-target //foundation/communication/ipc:ipc_components` 与 `//foundation/communication/ipc/test/unittest:ipc_unittest`。本次结论基于静态走查，若构建失败或单测暴露出并发问题，需回退本 PASS 结论重新评估。 |
| RR-05 | `CONFIG_IPC_SINGLE` 宏分支的覆盖 | TASK-5 / CI | 本次新增门控全部位于 `#ifndef CONFIG_IPC_SINGLE`；需确认 `ipc_single` 构建变体下不引入未使用符号或空实现告警。 |