# Task Review — 漏改与外部接口影响（③ 完整性 / ④ 接口影响）

## 审查对象

| 项 | 值 |
|----|----|
| Task | TASK-1 能力表契约与加载器 |
| 风险等级 | R2（跨库 ABI、并发与生命周期）；本次复核未升级 |
| Diff 范围 | 未提交工作树（无 base..head 提交）：`M .gitignore`、`M interfaces/innerkits/ipc_single/BUILD.gn`、`M test/unittest/ipc/native/core/BUILD.gn`、`?? ipc/native/src/core/dbinder/include/ipc_rpc_ops.h`、`?? ipc/native/src/core/dbinder/include/ipc_rpc_loader.h`、`?? ipc/native/src/core/dbinder/source/ipc_rpc_loader.cpp`、`?? test/unittest/ipc/native/core/ipc_rpc_loader_unittest.cpp` |
| 验证证据 | `execution-plan.md` TASK-1 Verification 表（第 197-205 行）；本文件「③ 变更完整性」逐项复核记录；`.ninja_log` 中 `tests/unittest/ipc/ipc/core/IpcRpcLoaderUnitTest` 链接记录（21:43:15）；`out/x86_64_virt/tests/unittest/ipc/ipc/core/IpcRpcLoaderUnitTest` 在 host 经 musl loader 实际执行 7/7 `[ OK ]`、退出码 0 |

## ③ 变更完整性

| 计划文件/入口 | 应有改动 | 实际改动（File:Line） | 处置（已覆盖/漏改/待确认） |
|---------------|----------|------------------------|----------------------------|
| `ipc_rpc_ops.h`（新增） | `IPC_RPC_OPS_ABI_VERSION=1`、`IPC_RPC_LIBRARY_NAME`、`IPC_RPC_GET_OPS_SYMBOL`、`IPC_RPC_SOFTBUS_SUCCESS`、三个 `DBINDER_*` 常量、`IpcRpcOps`（`abiVersion` + 9 指针）、`IpcRpcGetOpsFunc`；仅前向声明 `IRemoteInvoker`/`DBinderSessionObject` | `ipc/native/src/core/dbinder/include/ipc_rpc_ops.h:33-58`（常量 33-43、结构体 45-56、函数类型 58、前向声明 25-26） | 已覆盖 |
| `ipc_rpc_loader.h`（新增） | 声明 `IpcRpcOpsGet()`/`IpcRpcOpsPeek()`；`IPC_RPC_UNIT_TEST` 下 `IpcRpcTestHooks` 与 `IpcRpcTestReset()` | `ipc/native/src/core/dbinder/include/ipc_rpc_loader.h:27`、`:33`、`:35-43` | 已覆盖 |
| `ipc_rpc_loader.cpp`（新增） | `std::once_flag` + `std::atomic<const IpcRpcOps *>`；`dlopen`/`dlsym`；`abiVersion` 与 9 指针全量校验；失败仅一条日志；测试 seam | `ipc/native/src/core/dbinder/source/ipc_rpc_loader.cpp:45-50`（状态）、`:52-59`（9 指针校验）、`:63-113`（`LoadOnce`）、`:73`/`:87`（唯一 `dlopen`/`dlsym`）、`:108`（唯一日志）、`:117-121`/`:123-126`（两访问器）、`:128-140`（seam） | 已覆盖 |
| `ipc_rpc_loader_unittest.cpp`（新增） | 7 个用例：失败缓存一次、成功缓存、8 线程并发同指针、ABI 不匹配、dlsym 缺失、空函数指针、`Peek()` 不加载 | `test/unittest/ipc/native/core/ipc_rpc_loader_unittest.cpp:165`、`:174`、`:185`、`:205`、`:215`、`:223`、`:231` | 已覆盖 |
| `interfaces/innerkits/ipc_single/BUILD.gn`（修改） | `common_source` 增加 `ipc_rpc_loader.cpp`，使核心库与两个测试 so 均含 loader | `interfaces/innerkits/ipc_single/BUILD.gn:54`；`common_source` 被 `ipc_single`（`:111`）与 `ipc_single_test`（`:172` sources）复用 | 已覆盖（`ipc_single_test_static` 见 F5） |
| `test/unittest/ipc/native/core/BUILD.gn`（修改） | 新增独立 `ohos_unittest("IpcRpcLoaderUnitTest")`，只编 loader + 单测，不依赖 `ipc_single_test`，并加入 group | `test/unittest/ipc/native/core/BUILD.gn:170-194`（`defines=["IPC_RPC_UNIT_TEST"]`、sources 仅 2 个、`external_deps` 仅 `googletest:gmock`、`ldflags=["-ldl","-lpthread"]`）、`:218`（group 加入） | 已覆盖 |
| 禁止修改：`interfaces/innerkits/ipc_core/include/` 公开头文件 | 0 处改动 | `git status` 无该目录条目 | 已覆盖 |
| 禁止修改：`ipc/native/src/core/framework/source/` | 0 处改动 | `git status` 无该目录条目 | 已覆盖 |
| 禁止修改：`interfaces/innerkits/ipc_single/libipc_single_map`、根 `BUILD.gn` | 0 处改动 | `git status` 无这两个路径 | 已覆盖 |
| 正常分支（加载成功、缓存命中、并发首次） | 由 `LoadSuccessIsCached`、`ConcurrentFirstUseReturnsSameOps` 覆盖 | `ipc_rpc_loader_unittest.cpp:174-203`；host 与真实 gtest 二进制均 7/7 通过 | 已覆盖 |
| 异常分支（dlopen 失败、dlsym 缺失、ABI 不匹配、9 指针为空） | 5 条失败路径均有用例 | `ipc_rpc_loader_unittest.cpp:165`（dlopen）、`:215`（dlsym）、`:205`（ABI）、`:223`（空指针）；日志单调打印在 `ipc_rpc_loader.cpp:108` | 部分覆盖：`IpcRpcGetOps()` 返回 `nullptr`（`ipc_rpc_loader.cpp:98-99`）无用例，见 F1 |
| 清理分支（无释放、`RTLD_NODELETE`、测试期重置） | 生命周期不承担释放；`IpcRpcTestReset()` 复位 `once_flag` 与原子表 | `ipc_rpc_loader.cpp:73`（`RTLD_NODELETE`）、`:134-139`（reset） | 已覆盖（设计 D-09 明确无显式清理） |
| 兼容分支（`IPC_RPC_STATIC_LINK` 静态回退） | 设计 D-06/D-07 要求开关关闭态走静态直连 | `ipc_rpc_loader.cpp` 全文无 `IPC_RPC_STATIC_LINK` 相关分支 | 漏改（跨 Task 归属缺口，见 F3；不阻塞 TASK-1） |
| `Peek()` 在成功 `Get()` 之后返回同一张表 | D-09/TASK-2 依赖该语义 | `ipc_rpc_loader.cpp:117-126` 实现正确，但用例仅断言 `Peek()` 在未加载时为 `nullptr`（`ipc_rpc_loader_unittest.cpp:231-236`） | 漏改（测试缺口，见 F2） |

复核方式说明：本仓无可用语义索引工具，采用 `grep`/`git status`/`nm -D`/`readelf -d`/`.ninja_log` 与真实执行二进制交叉验证。构建日志 `out/x86_64_virt/build.log`（21:45:16）不含目标名也不含 `build success`，不能单独作为「构建通过」的证据；改以 `.ninja_log` 目标边 + 产物 mtime/size + 二进制实跑三方相互印证（见 F9）。

## ④ 外部接口影响

| 接口/符号 | 类型（inner API/JS API/C API/宏/错误码/跨仓） | 变更 | 调用方与兼容性 | 处置 |
|-----------|-----------------------------------------------|------|----------------|------|
| `OHOS::IpcRpcOps` | inner 私有二进制契约（跨 so） | 新增结构体（`abiVersion` + 9 函数指针），头文件位于 `ipc/native/src/core/dbinder/include`（非安装目录） | 当前仅 loader 与 TASK-1 单测引用；TASK-2/TASK-3 尚未接入。非安装头 → 不构成对外 SDK/ABI 面 | 通过 |
| `OHOS::IpcRpcOpsGet()` / `IpcRpcOpsPeek()` | inner C++ 符号 | 新增 | `nm -D out/x86_64_virt/communication/ipc/libipc_single.z.so` 中 `IpcRpc` 匹配数 = 0，版本脚本已隐藏；仓外调用方 0 | 通过 |
| `IPC_RPC_OPS_ABI_VERSION`、`IPC_RPC_LIBRARY_NAME`、`IPC_RPC_GET_OPS_SYMBOL`、`IPC_RPC_SOFTBUS_SUCCESS`、`IpcRpcGetOpsFunc` | 宏/`constexpr`/类型别名（私有头） | 新增 | 仅本 Task 内使用；不进入公开头、不导出 | 通过 |
| `DBINDER_PKG_NAME`、`DBINDER_SOCKET_NAME_PREFIX`、`DBINDER_UID_PID_SEPARATOR` | 头文件 `static const std::string` | 在 `ipc_rpc_ops.h:41-43` 新增，与 `databus_socket_listener.h:34-36` 同名同值重复定义 | 当前无 TU 同时包含两文件，故可编译；TASK-2 若在同一 TU 同时包含将因命名空间作用域重复定义而编译失败 | 低风险，见 F6 |
| `libipc_single.z.so` 依赖图 | 跨库依赖 | 仅新增编译单元，`dlopen` 为运行时解析 | `readelf -d` 的 `NEEDED` 列表中无 `libipc_rpc.z.so`；库体积 492112 B（前次 492056 B），无 `DT_NEEDED` 反向依赖 | 通过 |
| `libipc_single.z.so` 导出符号集 | ABI | 未新增导出 | `nm -D` 中 `IpcRpc*` 计数 0；既有 `DBinderSoftbusClient`/`DBinderDatabusInvoker` 导出属 TASK-2/TASK-3 迁移范围，本 Task 未改 | 通过 |
| `ipc_single_test` / `ipc_single_test_static` 测试 so | 测试构建产物 | `common_source` 使 `ipc_single_test` 含 loader；`ipc_single_test_static` 用硬编码列表，未含 loader | 两目标产物名不同且未互相链接；`IpcRpcLoaderUnitTest` 仅链自身 2 个 TU | 通过（`ipc_single_test_static` 见 F5） |
| 权限/错误码语义（`IPC_RPC_SOFTBUS_SUCCESS`=0 等） | 错误码契约 | 仅新增常量，无调用点变更 | 本 Task 无调用点；`StartServerListener` 以 `socketId > 0` 表成功、`IPC_RPC_SOFTBUS_SUCCESS` 仅适用于权限与 `getLocalNodeDeviceId`，需 TASK-2 保持 `socketId <= 0` 判失败 | 待确认（TASK-2 范围，见「待确认」表） |
| ioctl / seccomp 策略 | 跨仓（ArkWeb） | 无新增 ioctl command、无参数结构体变更 | 不需要同步 `chromium_ext/sandbox/seccomp-bpf-helpers/baseline_policy_ohos.cc` | 通过（无影响） |

## 发现

| 编号 | 严重度 | File:Line | 问题 | 修复要求 |
|------|--------|-----------|------|----------|
| F1 | Medium | `ipc/native/src/core/dbinder/source/ipc_rpc_loader.cpp:98-99` | `IpcRpcGetOpsFunc` 解析成功但 `getOps()` 返回 `nullptr` 的分支无对应用例；现有「空函数指针」用例（`ipc_rpc_loader_unittest.cpp:223`）走的是 `IsOpsComplete()` 分支（`:102-104`），不能覆盖此路径 | 增加一个用例：`symHook` 返回可调用桩、桩函数返回 `nullptr`，断言 `IpcRpcOpsGet()==nullptr`、`Peek()==nullptr`，且只产生一条失败日志。属 TASK-1 收尾项，不改变契约，可在 TASK-2 前补齐 |
| F2 | Medium | `test/unittest/ipc/native/core/ipc_rpc_loader_unittest.cpp:231-236` | 仅验证 `Peek()` 未加载时返回 `nullptr`，未验证「成功 `Get()` 后 `Peek()` 返回同一指针」；而 TASK-2 的关闭/析构路径与 D-09 都依赖该语义，回归时可能被误改 | 增加用例：先 `IpcRpcOpsGet()`，再断言 `IpcRpcOpsPeek()==IpcRpcOpsGet()` 且 `g_openCount==1` |
| F3 | Medium | `ipc/native/src/core/dbinder/source/ipc_rpc_loader.cpp`（全文无 `IPC_RPC_STATIC_LINK` 分支） | 设计 D-06/D-07 要求开关关闭态走静态直连能力表，但 TASK-1~TASK-6 的 Files 均未列该文件的静态回退改动，形成跨 Task 归属缺口；当前开启态功能不受影响 | 非 TASK-1 阻塞项：在整体变更评审前（最迟 TASK-4 完成前）明确归属并把该分支补入 loader 或 TASK-4/TASK-5 的 Files；补入后需覆盖「静态直连表非空」用例 |
| F4 | Low | `test/unittest/ipc/native/core/ipc_rpc_loader_unittest.cpp:225-226` | 9 指针完整性校验只用 `removePermission` 一处空指针代表全部；`IsOpsComplete()` 的其余 8 个条件无独立覆盖 | 可选：用参数化或循环逐指针置空断言；不要求本 Task 阻塞 |
| F5 | Low | `execution-plan.md:171` | 计划写「核心库与两个测试 so 都要有 loader」，但 `test/BUILD.gn:101-132` 的 `ipc_single_test_static` 使用硬编码 sources，未包含 `ipc_rpc_loader.cpp` | TASK-5 装配 `ipc_single_test_static` 时同步补入 loader；本 Task 不阻塞 |
| F6 | Low | `ipc/native/src/core/dbinder/include/ipc_rpc_ops.h:41-43` | 三个 `DBINDER_*` 常量与 `databus_socket_listener.h:34-36` 重复定义（同名同值、命名空间作用域 `static`）；一旦某 TU 同时包含两文件即编译失败 | TASK-2 删除 `databus_socket_listener.h` 中的原定义（TASK-2 Files 已含该头文件）；删除前不得新增同时包含两头的 TU |
| F7 | Low | `execution-plan.md:119` | TASK 列表验证命令仍写 `--build-target //foundation/...`，而实际执行需去掉前导 `//`（否则 ninja 报 `unknown target`，已在 `execution-plan.md:201` 记录） | 统一 TASK 列表与详情表的命令形式，避免后续 Task 重复踩坑 |
| F8 | Low | `.gitignore` | 修改了 `codespec/`、`docs/superpowers/`、`matspec/`、`.matspec-cli/`，不在 TASK-1 Files 清单内；mtime（18:09）早于 codespec 目录创建，判断为 ODK/环境预置而非 TASK-1 执行产物 | 提交前确认该改动是否随本次变更入库；若属环境配置应单独提交或还原，避免混入功能提交 |
| F9 | Low | `out/x86_64_virt/build.log`（21:45:16，1300 行） | Verification 第 201 行声称「`x86_64_virt build success`」，但该 build.log 既不含目标名也不含 `build success`，该措辞无法由现有日志佐证 | 修正 Verification 证据表述：以 `.ninja_log` 目标边（137371/137374）、产物 mtime/size 和 7/7 实跑结果作为构建证据，不引用不可复现的 success 字样 |

## 待确认

| 项 | 缺少的事实 | 影响 |
|----|------------|------|
| AC-3.1 端到端（空表时调用点返回既有错误码） | 本 Task 仅提供 loader；`ERR_NULL_OBJECT`/`SOCKET_ID_INVALID` 等映射在 TASK-2 调用点实现，当前无代码可查 | 不影响 TASK-1 判定；须在 TASK-2 评审时以调用点单测覆盖，不计入本次阻塞 |
| AC-3.2 端到端（失败不重试、不重复日志） | loader 单测已覆盖进程内语义；跨调用点的「不会每次调用都尝试加载」需 TASK-2 接入后回归 | 同上，转 TASK-2 验证 |
| AC-3.3 端到端（并发首次同结果） | 单测覆盖 8 线程首次加载；真实调用点并发路径（`GetRemoteInvoker` 首次未命中）尚未存在 | 同上，转 TASK-2 验证 |
| `IpcRpcOps` 9 指针集合的最终充分性 | 已核对 core 保留 TU 的跨库调用仅 9 个符号（`ipc_object_proxy.cpp:1098,1133,1168`；`ipc_object_stub.cpp:256,290,778,793`；`ipc_process_skeleton.cpp:113,542,1320`；`dbinder_session_object.cpp:45`），`GetCallerPid/Uid/TokenID/DeviceID` 走核心侧 `IRemoteInvoker` 虚函数，invoker 创建由 rpc 库内 `InvokerDelegator<DBinderDatabusInvoker>` 静态注册完成 | 当前判定充分；若 TASK-2/TASK-3 实现时发现新跨库符号，需回改契约并升 `IPC_RPC_OPS_ABI_VERSION`，届时以 TASK-3 链接期 `--no-undefined` 结果为准 |
| `updateClientSession` 形参差异 | 契约用 `const std::shared_ptr<DBinderSessionObject> &`，真实实现 `dbinder_databus_invoker.cpp:568` 为按值 `std::shared_ptr<DBinderSessionObject>`；`getClientFd` 真实实现为 `const` 成员（`dbinder_databus_invoker.cpp:703`），契约侧通过向下转型后的非 const 指针调用 | 两者均可由 TASK-3 适配层吸收，无 ABI/行为差异；需 TASK-3 提交时确认适配函数签名与 `-Wl,--no-undefined` 通过 |
| `StartServerListener` 成功判定 | 真实 API 返回 socketId（>0 成功），契约注释未写该语义 | 需 TASK-2 保持 `socketId <= 0` 判失败，不能复用 `IPC_RPC_SOFTBUS_SUCCESS` |
| 设备侧真实运行 | 本机无 `hdc`/`qemu-system-x86_64`；已改用 in-tree `ld-musl` 在 host 直接跑设备构建产物 `IpcRpcLoaderUnitTest`，7/7 `[ OK ]`、退出码 0 | 已消除此前「未执行设备二进制」的待确认；宿主机与 x86_64_virt 目标同为 x86-64 + musl，结论可接受。真机/模拟器复跑仍建议作为 TASK-5 回归项 |

## 结论

- 维度③：Pass（计划内文件与禁止修改边界全部合规；正常/异常/清理分支均有实现与用例，3 项测试与跨 Task 缺口已作为 Medium/Low 发现记录，无 High/Critical）
- 维度④：Pass（无新增对外导出符号、无新增 `DT_NEEDED`、无公开头文件与错误码变更；`DBINDER_*` 重复定义与 `socketId` 语义已在发现/待确认中标注，转入 TASK-2/TASK-3）
- 阻塞发现数：0