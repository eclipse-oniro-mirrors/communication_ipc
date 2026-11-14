---
task_id: TASK-5
risk: R2
seat: arch
verdict: PASS
blocking_findings: 0
---

# TASK-5 Arch Review

评审对象：TASK-5「测试装配与四类场景用例」（`codespec/changes/ipc/draft-20261112-split-rpc-so/execution-plan.md:580-687`）。

席位边界：只评审 ① 重复造轮子 / ② 性能与资源。本次为只读评审，**除本文件外未修改仓库任何文件**，未执行 `git add/commit/checkout/stash`，**未执行 `./build.sh`**。构建结论来自作者留存日志原文，测试结论来自本席位对当前 `out/x86_64_virt` 产物中 6 个二进制的独立重跑与产物级测量——作者日志本身不含退出码，故其 `EXIT=0` 断言不被直接采信。TASK-4 已接受的取舍（A-01/A-03/A-06 等）不计入本任务发现。

## 结论

**PASS，阻塞发现 0 条（无 Critical / High）。6 条发现全部为 Low。**

TASK-5 增量可独立复现的关键事实：

- 增量形态是「3 个 `BUILD.gn` + 2 个新用例文件 + 1 处委托器定义下沉」；`test/BUILD.gn` 恰好 `+6 −0`（2 行 source + 3 行注释 + 1 行 define），`test/unittest/ipc/native/core/BUILD.gn` 恰好 `+67 −0`（纯新增两个 target 与 group 条目，未改既有用例）；
- 测试 so 仍是全量 TU：`libipc_single_test.z.so` 606,944 B（基线 602,552 B，**+4,392 B / +0.73%**），`.text` 317,391→318,613（**+1,222 B**），`DT_NEEDED` 前后均 12 条，导出面 839→839（**+1 `IpcRpcGetOps`，−1 守护变量**），`DBinderDatabusInvoker` 72 / `DBinderSoftbusClient` 17 / `DatabusSocketListener` 29 / `DbinderSoftbusClientDeathRecipient` 4，与基线一致；
- 静态分支确实编入测试 so：动态分支 3 条特征串计数 0、`<static link>` 1、`IpcRpcGetOps`/`IpcRpcOpsGet`/`IpcRpcOpsPeek` 各 1；
- `ipc_single_test_static` 11,006,722 B / 32 成员，含 `ipc_rpc_entry.o`、`ipc_rpc_loader.o`；loader 的 `dlopen|dlsym|dladdr|dlerror` 未定义符号计数 **0**，entry 定义 `IpcRpcGetOps` 1 处；
- 6 个二进制独立重跑全绿：4/4、1/1、16/16、80/80、64/64、79/79（含 TASK-2 登记的 13 个既有失败用例转绿）；
- 三个测试二进制各自只持有一份 loader（分别来自 `libipc_single_test.z.so` 或自带副本），不存在「同进程两份 loader 抢 `call_once`」造成的测试假象。

发现均为**测试资产口径与可维护性**问题（A-01~A-06），不改变「静态形态装配正确、既有用例零回归、生产目标零改动」的判据。

## 维度结论

| 维度 | 结论 | 依据摘要 |
|------|------|---------|
| ① 重复造轮子（loader / 用例实现） | 通过 | 未新增第二份 loader：静态用例直接链接生产 `ipc_single_test`，只调用 `IpcRpcGetOps()`/`IpcRpcOpsGet()`/`IpcRpcOpsPeek()` 与 `IPCThreadSkeleton::GetRemoteInvoker`，不复制加载逻辑（`ipc_rpc_static_link_unittest.cpp:62-116`）；静态/动态两形态共用同一份 `ipc_rpc_loader.cpp`，仅宏切换（`:93/:272`），未出现「测试版 loader」；未引入 Mock 框架，沿用 gtest 与既有 `private = public` 手段 |
| ① 重复造轮子（构建清单） | 通过（带 Low 观察） | `ipc_single_test` 复用 `common_source` 而非复制源列表（`ipc_single/BUILD.gn:265`）；`ipc_single_test_static` 是在既有 source 列表上加 2 行（`test/BUILD.gn:110-111`），没有第二套清单；但测试 target 的 include/config/external_deps/ldflags 已在两个 standalone 目标内手抄，见 A-05 |
| ② 性能与资源（生产面） | 通过 | 本次改动的三个 target 均为测试资产：`ipc_single_test` 与 `ipc_single_test_static` 前者 `install_enable = false`（`ipc_single/BUILD.gn:288`），后者仅被 `test/fuzztest/**` 的 99 处 `deps` 消费、无产品目标依赖；`libipc_single.z.so` 371,584 B、`libipc_rpc.z.so` 177,304 B 与 TASK-4 记录逐字节一致，生产热路径无新增分支 |
| ② 性能与资源（测试资产） | 通过（带 Low 观察） | 测试 so +4,392 B / `.text` +1,222 B、`DT_NEEDED` 不变、导出面净增 0（+1/−1）；静态档案含 bitcode 11,006,722 B；新增 5 个用例运行耗时均 `<1 ms`（gtest 报告 0 ms），无新增线程/IO；但 entry TU 进入静态档案后扩大了 `ipc_single_test_static` 的链接闭包，见 A-03 |

## 独立核验记录

| # | 检查项 | 命令 | 实际输出摘要 |
|---|--------|------|-------------|
| 1 | 作者构建日志是否与声明一致（不重跑构建） | `tail -4 /tmp/opencode/task5_*.log` | `task5_unittest_build3.log` / `task5_static_test_build.log` / `task5_components_build.log` 均 `build successful`；`task5_unittest_build.log`（缺 `hilog/log.h`）与 `build2.log`（`undefined symbol: DBinderDatabusInvoker::DBinderDatabusInvoker()`）均 `build error`，与 U-1 描述逐字吻合；另发现未登记的 `task5_components_test_build.log` 为 `unknown target 'foundation/communication/ipc/test:ipc_components_test'`，佐证 U-2 确实存在 |
| 2 | 6 个二进制独立重跑（不采信作者 `EXIT=0`） | `ld-musl-x86_64.so.1 --library-path <resource>:<comm>:<musl>:/tmp/opencode/t5libs ./tests/unittest/ipc/ipc/core/<T>` | `IpcRpcStaticLinkUnitTest` 4/4、`IpcRpcStaticLinkFailureUnitTest` 1/1、`IpcRpcLoaderUnitTest` 16/16、`IPCProcessSkeletonUnitTest` 80/80、`IPCObjectProxyUnitTest` 64/64、`IpcObjectStubUnitTest` 79/79，`[  PASSED  ]` 与用例数逐项一致 |
| 3 | 新用例是否真正接入 group | `grep -n 'IpcRpc' test/unittest/ipc/native/core/BUILD.gn` | `:256-258` 三个 target 均在 `group("ipc_nativecore_unittest")` 的 deps 内；target 定义在 `:172`（Loader）、`:199`（StaticLink）、`:206`（Failure） |
| 4 | 测试 so 是否保留全量 TU（非目标：不丢既有符号） | `llvm-nm -D --defined-only -C libipc_single_test.z.so \| grep -c <符号>` | `DBinderDatabusInvoker` 72、`DBinderSoftbusClient` 17、`DatabusSocketListener` 29、`DbinderSoftbusClientDeathRecipient` 4（死亡通知对象仍在同 so，可作为 AC-2.2 的测试侧证据）；与基线的死通知 4 条一致 |
| 5 | 静态分支是否真的编入测试 so（排除 `#ifdef` 删实现） | `llvm-strings libipc_single_test.z.so \| grep -cF <串>`；`llvm-nm -D` | `cannot resolve the directory of the core library` 0、`resolved entry is not provided by the expected library` 0、`libipc_rpc.z.so` 0、`<static link>` 1；`IpcRpcGetOps`/`IpcRpcOpsGet`/`IpcRpcOpsPeek` 各 1 |
| 6 | 静态档案内容与「静态分支无 dlopen」 | `llvm-ar t` + `llvm-ar x ... ipc_rpc_{loader,entry}.o` + `llvm-nm -u` | 32 成员，含 `ipc_rpc_entry.o`(421,548 B bitcode)、`ipc_rpc_loader.o`(117,364 B)；loader 未定义 `dlopen/dlsym/dladdr/dlerror` 计数 **0**；entry `--defined-only` 中 `IpcRpcGetOps` 1 |
| 7 | `IPC_RPC_STATIC_LINK` 是否被用来删实现 | `grep -rn IPC_RPC_STATIC_LINK --include=*.gn --include=*.cpp --include=*.h .` | 4 个 define 点（`ipc_single/BUILD.gn:171` 关闭态、`:266` 测试 so、`test/BUILD.gn:165` 静态档案、`BUILD.gn:216` 失败目标）；代码门控仅在 `ipc_rpc_loader.cpp:93/263/272/347` 与 `ipc_rpc_loader.h:75`，**无任何实现文件借宏删除功能** |
| 8 | 测试进程是否出现两份 loader | `llvm-readelf -d tests/unittest/ipc/ipc/core/<T>` | StaticLink → `libipc_single_test.z.so`；Failure → 仅 `libhilog/libc/libc++`；Loader → 仅 `libc/libc++`（后两者自带副本且不依赖测试 so） |
| 9 | 测试 so 导出面与体积增量 | `stat -c %s`；`llvm-nm -D \| wc -l` + `comm`；`llvm-size -A` | 606,944 vs 602,552（+4,392 B）；导出 839→839：`+IpcRpcGetOps`、`−_ZGVN4OHOS21DBinderDatabusInvoker24DBinderDatabusDelegator_E`；`.text` +1,222、`.rodata` −64、`.data.rel.ro` +80、Total +2,601；`DT_NEEDED` 12→12 |
| 10 | 静态用例断言是否覆盖「逐字段」 | `grep -c "EXPECT_STREQ(YesOrNo"`、`grep -c abiVersion` 与人工通读 | `EXPECT_STREQ(YesOrNo` 9、`abiVersion` 1；同一性断言 2 条（`IpcRpcOpsGet()`/`IpcRpcOpsPeek()` 与 provider 同址）；即使某槽为空也会打印槽名（`YesOrNo` 设计） |
| 11 | 负向用例验证的是哪一份 loader | 读 `ipc_rpc_static_link_failure_unittest.cpp` + 目标 sources + `llvm-readelf -d` | 目标 `:206-230` 只编 `ipc_rpc_loader.cpp` + 用例文件，NEEDED 无测试 so → 验证的是自带副本，不是 `libipc_single_test.z.so` 内那份；`ipc_rpc_loader_unittest.cpp` 同样以 hook 替换 `dlopen/dlsym`，全仓**无任何用例真正 `dlopen` `libipc_rpc.z.so`**（见 A-02） |
| 12 | `GetRemoteInvoker(DATABUS)` 是否依赖 SAMgr/SoftBus | 读 `ipc_thread_skeleton.cpp:207-263`、`dbinder_databus_invoker.cpp:33-50` | 该用例只走 `invokers_[proto]` 缓存与 `InvokerFactory::newInstance`；`DBinderDatabusInvoker` 构造仅初始化 `stopWorkThread_/callerPid_/callerUid_/callerDeviceID_/callerTokenID_/firstTokenID_/status_`，无 SoftBus/SAMgr 调用 |
| 13 | `GetRemoteInvoker` 返回对象是否需要 `ReleaseHandle` | 读 `ipc_thread_skeleton.h:88`、`iremote_invoker.h:46`、`dbinder_databus_invoker.cpp:58-62` | `invokers_` 是 `IRemoteInvoker *invokers_[INVOKER_MAX_COUNT]` 裸指针缓存，取用不转移所有权；`ReleaseHandle` 是 binder 句柄引用计数接口，`DBinderDatabusInvoker::ReleaseHandle` 仅打日志 `return true`，与对象生命周期无关 |
| 14 | 静态档案的链接闭包是否被 entry TU 放大 | `llvm-ar x ...` 后 `llvm-nm -u` 三个对象 | `ipc_thread_skeleton.o` 无条件 `U OHOS::IpcRpcOpsGet()`（`CONFIG_IPC_SINGLE` 在全部 `.gn/.gni` 中 0 定义）；静态分支 `loader.o U IpcRpcGetOps`；`entry.o U DBinderSoftbusClient::*`、`U DatabusSocketListener::*`、`U DBinderSocketInfo` 虚表/析构 → 任何拉入 thread skeleton 的消费者会连带拉入 RPC 实现闭包 |
| 15 | 生产目标是否被本次改动波及 | `stat` 生产 so；`grep install_enable` | `libipc_single.z.so` 371,584 B / `libipc_rpc.z.so` 177,304 B，与 TASK-4 记录一致；`ipc_single_test` 与 `ipc_single_test_static` 均不安装（`install_enable = false` / 仅 fuzz 消费），生产镜像体积与热路径零变化 |

## 发现清单

| 编号 | 严重度 | File:Line | 问题 | 证据 | 建议 | 状态 |
|------|--------|-----------|------|------|------|------|
| A-01 | Low | `test/unittest/ipc/native/core/ipc_rpc_static_link_failure_unittest.cpp:36`；口径来源 `execution-plan.md:595` | 新增命名空间作用域可变变量 `int32_t providerCalls = 0;`（内部链接），与计划「新增用例不引入全局静态可变状态」字面冲突。实际风险为零：该二进制只有 1 个用例、无重复运行，且计数在用例内单调递增并逐次断言 | 读文件 `:36`、`:93`、`:114-121`；该二进制 1/1 PASS | 二选一：把计数改为 fixture 的 `static int32_t` 并在 `SetUp` 重置；或把计划口径改为「不引入跨用例共享的可变状态（单用例二进制允许内部链接计数）」。属口径澄清，不需返工 | 未修复（非阻塞） |
| A-02 | Low | `test/unittest/ipc/native/core/BUILD.gn:206-230`、`ipc_rpc_loader_unittest.cpp:224/299`、`ipc_rpc_loader.h:75-95` | 运行期覆盖边界未写明：负向用例链接的是**重新编译的 loader 副本**（未继承 `ipc_rpc_abi_defines`、未定义 `IPC_RPC_UNIT_TEST`），证明的是「loader 源码的静态失败分支」，不是 `libipc_single_test.z.so` 内那份 loader；`IpcRpcLoaderUnitTest` 又以 `openHook/symHook/pathHook/verifyHook` 替换 `dlopen/dlsym/dladdr` 与映像校验，因此**全仓没有任何用例真正加载 `libipc_rpc.z.so`**。`execution-plan.md:668` 把四类场景都归入「覆盖」，容易被读成真实拆分形态已跑通 | 核验 #8（NEEDED 无测试 so）、#11（目标 sources、hook 定义）；`:93/:272` 之外 loader 无形态差异 | 在 `spec.md`/批次 `verdict.md` 的 AC-6.1 映射里显式标注：①关闭态为真实运行；②首次成功/降级为「静态分支 + 注入 provider」的等价覆盖；③真实 `dlopen` 路径仅有产物级证据（TASK-3/4），无运行期用例。若批次要求真实形态运行期证据，应在 TASK-6 用带镜像的环境补一条最小 smoke | 未修复（非阻塞） |
| A-03 | Low | `test/BUILD.gn:110`；`ipc_rpc_entry.cpp:117-128`；`ipc_thread_skeleton.cpp:241-247` | `ipc_single_test_static` 追加 entry TU 后，静态档案的链接闭包被放大：`ipc_thread_skeleton.o` 无条件引用 `IpcRpcOpsGet()`（`CONFIG_IPC_SINGLE` 全仓 0 定义）→ 静态分支 `loader.o` 引用 `IpcRpcGetOps` → `entry.o` 引用 `DBinderSoftbusClient`/`DatabusSocketListener`/`DBinderSocketInfo`。即任何引用 thread skeleton 的 fuzz 目标（如 `ipcthreadskeleton_fuzzer`）从此连带链入 RPC 实现闭包。仅影响测试/fuzz 二进制，不进产品镜像 | 核验 #14 的三个对象未定义符号；档案为 LLVM bitcode（`file` 实测），最终体积受 LTO 与 `--gc-sections` 影响，**未量化** | 不需要改装配（这是「同 so 静态形态」的必然结果，也是对作者自查要点「两份 TU 都补齐」的正确实现）。建议 TASK-6 对 1 个 fuzz 目标做前后体积对照并记录，避免后续把 fuzz 二进制膨胀误判为生产回归 | 未修复（非阻塞） |
| A-04 | Low | `ipc_rpc_static_link_failure_unittest.cpp:77-88`、`ipc_rpc_entry.cpp:117-128`、`ipc_rpc_loader.cpp:56-66`、`ipc_rpc_static_link_unittest.cpp:93-101` | 能力表字段集合的枚举点从 2 处增至 4 处。生产表用指定初始化（新增字段会编译报错，安全），但**负向用例用位置初始化**：若将来在中间插入第 10 个字段，`DummyGetClientFd` 会被静默填到错误的槽、`nullptr // shutdownListenSocket` 注释随之失真，负向用例仍可能「通过」但已不测原目标；静态用例的 9 条逐字段断言也不会因新增字段而失败。该风险面是 TASK-4 A-06 的放大，非新根因 | 读四个位置；`IpcRpcOps` 字段数 9；`sizeof` 无静态断言 | 负向用例改用指定初始化（与生产表一致）；或给 `IpcRpcOps` 加 `static_assert(sizeof(IpcRpcOps) == 4 + 9 * sizeof(void*))` 类的编译期护栏，并在字段数变化时强制四个枚举点同步。属 TASK-1/3 实现范围的延续，本次不返工 | 未修复（非阻塞） |
| A-05 | Low | `test/unittest/ipc/native/core/BUILD.gn:172-193`、`:206-230` | 第二个 standalone 测试目标出现后，`ipc_unittest` 模板无法复用的部分被手抄两份（include_dirs、configs、external_deps、ldflags、`resource_config_file`，约 15 行）。理由成立：模板硬依赖 `ipc_single_test`（`:43-46`），而这两个目标必须绕开它，因此**不是**无理由复制 | 对比 `:20-85` 模板与 `:172-193`、`:206-230` | 在 `test/unittest/ipc/native/core/BUILD.gn` 增加一个 `ipc_standalone_unittest` 模板（与 `ipc_unittest` 的差异只有 deps 与 `module_out_path` 可选覆盖），供这两个目标复用；后续若再加第三类形态，收益更明显 | 未修复（非阻塞） |
| A-06 | Low | `out/.../libipc_single_test.z.so`；`dbinder_databus_invoker.h:132`、`.cpp:41` | U-1 使测试 so 的导出面为「+1/−1」而非纯增：新增 `IpcRpcGetOps`，同时**消失** `_ZGVN4OHOS21DBinderDatabusInvoker24DBinderDatabusDelegator_E`（header 内联变量的初始化守护符）。守护符属编译器内部符号，只被其定义 TU 的静态初始化路径引用，且全量重编译，无实际兼容风险；但作者「保留全量 TU」的自查未提及这条 | 核验 #9 的 `comm` 结果（839→839，added 1 / removed 1）；`_ZN...Delegator_E` 对象符号本身前后都在 | 无需改代码。若 TASK-6 用「测试 so 导出面只增不减」作为证据，需把这条 −1 列出，避免读者以为对比脚本漏算 | 未修复（非阻塞） |

## Review Handoff 两问的明确回答

### 1. 新增用例是否需要显式 mock `IPCThreadSkeleton` 依赖的 SAMgr 环境？

**不需要。** 依据：

- 用例只调用 `IPCThreadSkeleton::GetRemoteInvoker(IF_PROT_DATABUS)`（`ipc_rpc_static_link_unittest.cpp:113-115`）。该函数在 `proto` 合法、`GetCurrent()` 可用、`usingFlag_` 合法后，只做两件事：查 `invokers_[proto]` 缓存，未命中则 `InvokerFactory::newInstance(proto)`（`ipc_thread_skeleton.cpp:207-259`）。整条路径不触碰 SAMgr、SoftBus 或 `DBinderSoftbusClient::GetInstance()`。
- 被创建的 `DBinderDatabusInvoker` 构造函数只初始化 7 个普通成员（`dbinder_databus_invoker.cpp:42-46`），不发 IPC、不注册服务、不建 socket。`DBinderDatabusInvoker::DBinderDatabusDelegator_` 的静态初始化只把工厂函数指针放进 `InvokerFactory` 的表（`dbinder_databus_invoker.cpp:41`、`invoker_factory.h`），同样无外部依赖。
- 实测该二进制 `NEEDED` 只有 `libipc_single_test.z.so`/`libc`/`libc++` 三个库（无 SAMgr/SoftBus 相关库），运行 4/4 通过；除已过滤的 `HiLogAdapter_init: Can't connect to server` 噪声外，本席位未逐行审查全部 stderr，故不主张「无任何其他报错」。因此显式 mock 反而会掩盖「静态形态下工厂自注册是否成功」这一被测事实。

### 2. `GetRemoteInvoker` 返回的对象是否需要 `ReleaseHandle`？

**不需要。** 依据：

- 返回值是 `IRemoteInvoker *`，指针存放在 `IPCThreadSkeleton::invokers_[proto]` 裸指针数组中（`ipc_thread_skeleton.h:88`、`.cpp:258`），由 skeleton 持有并复用；调用方从未获得所有权，也没有对应的 `Acquire`。用例第二次调用拿到同一指针（`:115`），正说明它是缓存对象而非新建对象。
- `ReleaseHandle(int32_t handle)` 是 **binder 句柄引用计数**接口（`iremote_invoker.h:46`），参数是 handle 而不是 invoker 自身；`DBinderDatabusInvoker::ReleaseHandle` 仅打印日志并 `return true`（`dbinder_databus_invoker.cpp:58-62`），不析构对象。对 DATABUS invoker 调用它既无对象生命周期语义，也会在无 handle 的测试上下文中产生误导性日志。
- 与既有用例一致：仓库内 `GetRemoteInvoker` 的调用方均不 `delete`/`ReleaseHandle`（计划 `:632` 已声明「不 `delete`，与既有用例一致」），实测 4/4 通过且无泄漏相关告警。

## 阻塞判定

| 项 | 判定 |
|----|------|
| Critical | 无 |
| High | 无 |
| 阻塞发现数 | **0** |
| 结论 | **PASS**（A-01~A-06 均为 Low，按规则不计阻塞） |

说明：

1. **本任务的判据全部实测成立**：静态形态测试装配（entry TU + `IPC_RPC_STATIC_LINK`）确实生效、既有 13 个失败用例转绿、生产目标零改动、无第二份 loader。这是 TASK-4 A-02 / S-02 / SC-04 所欠「运行期证据」的闭环，批次 `verdict.md` 现在可以引用「关闭开关后既有用例全部通过」。
2. **A-02 是唯一会影响批次叙事的条目**：AC-6.1 的四类场景中，「真实拆分形态的 `dlopen` 路径」仍无运行期用例，只有产物级证据。这不否定 TASK-5（任务目标就是静态形态装配），但批次结论不能表述为「四种形态均已运行期验证」。
3. **A-03 的量化缺口留给 TASK-6**：`ipc_single_test_static` 的 bitcode 体积增长对 fuzz 二进制的实际影响未测，本席位不据此下结论。

## 未独立验证项

| 项 | 原因 | 承接方 |
|----|------|--------|
| `./build.sh` 双目标重跑 | 评审约束明确禁止构建 | 作者日志（`task5_unittest_build3.log` 等）+ 现测产物已覆盖结论 |
| 关闭态（拆分关闭）整树重跑 | `out/x86_64_virt` 已被作者重建为开启态，重跑会覆盖产物 | TASK-4 已用冻结件闭环；本次仅复核开启态测试 so 与静态档案 |
| fuzz 目标链接后的实际体积 | 本次未构建任何 fuzz 目标，档案为 bitcode，LTO 影响无法静态推断 | TASK-6（A-03） |
| 真实 `dlopen(libipc_rpc.z.so)` 运行期路径 | 无镜像/设备，且现无任何用例覆盖该路径 | TASK-6 或批次遗留风险登记（A-02） |
| 真机/`platformsdk` 实盘安装 | 无 `hdc`/设备，`make_packages` 受 preheat 遗留问题阻塞（同 TASK-3/4） | TASK-6 |
| 非 x86_64 产品（arm/arm64） | 本机仅有 x86_64_virt 产物 | TASK-6 多产品抽检 |
