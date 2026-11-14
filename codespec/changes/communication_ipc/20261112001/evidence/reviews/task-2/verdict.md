---
task_id: TASK-2
risk: R2
independent: true
verdict: PASS
blocking_findings: 0
---

# Task Review Verdict — TASK-2

## 审查方式

| 项 | 值 |
|----|----|
| 宿主与委派 | host = opencode；`can_delegate = true`、`max_parallel = 3`；按固定三席并行派发 `subagent(agent=general)`，三席均为只读，只允许写各自证据文件 |
| 评审席位 | arch（① ②）、scope（③ ④）、security（⑤） |
| Diff 范围 | 未提交工作区：`ipc/native/src/core/framework/source/{ipc_thread_skeleton,ipc_object_proxy,ipc_object_stub,ipc_process_skeleton}.cpp`、`ipc/native/src/core/framework/include/ipc_process_skeleton.h`、`ipc/native/src/core/dbinder/{include,source}/dbinder_session_object.*`、`ipc/native/src/core/dbinder/include/databus_socket_listener.h`，以及 4 个测试/fuzz 的 include 补齐（详见 execution-plan TASK-2 Files 表） |
| 证据文件 | `evidence/reviews/task-2/arch.md`、`scope.md`、`security.md` |

评审基于作者交付的原始代码与原始命令输出，未采信作者结论；三席各自独立读取文件、`git show HEAD:` 基线、构建产物与 `llvm-nm/readelf` 输出。TASK-1 产物（`ipc_rpc_ops.h`、`ipc_rpc_loader.*`）仅作上下文，不在本次评审范围。

## 五维结论

| 维度 | 评审者 | 结论 | 证据文件 | 阻塞发现 |
|------|--------|------|----------|----------|
| ① 重复造轮子 | arch | Pass | `arch.md` | 0 |
| ② 性能 | arch | Pass | `arch.md` | 0 |
| ③ 漏改/完整性 | scope | Pass | `scope.md` | 0 |
| ④ 外部接口影响 | scope | Pass | `scope.md` | 0 |
| ⑤ 安全/攻击面 | security | Pass | `security.md` | 0 |

## 阻塞发现

| 编号 | 严重度 | 维度 | File:Line | 问题 | 状态（Open/Resolved） | 复验依据 |
|------|--------|------|-----------|------|------------------------|----------|
| — | — | — | — | 无 Critical/High 发现 | — | — |

说明：持锁 `dlopen`（arch NB-A2 / security NB-01）虽违反 `design.md` 的字面不变量，但改造前同一路径同样在持锁状态下 `dlopen`（`git show HEAD:ipc_process_skeleton.cpp` + `dbinder_softbus_client.cpp:165-181`），无新增死锁环，按 Medium 记录；`Peek()==nullptr` 的 fd 泄漏与 `usingFlag_` 重入风险均为「拆分后新前提 + 既有缺陷」组合，且已在 `design.md` 风险表与下游 Task 固化，按 Medium/Low 转下游。故本 Task 无未闭环 Critical/High。

## 非阻塞建议与处置

| 编号 | 严重度 | 维度 | 建议 | 处置 |
|------|--------|------|------|------|
| NB-A1 | Medium | ② | `ipc_rpc_loader.h` 的「按次路径禁止 `Get()`」契约与 9 个业务点实现矛盾；真正约束是设备内 IPC 热路径 | **已修复**：改为「允许在真实 RPC 入口调用（热读为已完成 `call_once` + acquire load）；仅设备内 IPC 热路径与析构路径禁止」 |
| NB-A2 / NB-01 | Medium | ②⑤ | `GetLocalDeviceID()` 在 `databusProcMutex_` 内 `dlopen` | **已修复**：取表移到 `lock_guard` 之前；`pkgName` 只依赖 `getpid()`，锁内逻辑不变 |
| NB-A3 | Low | ① | 9 处空表守卫重复但返回码有 5 种，抽 helper 收益为负 | **已修复（契约登记）**：`ipc_rpc_loader.h` 明确「每个调用点负责判空并返回自身既有错误码，不提供抛异常/默认值变体」 |
| NB-A4 | Low | ③ | `CreateSessionName` 硬编码 `"_"`，与 `DBINDER_UID_PID_SEPARATOR` 可能漂移 | **已修复**：改用 `DBINDER_UID_PID_SEPARATOR` |
| NB-A5 | Low | ① | `IPC_RPC_SOFTBUS_SUCCESS` 注释来源指向 `SOFTBUS_OK`，实际消费返回值域是 `SOFTBUS_CLIENT_SUCCESS` | **已修复**：注释改为对齐 `DBinderSoftbusClient::SOFTBUS_CLIENT_SUCCESS` |
| NB-A6 | Low | ② | 三个 `inline const std::string` 常量的动态初始化 | 保持现状：使用点只做拼接/`find`，无生命周期风险；已由 `inline` 消除每 TU 副本 |
| M-1 | Medium | ③ | TASK-2 单独状态下 `IpcRpcOpsGet()` 恒 `nullptr`，实测 13 个既有单测运行期失败 | **转 TASK-5**：由 `IPC_RPC_STATIC_LINK` 恢复；已在 `execution-plan` Actual Result 与 Gate-2 记录「TASK-2 不可单独合入」，TASK-5 完成时复跑这 13 例并回填 |
| M-2 | Medium | ③ | design D-02 声称「加载触发点唯一」，实际 10 个 RPC 入口均可触发 | **已修复（design 回写）**：D-02 改为「主挂载点 + 业务入口防御性兜底，`call_once` 幂等」 |
| M-3 | Medium | ③ | TASK-2→TASK-3 之间存在功能性倒退窗口 | **转提交约束**：TASK-2 必须与 TASK-3 同批提交/同一 PR；已写入 Actual Result 与 Gate-2 |
| L-1 | Low | ④ | `LOG_LABEL`→`LABEL` 使 3 处日志域变为 `IPCProxy`；Actual Result 误记为「5 处」 | **已修复**：Actual Result 更正为「原有 3 处 + 新增 2 处」；日志域迁移转 TASK-6 DFX 核对 |
| L-2 | Low | ③ | (a) `.gitignore` 未登记；(b) 2 个计划外必需测试文件未进代码范围映射 | **已修复 (b)**：代码范围映射补 2 行并标注「计划外必需」；**(a) 转交付说明**：`.gitignore` 属流程工具产物，不随功能提交，由 TASK-6/PR 描述单列 |
| NB-02 | Medium | ⑤ | 静态初始化期重入 `Get()` 只有注释约束，`usingFlag_` 不是重入锁 | **转 TASK-3**：loader 侧增加「加载中」可用性方案或运行时守护，并在 TASK-3 验证项补重入用例 |
| NB-03 | Medium | ⑤ | `DBinderAddCommAuth` 无 ops 门控，与「ops 唯一判据」字面冲突 | **已修复（例外备案）**：`design.md` 状态不变量登记「该事务只依赖 `IRemoteInvoker` 虚函数，以 `invoker != nullptr` 为门控，新增能力表调用点不得援引」 |
| NB-04 | Medium | ⑤ | `Peek()==nullptr` ≠ rpc 未加载；TASK-4 建立 DT_NEEDED 后可能 fd 泄漏 | **转 TASK-4**：`design.md` 风险表新增该行，要求审计所有 `DBinderSessionObject` 创建路径 |
| NB-05 | Low | ⑤ | RPC 事务入口空表日志可被对端反复触发，存在放大面 | **已修复（文档）**：`design.md` 日志条款改为承认放大面并转 TASK-5/TASK-6 评估降级或限频 |
| NB-06 | Low | ⑤ | 日志域迁移会打断现网按域检索 | **转 TASK-6**：DFX/日志核对项同步 domain 变化清单 |
| NB-07 | Low | ⑤ | `invokers_[proto]` 非原子写，并发首调可能重复构造 | 既有缺陷、非本任务引入；保持现有注释，登记为技术债 |
| NB-08 | Low | ⑤ | 异常路径下 `once_flag` 可能停在未完成态 | 仅 OOM/恶意库场景触发，优先级低；转 TASK-6 残余风险 |
| RR-A2 | Medium | ② | rpc 侧 `getClientFd` 需精确接受 `IF_PROT_DATABUS`；`databus_socket_listener.cpp:79-80` 建议 `static_cast` | **转 TASK-3**：已写入 TASK-3 评审强制项 |
| RR-05 | Low | ⑤ | 需确认 `ipc_single` 变体下门控无未使用符号/空实现告警 | **转 TASK-5**：在装配构建时检查 |

## 复验记录

评审后按非阻塞项修改，并重新执行验证（均为原始输出）：

| 检查项 | 命令 | 结果 |
|--------|------|------|
| 类型解耦 | `git grep -n "DBinderDatabusInvoker\|dbinder_databus_invoker.h" -- ipc/native/src/core/framework ipc/native/src/core/dbinder/source/dbinder_session_object.cpp ipc/native/src/core/dbinder/include/dbinder_session_object.h` | 0 命中，退出码 1 |
| listener 解耦 | `git grep -n "DatabusSocketListener\|databus_socket_listener.h\|dbinder_softbus_client.h" -- ipc/native/src/core/framework ipc/native/src/core/dbinder/source/dbinder_session_object.cpp` | 0 命中，退出码 1 |
| `Get()` 范围 | `git grep -n "IpcRpcOpsGet()" -- ipc/native/src/core` | 10 处，全部在 `ipc_thread_skeleton.cpp:243`、`ipc_object_proxy.cpp:1073/1135/1175`、`ipc_object_stub.cpp:244/283/786/806`、`ipc_process_skeleton.cpp:545/1322` |
| `Peek()` 范围 | `git grep -n "IpcRpcOpsPeek()" -- ipc/native/src/core` | 恰好 2 处：`ipc_process_skeleton.cpp:115`、`dbinder_session_object.cpp:44` |
| `DBINDER_*` 唯一真源 | `git grep -n "DBINDER_PKG_NAME\|DBINDER_SOCKET_NAME_PREFIX\|DBINDER_UID_PID_SEPARATOR" -- ipc/native/src interfaces` | 仅 `ipc_rpc_ops.h:47-49` 定义；其余为使用点 |
| 组件构建（修复后） | `./build.sh --product-name x86_64_virt --ccache --deps-guard=false --build-target foundation/communication/ipc:ipc_components` | `x86_64_virt build success`，`PIPE_EXIT=0`，耗时 1:36 |
| 单测目标构建 | `--build-target foundation/communication/ipc/test/unittest/ipc/native/core:ipc_nativecore_unittest` | 构建成功，`PIPE_EXIT=0`（运行期 13 例失败见 M-1） |
| fuzz 目标构建 | `--build-target .../ipcprocessskeletonmock004_fuzzer:IPCProcessSkeletonMock004FuzzTest` | 构建成功，`PIPE_EXIT=0` |

## 门禁结论

- [x] PASS — 无未闭环 Critical/High，`blocking_findings: 0`
- [ ] FAIL — 存在未闭环 Critical/High；Task 标记 `❌ Blocked`
- [ ] BLOCKED — 阻塞原因未解除，Task 标记 `❌ Blocked`

TASK-2 可标记 Done。**但不得单独合入**：中间态 RPC 能力恒不可用（13 例既有单测失败），必须与 TASK-3 同批提交；TASK-5 完成后复跑这 13 例并回填。转出的 TASK-3/TASK-4/TASK-5/TASK-6 项已写入对应 Task 的前置约束与 `design.md` 不变量/风险表，本 Task 不留 Open 项。