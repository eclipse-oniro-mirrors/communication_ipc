# Task Review — 安全与攻击面（⑤）

## 审查对象

| 项 | 值 |
|----|----|
| Task | TASK-1 能力表契约与加载器（`execution-plan.md` 第 131-211 行） |
| 风险等级 | R2（跨库 ABI、并发与生命周期；本次评审席位 = security） |
| Diff 范围 | 未提交/未跟踪变更：新增 `ipc/native/src/core/dbinder/include/ipc_rpc_ops.h`、`ipc/native/src/core/dbinder/include/ipc_rpc_loader.h`、`ipc/native/src/core/dbinder/source/ipc_rpc_loader.cpp`、`test/unittest/ipc/native/core/ipc_rpc_loader_unittest.cpp`；修改 `interfaces/innerkits/ipc_single/BUILD.gn`（仅 `common_source` 追加 loader）、`test/unittest/ipc/native/core/BUILD.gn`（新增 `IpcRpcLoaderUnitTest` 目标并加入 group）。另观察到 `.gitignore` 被修改（不在 TASK-1 `Files` 表内，见「范围外观察」）。 |
| 验证证据 | `execution-plan.md` TASK-1 「Verification」表（5 项 Final Status=Pass）；本席独立复核见下节，未采信作者结论作为唯一依据。 |
| 对照契约 | `spec.md` US-3 / AC-3.1、AC-3.2、AC-3.3、BR-2、BR-3、BR-5；`design.md` D-01、D-03、D-09、「状态归属与不变量」、「安全基础检查」；先例 `ipc/native/src/core/dbinder/source/dbinder_softbus_client.cpp:142-163`。 |

## 独立复核证据（只读，未改任何源码/计划/契约）

| 复核项 | 方法 | 结果 |
|--------|------|------|
| 7 场景实跑 | 用仓库内 `ipc_rpc_loader.cpp`（非 `/tmp` 副本）重新编译 `harness.cpp` 到 `/tmp/ipc_rpc_recheck/t2` 并执行 | `RESULT PASSED (7 scenarios)`，退出码 0；4 条失败路径各仅打印 1 条 `[IpcRpcLoader] load ... failed, reason: ...`，第二次 `Get()` 无新增日志（AC-3.2 在 loader 层成立） |
| 单次加载点 | `grep -n "dlopen(\|dlsym(" ipc_rpc_loader.cpp` | 仅 `:73`（`dlopen`）与 `:87`（`dlsym`），均在 `LoadOnce()`（`:63-113`）内；`IpcRpcOpsPeek()`（`:123-126`）仅 `atomic::load(acquire)` |
| 失败缓存 | 阅读 `:107-112` | 失败路径显式 `g_ops.store(nullptr, release)` 后 `return`，非「不赋值」冒充缓存；`call_once` 保证不再进入 |
| 生产宏隔离 | `grep -rn "IPC_RPC_UNIT_TEST" --include=*.gn --include=*.gni` | 仅 `test/unittest/ipc/native/core/BUILD.gn:179` 定义；生产 `ipc_single`（`interfaces/innerkits/ipc_single/BUILD.gn:54` 引用 loader 源文件）未定义，`IpcRpcTestReset()`/hooks 不进入生产代码 |
| 加载路径搜索面 | 读 `out/x86_64_virt/packages/phone/system/etc/ld-musl-aarch64.path`；`readelf -d out/x86_64_virt/communication/ipc/libipc_single.z.so` | 搜索路径共 31 个目录，`/system/lib64` 排在 `/system/lib64/platformsdk` 之前；核心库无 `RUNPATH/RPATH`（裸 soname 只能走全局搜索路径）。当前镜像内 `find ... -name "libipc_rpc*"` 无结果（TASK-3 尚未产出） |
| 重入探针（自写，仅 `/tmp`） | 在注入的 `openHook` 内再调用 `IpcRpcOpsGet()`，模拟「rpc 库静态构造器回调 loader」 | 子进程 3s 后仍未返回，被 `SIGALRM` 终止 → `once_flag` 自重入为确定性死锁（见 SEC-02） |
| fork 探针（自写，仅 `/tmp`） | 另一线程在 `LoadOnce` 中时 `fork()`，子进程调用 `Get()` | 子进程正常返回，即子进程重跑了一次加载（每进程各一次 `dlopen`），未出现死锁 → 不作为发现 |
| 日志内容 | 读 `:107-108` 与 `:33-41` | 仅常量库名 + 固定 reason 字符串，无 `dlerror()`、无 deviceId/sessionName/uid/路径 |

## 攻击面

| 入口/信任边界 | 威胁 | 现有防护 | 缺口 | 处置 |
|---------------|------|----------|------|------|
| 首次真实 RPC → `dlopen("libipc_rpc.z.so")`（跨库加载边界） | 不可控搜索路径/同名库替换（CWE-427），被替换库可接管表中授权实现 | 固定 soname；`RTLD_NOW \| RTLD_NODELETE`；目标库按 D-06 安装到只读系统目录 `/system/lib64/platformsdk`（与 `libipc_single.z.so` 同级） | 未固定路径、未校验实际映射路径；裸名解析会先经过 `/system/lib64`、`/vendor/lib64*`、`/system/lib64/module*` 等 30 余个目录，且 `/system/lib64` 优先于实际安装目录 | SEC-01 |
| `dlsym` 结果 → `reinterpret_cast` 后直接调用 `getOps()` 与读 `ops->abiVersion` | 符号类型/签名不符、返回畸形表指针 → 调用期崩溃或任意调用 | 符号名固定；`abiVersion` 精确相等；9 个函数指针全量非空；`ops == nullptr` 先判 | 调用 `getOps()` 与读首个字段前无法做任何有效性校验；`abiVersion` 只覆盖结构布局，不覆盖 STL/编译选项 ABI | 接受（信任边界内，见 SEC-07 记录 ABI 前提） |
| 能力表的授权语义（`grantPermission`/`removePermission`/`getPidAndUidFromServiceName`） | 表被非预期库提供时，跨设备授权与 pid/uid 解析实现被替换，等同击穿 AC-2.3/BR-5 | 能力表不进入 SDK、仅私有头；`libipc_single_map` 末尾 `local: *` 收口，新增测试 seam 不导出 | 与 SEC-01 同源：信任的是「名字解析到的那个对象」，不是「已校验身份的对象」 | SEC-01（同一修复） |
| 加载失败降级路径（空表 → `ERR_NULL_OBJECT`(7) / `IPC_STUB_INVALID_DATA_ERR`(303)） | 失败路径绕过权限校验，或「半加载」状态被当作可用 | loader 返回 `nullptr`；逐行核对 `LoadOnce()` 无任何跳过校验分支；调用点既有错误返回复用（BR-2/BR-5） | ABI/完整度被拒时 rpc so 的静态初始化已完成并被 `InvokerFactory` 注册，`RTLD_NODELETE` 无法回滚；「invoker 可用但 ops 为空」的状态归属未定义 | SEC-03 |
| 加载失败日志 | 高频刷日志；日志泄漏路径/敏感标识 | `call_once` 保证进程内仅 1 条；仅常量字符串，无 `%{private}` 数据 | 无 `dlerror()`，AC-3.1「可定位原因」只到阶段级（同时是防泄漏的正面取舍） | SEC-04 |
| 测试注入 seam（`IpcRpcTestSetHooks`/`IpcRpcTestReset`） | 若进入生产，可重放加载并注入 `openHook`/`symHook` → 任意函数调用 | `#ifdef IPC_RPC_UNIT_TEST` 双向隔离（头文件声明 `ipc_rpc_loader.h:35-43`，实现 `ipc_rpc_loader.cpp:128-140`），生产目标未定义该宏；`local: *` 使其即使编译也不会被外部 `dlsym` | 缺少构建期断言，宏一旦被误加到 `ipc_single` 定义中不会被发现 | SEC-05 |

检查清单（适用项逐项给结论，不适用写理由）：

| 项 | 结论 | 依据 |
|----|------|------|
| 输入校验与长度/边界 | Pass | `ipc_rpc_loader.cpp:89,98,100,102` 依次校验 `dlsym` 非空、`getOps()` 非空、`abiVersion` 精确相等、9 指针全量非空（`:52-59` 逐字段检查）；结构体无变长/长度字段，不存在可被越界读的边界输入。ABI 布局一致性依赖同一次构建（同工具链、同 C++ 运行库，design 约束 7），列为待确认项而非 Pass。 |
| 权限/认证/授权检查 | Pass | 本 Task 不新增、不修改任何权限判定；能力表只做转发，`grantPermission`/`removePermission`/`getPidAndUidFromServiceName` 的判定逻辑与顺序仍在 rpc 实现内。逐行核对 `LoadOnce()` 与 `IpcRpcOpsGet()` 无「加载失败则跳过校验」分支，失败即 fail-closed（BR-5）。唯一授权面风险来自库被替换（SEC-01）。 |
| 敏感数据与日志脱敏 | Pass | `ipc_rpc_loader.cpp:33-41,107-108` 仅输出常量库名与固定 reason；不含 deviceId、sessionName、uid/pid、文件路径或 `dlerror()`；失败仅 1 条（host 实测）。 |
| IPC/Binder 入口参数与 fd 校验 | 不适用 | 本 Task 无 IPC 事务入口、无 fd 收发、无 `MessageParcel` 参与；loader 只被核心库内部函数调用。fd 相关能力（`getClientFd`）仅以函数指针字段形式声明，实现与调用点属 TASK-2/TASK-3。 |
| 并发/竞态/生命周期 | Pass（含 1 项 Medium、1 项 Medium 待 TASK-3 闭环） | `std::call_once` 保护唯一加载；`const IpcRpcOps *` 以 release store / acquire load 发布，读取方看到的非空指针与 rpc so 构造完成有正确的 happens-before（构造在 `dlopen` 返回前完成，store 在其后）；`Peek()` 纯 load，不触发加载、不引入锁（AC-5.1）；8 线程并发用例与 host 实跑一致。缺口：`dlopen` 在 `call_once` 临界区内执行 rpc so 静态构造，自重入即死锁（SEC-02）；`IpcRpcTestReset()` 反析构 + placement new 不在生产编译单元内（已核实），使用前置条件「无并发在飞」仅由测试自身保证，可接受。fork 探针未见死锁，不作为发现。 |
| 错误路径与资源释放 | Pass | 失败路径显式写入 `nullptr` 并返回，无「半写入」；`handle` 不 `dlclose` 是 `RTLD_NODELETE` 语义下的有意选择（且避免卸载已注册静态对象的库）；无 `new`/`malloc`/fd 泄漏；`getOps()` 返回空表时不再读字段。 |

## 发现

| 编号 | 严重度 | File:Line | 问题 | 修复要求 |
|------|--------|-----------|------|----------|
| SEC-01 | Medium | `ipc/native/src/core/dbinder/source/ipc_rpc_loader.cpp:73`（常量 `ipc_rpc_ops.h:34`） | 裸 soname `dlopen("libipc_rpc.z.so", ...)` 依赖全局搜索路径。实测镜像 `system/etc/ld-musl-aarch64.path` 列出的 31 个目录中，`/system/lib64`、`/vendor/lib64`、`/system/lib64/module*` 等均排在真实安装目录 `/system/lib64/platformsdk` 之前，核心库自身无 `RUNPATH`；任何位于其前、可被写入的同名库都会被优先加载。这不是「仅本进程崩溃」问题：loader 把该库提供的 `grantPermission`/`removePermission`/`getPidAndUidFromServiceName` 直接当可信实现，替换库里这些函数就是替换跨设备授权判定（对照 BR-5/AC-2.3）。仓内先例 `dbinder_softbus_client.cpp:152` 同样是裸名加载，本次把它从 1 处扩大到 2 处，注入面扩大。当前未构成可利用（系统/vendor 分区只读、三方应用无法设置目标进程 `LD_LIBRARY_PATH`），故定 Medium。 | 在 TASK-3 确定安装路径时同步固定加载路径，任选其一并在 design D-03 记录：①用 `dladdr(&IpcRpcOpsGet, &info)` 取核心库自身目录后拼同目录绝对路径再 `dlopen`；②按 `target_cpu` 生成固定绝对路径（`/system/lib64/platformsdk/` 或 `/system/lib/...`）；③`dlopen` 成功后用 `dlinfo(handle, RTLD_DI_LINKMAP, ...)` 校验 `l_name` 落在预期系统目录，不符按加载失败处理并缓存空表。禁止在原地以「与先例一致」为由免修；至少需在 design 风险表登记该残余风险与理由。 |
| SEC-02 | Medium | `ipc/native/src/core/dbinder/source/ipc_rpc_loader.cpp:63-121` | `dlopen`（连同 rpc so 的全部静态构造）在 `std::call_once` 临界区内执行；若 rpc 库任何静态初始化路径间接调用 `IpcRpcOpsGet()`，即为同一 `once_flag` 自重入。已用探针复现：注入的 `openHook` 内再次 `Get()` → 子进程 3s 未返回（`SIGALRM`），生产表现为永久死锁而非报错。当前已核查 `DBinderSoftbusClient` 静态单例走 `IF_PROT_BINDER`（`dbinder_softbus_client.cpp:38-40,52-100`），不触达 loader；但 TASK-3 的 entry TU / 注册对象尚未落地，约束未被固化。 | 在 TASK-3 的 `ipc_rpc_entry.cpp` 与 design D-03（或 D-01「不变量」）显式写入：「rpc 库静态初始化期间禁止调用 `IpcRpcOpsGet()`（允许 `Peek()`）」，并在 entry TU 顶部注释说明原因；设备侧首次 RPC 用例作为回归。可选加固：在 `LoadOnce()` 入口用 thread-local 标志检测自重入并直接判失败，避免死锁。 |
| SEC-03 | Medium | `ipc/native/src/core/dbinder/source/ipc_rpc_loader.cpp:94-112`（契约侧 `ipc_rpc_ops.h:45-56`，design D-03） | ABI 版本不匹配或表不完整被拒时，rpc so 已经完成静态初始化并把 `IF_PROT_DATABUS` invoker 注册进 `InvokerFactory`，`RTLD_NODELETE` 又禁止回滚，于是形成「`newInstance()` 能返回 invoker、但 `IpcRpcOpsGet()` 恒为 `nullptr`」的半加载状态。若 TASK-2 的任一调用点以「invoker 非空」推断 RPC 可用，就会退化到未校验实现或解引用空表（`ops->...`）。这直接关系到失败降级是否真的 fail-closed（BR-5/EX-2）。 | 在 design「状态归属与不变量」补一条：**能力可用性的唯一判据是 `ops != nullptr`**，invoker 非空不能作为 ops 可用的替代判据；TASK-2 的 8 个使用项必须「先取 ops、判空、再用 invoker」。TASK-2/TASK-3 实现后由 reviewer 按此逐点核对。 |
| SEC-04 | Low | `ipc/native/src/core/dbinder/source/ipc_rpc_loader.cpp:107-108` | 失败日志只记录阶段（`dlopen failed` 等），未附 `dlerror()`/errno，AC-3.1「可定位原因」的粒度偏粗；反过来看，不打印路径也避免了泄漏，属有意取舍但未在文档中声明。 | 二选一并记录：①在 reason 中附加经脱敏的 `dlerror()`（仅保留库名/错误类别，剔除绝对路径）；②在 design「安全基础检查→错误处理安全」注明「不打印 `dlerror()` 以防路径泄漏，阶段 + 库名足以定位」。不阻塞。 |
| SEC-05 | Low | `ipc/native/src/core/dbinder/source/ipc_rpc_loader.cpp:128-140`、`ipc/native/src/core/dbinder/include/ipc_rpc_loader.h:35-43`、`test/unittest/ipc/native/core/BUILD.gn:179` | 测试 seam（可注入 `openHook`/`symHook` 并反析构 `once_flag` 重放加载）仅由 `IPC_RPC_UNIT_TEST` 隔离，已核实生产目标未定义该宏、且 `libipc_single_map` 的 `local: *` 使这些符号即使被编入也不导出，因此当前风险受限；但缺少构建期断言，宏被误加到 `ipc_single` 的 `defines` 时无任何拦截，届时 `IpcRpcTestSetHooks()` 就成为进程内可重放加载 + 函数指针注入的入口。 | 在 `interfaces/innerkits/ipc_single/BUILD.gn` 增加 `assert(!defined(IPC_RPC_UNIT_TEST))` 式的构建期守护，或把 hooks/Reset 拆到独立测试源文件（仅 `IpcRpcLoaderUnitTest` 引用），使生产 `common_source` 中不存在该 seam。不阻塞。 |
| SEC-06 | Low | `ipc/native/src/core/dbinder/include/ipc_rpc_ops.h:41-43` | 头文件作用域的 `static const std::string DBINDER_PKG_NAME/...` 会在每个包含该头的 TU 各生成一份副本并在库加载期动态初始化。核心库被全部进程加载，属启动期静态初始化开销与 static-init 顺序面（非安全问题，与 AC-1.1「仅 IPC 进程不承担 RPC 开销」方向相反）。 | 改为 `inline constexpr char DBINDER_PKG_NAME[] = "DBinderBus";` 或在 `.cpp` 内单点定义、头文件仅 `extern const char*`。不阻塞，建议与 arch 席位结论合并处理。 |
| SEC-07 | Low | `ipc/native/src/core/dbinder/include/ipc_rpc_ops.h:47-55` | 能力表跨 DSO 传递 `const std::string&`、`std::shared_ptr<DBinderSessionObject>`，`abiVersion` 只能发现结构布局不一致，无法发现 STL ABI/编译选项不一致（例如 `_GLIBCXX_USE_CXX11_ABI`、不同 libc++ 版本）。混镜像时可能在 `abiVersion` 校验之前就发生内存破坏或崩溃。 | 接受当前设计（design 约束 7 已声明同一次构建、同工具链、无跨 DSO CFI），但建议在 D-01 明确写出「参数跨 DSO 传递 STL 类型，依赖同一次构建」这一 ABI 前提；长期可把表内参数收敛为 POD/`const char*`。不阻塞。 |

## 待确认

| 项 | 缺少的事实 | 影响 |
|----|------------|------|
| rpc 库静态初始化是否回调 `IpcRpcOpsGet()` | TASK-3 的 `ipc_rpc_entry.cpp` 与 `InvokerDelegator`/`DBinderSoftbusClient` 静态对象尚未实现，无法静态判定；已核查现有 `DBinderSoftbusClient` 静态路径走 `IF_PROT_BINDER`，不触达 loader | 与 SEC-02 直接相关：若存在回调则首次 RPC 永久死锁。需 TASK-3 落代码后复审 |
| 镜像/搜索路径中是否存在同名 `libipc_rpc.z.so` 冲突 | 当前 `out/x86_64_virt` 镜像无该文件（TASK-3 未产出）；`/vendor`、`/chip_prod`、32 位 `/system/lib` 与各产品 `ld-musl-*.path`/namespace 配置未逐一核对 | 决定 SEC-01 的实际可利用性；不影响其加固建议 |
| 设备侧真实 `dlopen` 行为与失败日志条数 | 本机无 `hdc` 与 `qemu-system-x86_64`，未在设备执行 `IpcRpcLoaderUnitTest` 与首次 RPC 用例 | AC-3.1/AC-3.2 的设备侧证据缺口；host 侧已用同一 `ipc_rpc_loader.cpp` 复现 7/7 |
| 核心库安装目录在目标进程命名空间中的可见性 | 仅核对了 `x86_64_virt` 镜像的 `system/etc/ld-musl-aarch64.path`（含 `/system/lib64/platformsdk`）；app 进程 namespace 配置与 arm/arm64 产品路径未核对 | 既影响 AC-3.1 可用性，也决定 SEC-01 的解析顺序。需设备侧 `cat /proc/<pid>/maps` 与 `dlopen` 用例确认 |

## 范围外观察（提交给 scope 席位，不计入本席结论）

- `.gitignore` 被修改（新增 `codespec/`、`docs/superpowers/`、`matspec/`、`.matspec-cli/`），不在 TASK-1 `Files` 表内。请 scope 席位确认该改动是否应纳入本 Task 或另行登记。

## 结论

- 维度⑤：Pass（无阻塞发现；4 项待确认与设备侧/TASK-3 残余面相关，均不构成 High 风险项）
- 阻塞发现数：0

> 复核边界：本席只读，未修改任何源码、计划或契约；仅新增本证据文件，并在 `/tmp/ipc_rpc_recheck/` 下自建探针（`fork_probe.cpp`、`reentrancy_probe.cpp`）用于验证并发/重入结论，未写入仓库。