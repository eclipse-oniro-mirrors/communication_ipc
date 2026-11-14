# Task Review — 架构与性能（① 重复造轮子 / ② 性能）

## 审查对象

| 项 | 值 |
|----|----|
| Task | TASK-1 能力表契约与加载器（`execution-plan.md`「TASK-1 能力表契约与加载器」） |
| 风险等级 | R2（跨库 ABI、并发与生命周期）；实际未升级 |
| Diff 范围 | 未提交工作区：新增 `ipc/native/src/core/dbinder/include/ipc_rpc_ops.h`、`ipc_rpc_loader.h`、`ipc/native/src/core/dbinder/source/ipc_rpc_loader.cpp`、`test/unittest/ipc/native/core/ipc_rpc_loader_unittest.cpp`；修改 `interfaces/innerkits/ipc_single/BUILD.gn`、`test/unittest/ipc/native/core/BUILD.gn`。另含计划外修改 `.gitignore`（见「待确认」） |
| 验证证据 | `execution-plan.md` TASK-1「Verification」表（5 行，均记 Pass）；本席复核：产物 `code/out/x86_64_virt/tests/unittest/ipc/ipc/core/IpcRpcLoaderUnitTest`（292384 B、ELF x86-64 musl）存在；host harness 重跑 7 场景全 PASS（`-Wall -Wextra` 无告警）；设备侧/真机执行未完成 |

审查依据：`spec.md` US-3 / AC-3.1–3.3、`design.md` D-01 / D-03 / D-09 + 「状态归属与不变量」+ 性能章节；先读代码与原始证据，再对照作者声明。

## ① 重复造轮子

| File:Line | 已有实现/仓内模式 | 判断（复用/新增有据/待确认） | 依据 |
|-----------|-------------------|------------------------------|------|
| `ipc_rpc_loader.cpp:63-113` 全量 `LoadOnce()` | `dbinder_softbus_client.cpp:143-162` `OpenSoftbusClientSo()` | 新增有据 | 后者是实例成员语义：`CHECK_INSTANCE_EXIT_WITH_RETVAL(exitFlag_)` + `std::mutex loadSoMutex_` + `isLoaded_`/`soHandle_` 实例状态，且返回 bool 供调用点重试。本任务需要进程级「至多一次、失败缓存 nullptr、无锁热读」语义（AC-3.2/3.3、D-03），实例级 mutex+refcount 无法表达，直接复用会把 loader 变成单例并改变失败语义。 |
| `ipc_rpc_loader.cpp:73,87` `dlopen`/`dlsym` | 仓内另两处 `dlopen`：`ipc/native/src/core/framework/source/ipc_trace.cpp:94`、`ipc_thread_pool.cpp:51` | 新增有据 | 仓内不存在通用「dlopen 一次 + 函数表校验」helper；其余两处为各自独立的一次性加载，签名与失败语义均不同。抽取新公共 helper 超出 TASK-1 范围，且需改动其他组件，反而不符合任务边界。 |
| `ipc_rpc_loader.cpp:117-126` `IpcRpcOpsGet()`/`IpcRpcOpsPeek()` | 无同类双访问器先例 | 新增有据 | D-09 明确要求分离「可触发加载」与「只读缓存」；仓内 `IPCProcessSkeleton`/`DBinderSessionObject` 析构路径已有的语义是「不加载、直接跳过」，双访问器是该语义的最小编码，未见更简实现。 |
| `ipc_rpc_ops.h:41-43` `DBINDER_PKG_NAME`/`DBINDER_SOCKET_NAME_PREFIX`/`DBINDER_UID_PID_SEPARATOR` | `databus_socket_listener.h:34-36` 已定义同名同值常量 | 复用但重复定义（见 A-01） | 计划「任务间接口」明确要求由 `ipc_rpc_ops.h` 暴露这三个常量，属计划授权的迁移而非擅自复制；但原定义未在同一 Task 删除，形成两份真源。 |
| `ipc_rpc_ops.h:38` `IPC_RPC_SOFTBUS_SUCCESS` | `dl_deps/dsoftbus_interface.h:1071` `SOFTBUS_OK`（=0）；`ERR_NONE` | 新增有据 | 核心库若 include softbus 头会破坏 D-01 的头文件解耦目标，常量自持是解耦的代价；仅重复了一个外部契约取值（见 A-07）。 |
| `test/unittest/ipc/native/core/BUILD.gn:172-194` `ohos_unittest("IpcRpcLoaderUnitTest")` | 同文件 `ipc_unittest` 模板（`BUILD.gn:20-89`） | 复用模板结构但复制配置（见 A-06） | 计划「只读上下文」要求「沿用同一风格但不依赖 `ipc_single_test`」，由于 loader 需以 `IPC_RPC_UNIT_TEST` 单独编译，无法直接套用模板；但 `include_dirs`/`configs`/`resource_config_file` 被逐字复制。 |
| `ipc_rpc_loader.cpp:33-41` 日志宏 | `dbinder_softbus_client.cpp:154` 的 `ZLOGE(LOG_LABEL, ...)`；`log_tags.h` `LOG_ID_IPC_COMMON` | 复用 | 宏体为 `ZLOGE(RPC_LOG_LABEL, ...)`，`RPC_LOG_LABEL = {LOG_CORE, LOG_ID_IPC_COMMON, "IpcRpcLoader"}`，与仓内 hilog 用法一致；`IPC_RPC_UNIT_TEST` 分支改 `fprintf` 是 host 侧可测的必要 seam，未污染设备路径（已用 `out/x86_64_virt/obj/.../ipc_single/ipc_rpc_loader.o` 的符号确认设备分支真实编译）。 |

结论：未发现「明明有现成实现却另起炉灶」的项；`LoadOnce()` 与双访问器的新增有明确设计依据，重复项集中在常量与 BUILD.gn 配置，均以 Medium/Low 记录。

## ② 性能

| File:Line | 场景（启动/热路径/累积循环/增长输入） | 风险 | 结论（Keep/Change/Needs Evidence） | 依据/未验证 |
|-----------|----------------------------------------|------|-------------------------------------|-------------|
| `ipc_rpc_loader.cpp:123-126` `IpcRpcOpsPeek()` | 热路径/关闭路径 | 无 | Keep | 仅一次 `g_ops.load(acquire)`；无锁、无分配、无分支外的字符串构造。满足 design「`Peek()` 代价为一次 atomic load」。 |
| `ipc_rpc_loader.cpp:117-121` `IpcRpcOpsGet()` | 首次 RPC（一次性）；若被误用于按次路径则成热路径 | 低 | Needs Evidence（TASK-2 侧） | `std::call_once` 快路径是一次 atomic 判定，成本极低但非零；设计的约束是「设备内 IPC 热路径不出现 `IpcRpcOpsGet()`」（AC-5.1）。TASK-1 仅提供访问器，实际分布由 TASK-2 决定，故此处不下 Pass 结论；见 A-05。 |
| `ipc_rpc_loader.cpp:63-113` `LoadOnce()` | 启动（仅首次 RPC） | 无 | Keep | `dlopen` 带 `RTLD_NOW`，符号在加载期一次性解析；`RTLD_NODELETE` 避免重入解析与卸载抖动。设计已声明不提供卸载。 |
| `ipc_rpc_loader.cpp:52-59` `IsOpsComplete()` | 首次加载 | 无 | Keep | 9 次指针比较，一次性开销；换来 ABI 早失败。 |
| `ipc_rpc_ops.h:41-43` `static const std::string` ×3 | 启动（每个包含 TU 的静态初始化 + `atexit` 注册） | 低 | Keep（但记录） | 头文件内 `static const std::string` 使每个包含 TU 各生成一份动态初始化与析构注册；当前仅 3 个 TU 包含（loader.h/loader.cpp/unittest），累计 9 次构造 + 9 次析构注册，量级可忽略。属仓内既有写法（`databus_socket_listener.h:34-36`），非本 Task 引入的回归；见 A-04。 |
| `ipc_rpc_ops.h:47-55` 函数指针表 | 每次跨库调用 | 无 | Keep | 表中 `updateClientSession` 等以 `const std::shared_ptr<...> &` 传参，不产生引用计数增减；调用为单次间接跳转。 |
| `ipc_rpc_loader.cpp:94-105` 校验失败分支 | 首次加载（一次性） | 无 | Keep | 校验全在 `call_once` 内，失败后不再进入；无累积成本。 |
| `test/unittest/ipc/native/core/BUILD.gn:172-194` 独立单测目标 | 构建期 | 无（不进入运行时） | Keep | 只编 2 个源文件，构建开销小于复用 `ipc_single_test`。 |

结论：TASK-1 引入的运行时成本为「首次 RPC 一次 `dlopen`+`dlsym`+9 次校验」与「此后每路径一次函数指针跳转 / 关闭路径一次 atomic load」，无按次锁、无按次分配、无增长输入场景。热路径的最终结论取决于 TASK-2 的访问器分布，故维度②在 TASK-1 范围内记 Pass，并保留 A-05 交接项。

## 发现

| 编号 | 严重度 | File:Line | 问题 | 修复要求 |
|------|--------|-----------|------|----------|
| A-01 | Medium | `ipc_rpc_ops.h:41-43`（对照 `databus_socket_listener.h:34-36`） | 三个 `DBINDER_*` 常量在头文件中被重复定义，形成两份真源。当前安全：已确认仓内无任何 TU 同时包含两处（只有 loader.h/loader.cpp/unittest 包含 `ipc_rpc_ops.h`，`dbinder_databus_invoker.cpp:23` 仅含 `databus_socket_listener.h`）。但 TASK-2 要让 `dbinder_session_object.h` 经 `dbinder_databus_invoker.h` 引入 `ipc_rpc_ops.h`，届时若未在同一提交内删除 `databus_socket_listener.h:34-36` 的原定义，会立刻出现同 TU 重复定义编译错误。 | TASK-2 实施时，删除 `databus_socket_listener.h:34-36` 与 `ipc_rpc_ops.h` 迁移必须同一提交完成；并在 TASK-2 验证表增加 `git grep -c "DBINDER_PKG_NAME = " -- ipc/native/src/core` 结果为 1 的守卫。 |
| A-02 | Medium | `ipc_rpc_ops.h:48,50,51,53` | 4 个字段返回 `int32_t` 但语义不一致：`startServerListener` 返回 socketId（`>0` 为成功），`grantPermission`/`removePermission`/`getLocalNodeDeviceId` 按 `0` 为成功。表是唯一跨库契约，缺少逐字段语义注释时，TASK-2 极易对 `startServerListener` 套用统一的 `!= IPC_RPC_SOFTBUS_SUCCESS` 判断，把成功误判为失败（socketId 永不为 0）。 | 在 `IpcRpcOps` 每个整型返回字段上方加一行语义注释（成功取值、失败取值、以及 `startServerListener` 的 socketId 为 `<=0` 即失败）；TASK-2 的 `CreateSoftbusServer` 调用点补一条对应单测。 |
| A-03 | Medium | `ipc_rpc_loader.cpp:76` | `dlopen` 失败原因固定为字面量 `"dlopen failed"`，未取 `dlerror()`（全文件 0 处 `dlerror`）。`RTLD_NOW` 下 `libipc_rpc.z.so` 可能因未定义符号、权限、`ENOENT`、架构不符等多种原因失败，只输出 `"dlopen failed"` 无法区分；且失败结果被 `call_once` 永久缓存、不再重试，这一次日志是唯一现场。AC-3.1 要求「加载失败返回空表且原因可诊断」，当前证据弱于仓内先例 `dbinder_softbus_client.cpp:154`（打印 `dlerror()`）。 | 失败分支改为携带 `dlerror()`（注意 `dlerror()` 只能取一次，需在 `dlopen` 后立即取；`IPC_RPC_UNIT_TEST` 分支同样输出），并同步更新 7 用例中对失败日志文本的断言。 |
| A-04 | Low | `ipc_rpc_ops.h:41-43` | 头文件内 `static const std::string` 导致每个包含 TU 各生成一份动态初始化与 `atexit` 析构注册（当前 3 TU × 3 常量）。无按调用成本，属既有惯例（`databus_socket_listener.h:34-36`）的延续，非本 Task 引入。 | 建议在 TASK-2 一并改为 `constexpr const char *`（或 `inline constexpr std::string_view`）——调用点均为 `std::string` 构造或拼接，改用 `const char *` 不会新增开销，同时消掉静态初始化。若担心与 `databus_socket_listener.h` 的 `std::string` 类型不一致，需在 TASK-2 同提交统一。 |
| A-05 | Low | `ipc_rpc_loader.cpp:117-121`（设计约束见 `design.md` 性能章节、`execution-plan.md:63` AC-5.1） | `IpcRpcOpsGet()` 每次调用都进入 `std::call_once`。契约未禁止在按次路径上使用它；若 TASK-2 在 `ipc_object_stub.cpp` 的 `GetClientFd`（每个 DBinder 事务）用 `Get()` 取表，会形成「每事务一次 `call_once`」的热路径写法，虽非锁但属不必要的固定开销，也与 AC-5.1 的核对口径冲突。 | 在 `ipc_rpc_loader.h:23-33` 的注释中补一句「按次路径优先复用已取得的 `ops` 指针，`Get()` 仅用于首次 RPC 入口」，把约束固化在契约头文件里；TASK-2 实施时据此选择，不依赖 reviewer 记忆。 |
| A-06 | Low | `test/unittest/ipc/native/core/BUILD.gn:172-194` | 新目标逐字复制了 `ipc_unittest` 模板（`:20-89`）的 `module_out_path`/`include_dirs`/`configs`/`resource_config_file` 等约 20 行配置，而非在模板上加避免链接 `ipc_single_test` 的开关。后续模板调整（如新增 `cflags`、`deps` 约束）不会传导到该目标，形成配置漂移；`IPC_RPC_UNIT_TEST` 与 `ldflags = ["-ldl","-lpthread"]` 也与其他测试目标风格不一致。 | 若 `ipc_unittest` 模板可加可选参数（如 `standalone = true` 时跳过 `ipc_single_test` 依赖），优先改成参数化复用；否则在模板定义处加注释说明该独立目标的存在理由与需同步的字段，降低漂移风险。 |
| A-07 | Low | `ipc_rpc_ops.h:38` | `IPC_RPC_SOFTBUS_SUCCESS = 0` 复述了 `dl_deps/dsoftbus_interface.h:1071` 的 `SOFTBUS_OK`。头文件解耦目标使其合理，但这是一份必须与外部库同步的契约取值。 | 在常量上方注释标注来源（softbus `SOFTBUS_OK`）与「变更需同步 softbus 接口」；无需改动取值。 |
| A-08 | Low | `ipc_rpc_loader.cpp:94-110` | `dlopen` 成功后若 ABI 版本不符 / 表项为空 / `IpcRpcGetOps` 返回空，句柄既不 `dlclose` 也不记录。`RTLD_NODELETE` 使 `dlclose` 实为空操作，且 rpc so 的静态初始化（含 `IF_PROT_DATABUS` invoker 注册）此时已执行，故当前无害；但代码意图（「校验失败即当作未加载」）与副作用（库仍驻留、注册已生效）并不一致，读者易误判失败后进程处于干净状态。 | 在校验失败分支加注释说明「句柄故意不释放：`RTLD_NODELETE` 下 `dlclose` 无效，且静态初始化已完成，失败仅表示核心库不使用该表」；若后续需要真正的失败隔离，应在设计层记录而不在本 Task 处理。 |

补充观察（不计入发现，供 scope 席位参考）：

- `execution-plan.md` TASK-1「Verification」矩阵中「失败日志唯一」一行记实际结果「第 110 行 `IPC_RPC_LOG_LOAD_FAIL(failure);`」，实际位置为 `ipc_rpc_loader.cpp:108`，行号已漂移；结论（仅 1 处调用）本身正确。
- `.gitignore` 出现计划外修改（新增 `codespec/`、`docs/superpowers/`、`matspec/`、`.matspec-cli/`），不在 TASK-1「Files」清单内，属范围问题，本席位不作判定。

## 待确认

| 项 | 缺少的事实 | 影响 |
|----|------------|------|
| AC-3.3 并发首次同结果的设备侧证据 | 设备侧 gtest 二进制已构建（`IpcRpcLoaderUnitTest`），但本机无 `hdc`/`qemu-system-x86_64`，未在设备执行；目前只有 host harness（同源 `ipc_rpc_loader.cpp`、7 场景含 8 线程并发）证据。 | 并发正确性结论基于静态阅读 + host 复现，未覆盖设备 musl 下的 `call_once`/`atomic` 实测。若协调者要求设备侧证据，该行只能记「待确认」，不能记 Pass。 |
| `IpcRpcOps` 字段集合对核心侧需求的完备性 | 已逐项把 9 个函数指针映射到现存签名（`dbinder_softbus_client.h:46-55`、`databus_socket_listener.h:70,72,82`、`dbinder_databus_invoker.h:50,55`）并与 design「实现入口」表比对，未发现漏项；但 TASK-2/TASK-3 尚未实施，无法用 `--no-undefined` 链接结果反证。 | 漏项会在 TASK-3 以链接错误暴露，代价后移；当前只能记「静态比对无缺项」，不能记已验证完备。 |
| `libipc_single.z.so` 未包含 loader 符号是否影响 AC-3.1 | 已确认 `ipc_rpc_loader.o` 存在且含 `RPC_LOG_LABEL`/HiLog 符号，但 `libipc_single.z.so`（21:44:59）中 `IpcRpc` 字符串为 0 —— 因暂无调用方，`--gc-sections` 配合 `local: *` 导出表把该 TU 丢弃。TASK-2 接入调用点后应重新出现。 | 对 TASK-1 无害；但 TASK-2 需以「产物含 loader 符号」作为回归守卫，否则可能误判 TASK-1 交付未生效。 |
| `startServerListener` 的 socketId 语义是否与 rpc 侧实现一致 | `ipc_rpc_entry.cpp`（TASK-3）尚未存在，无法核对 RPC 侧实际返回值域；当前只依据 `dbinder_softbus_client.h:46-55` 既有签名推断。 | A-02 的修复（逐字段注释）在契约侧即可闭环，但成功判定的正确性最终需 TASK-3 落地后复核。 |

## 结论

- 维度①：Pass
- 维度②：Pass
- 阻塞发现数：0