---
task_id: TASK-5
risk: R2
seat: security
verdict: PASS
blocking_findings: 0
---

# TASK-5 安全与攻击面评审（⑤）

## 结论

**`verdict = PASS`，`blocking_findings = 0`。**

TASK-5 只做「测试装配 + 用例补齐 + 一处装配修正 U-1」，没有改动任何生产路径的校验分支，也没有把测试用强符号 provider 纳入产品安装面：

1. **校验没有被放宽**：静态形态 `LoadOnce()`（`ipc_rpc_loader.cpp:272-283`）与动态形态（`:296-345`）调用同一个 `CheckCapabilityTable()`（`:70-85`）；「空表 → ABI 版本 → 9 槽完整性」的顺序一致，任一失败只打 1 条日志并把表缓存为 `nullptr`（`:279` / `:342`），由同一个 `std::call_once`（`:353`）驱动，不存在「部分表被发布」或「失败后重试成功」的窗口。
2. **测试 provider 进不了产品**：`ipc_single_test` 的 `install_enable = false`（`interfaces/innerkits/ipc_single/BUILD.gn:288`）、`visibility = ["$SUBSYSTEM_DIR/*"]`（`:250`），依赖方只有 `test/` 下的单测与 fuzz 目标，`out/x86_64_virt/packages` 中无 `*ipc_single_test*`；失败用例二进制 `IpcRpcStaticLinkFailureUnitTest` 的 `DT_NEEDED` 只有 `libhilog.so` / `libc.so` / `libc++.so`，不含任何 IPC DSO。
3. **产品静态产物没有被改变**：冻结的关闭态 `libipc_single.z.so` 仍是 `IpcRpc` 动态符号 **0**、`IpcRpc` 重定位 **0**、`libipc_rpc` 字符串 **0**、`<static link>` **1**，TASK-4 的关闭态结论在 TASK-5 之后继续成立。
4. **U-1 不 fail-open、不引入新的 TOCTOU**：`DBinderDatabusDelegator_` 由 header 内联改为 RPC 实现 TU 的唯一定义（`dbinder_databus_invoker.h:132`、`dbinder_databus_invoker.cpp:41-42`），注册时机绑定「承载实现的镜像被加载」；`GetRemoteInvoker` 的 miss 路径先取表再重试，仍取不到则返回 `nullptr`。

需治理层登记、但不阻塞的是 **SC-01（Medium，非阻塞）**：测试 so 的导出面与可抢占性和产品静态产物不同——测试 so 无版本脚本，`IpcRpcGetOps` / `IpcRpcOpsGet` / `IpcRpcOpsPeek` 三个符号可通过 `JUMP_SLOT` 被先序 DSO 抢占；产品静态产物 `local: *` 生效且 `IpcRpc` 重定位为 0。因此新用例证明的是 loader/provider 的**逻辑**，不能替代 TASK-6 对产品产物的**二进制级导出断言**。

本席位为只读评审：只写本文件，未执行 `./build.sh`，未改动任何源码/构建文件，未执行 `git add/commit/checkout/stash`。构建与运行证据取自作者保留的日志、`out/x86_64_virt` 现行产物与 `/tmp/opencode` 冻结副本。

## 1. 审查范围与证据口径

| 类别 | 内容 |
|---|---|
| 变更文件（TASK-5） | `interfaces/innerkits/ipc_single/BUILD.gn`（`ipc_single_test` 追加 `ipc_rpc_entry.cpp` + `IPC_RPC_STATIC_LINK`，`:265-266`）；`test/BUILD.gn`（`ipc_single_test_static` 追加 entry/loader，`:110-111`，宏 `:165`）；`test/unittest/ipc/native/core/BUILD.gn`（两个新目标 `:167-236`，group deps `:253-258`）；新增 `ipc_rpc_static_link_unittest.cpp`、`ipc_rpc_static_link_failure_unittest.cpp`；U-1（`dbinder_databus_invoker.h` / `.cpp`） |
| 安全关键读点 | 静态分支的校验顺序与失败语义、测试 so 的导出面与安装面、能力表 9 槽完整性、U-1 的注册时机与 miss 路径、权限链在 `ops == nullptr` 时的行为、测试侧日志/字符串泄露 |
| 产物证据 | `out/x86_64_virt/communication/ipc/{libipc_single.z.so,libipc_single_test.z.so,libipc_rpc.z.so}`；`out/x86_64_virt/tests/unittest/ipc/ipc/core/IpcRpcStaticLinkFailureUnitTest`；冻结副本 `/tmp/opencode/pre_task5_libipc_single_test.z.so`（602,552 B）、`/tmp/opencode/split_off_libipc_single.z.so`（488,464 B） |
| 构建/运行证据 | `/tmp/opencode/task5_unittest_build3.log`、`/tmp/opencode/task5_static_test_build.log`、`/tmp/opencode/task5_testlogs/` |
| 工具 | `code/prebuilts/clang/ohos/linux-x86_64/llvm/bin/{llvm-nm,llvm-readelf,llvm-strings,llvm-objdump}` |
| 未采用 | `out/x86_64_virt/packages/phone/system/lib64/platformsdk/libipc_single.z.so`（11-12 01:29，早于拆分，已过期，未用作导出面证据；仅用 `packages` 证明其中没有测试 so）；`execution-plan.md` 的结论文字只作交叉对照 |

## 2. 七个维度的结论

| # | 维度 | 结论 | 关键证据 |
|---|---|---|---|
| 1 | 静态分支 `CheckCapabilityTable()` 的顺序与 fail-closed | **通过** | 静态 `:276` 取表 → `:277` 校验 → 失败 `:278-279`（1 条 `<static link>` 日志 + `g_ops=nullptr`）→ 成功 `:282` 发布；校验函数为两形态共享的 `:70-85`，顺序为 `nullptr` → `abiVersion` → `IsOpsComplete()`（9 槽，`:56-62`） |
| 2 | 测试的强符号 provider 能否进入真实镜像 | **通过**（SC-04 为加固建议） | `install_enable = false`（`ipc_single/BUILD.gn:288`）、`visibility`（`:250`）；`grep -rn "ipc_single_test" --include=BUILD.gn` 的依赖方全部落在 `./test/`；`packages` 下无测试 so |
| 3 | `ipc_single_test` 是否意外导出 `IpcRpcGetOps` / `IpcRpcOpsGet` | **已确认导出，属测试面问题**（SC-01 Medium） | 测试 so 导出 839 条，含 `IpcRpcGetOps`(T)、`IpcRpcOpsGet`、`IpcRpcOpsPeek`；`llvm-readelf -V` 无输出（无版本脚本）；3 条 `JUMP_SLOT` 重定位可被抢占。产品静态产物 `ipcrpc` dynsym 计数为 0、重定位为 0 |
| 4 | U-1：`static inline` → 静态成员定义的初始化时序 / TOCTOU / fail-open | **通过** | 声明 `dbinder_databus_invoker.h:132`，唯一定义 `dbinder_databus_invoker.cpp:41-42`；`InvokerFactory::Register` 为 `creators_.insert(...)`（`invoker_factory.cpp:58`，先注册者胜出，重复注册不会覆盖）；`GetRemoteInvoker` 在 DATABUS miss 时先 `IpcRpcOpsGet()` 再重试（`ipc_thread_skeleton.cpp:241-247`），取表失败/注册缺失均返回 `nullptr` |
| 5 | 权限/授权链在两形态下是否等价、加载失败是否覆盖 | **基本通过**（SC-02 覆盖缺口，SC-05 既有清理降级） | 两形态的 `grantPermission` / `removePermission` 指向同一个 `ipc_rpc_entry.cpp:81-89` 实现；`ops == nullptr` 时 4 个调用点均提前返回错误码（`ipc_object_stub.cpp:245,284,787,807` 等），不会走到 `ops->grantPermission`（`:791`）/`ops->removePermission`（`:811`） |
| 6 | 测试侧信息泄露 | **通过** | 两个新用例源码无 token/deviceId/uid/pid/绝对路径字面量；`/tmp/opencode/task5_testlogs/` 仅含 `HiLogAdapter_init: Can't connect to server. Errno: 2` 与用例名；新用例二进制内 `/srv/workspace`、`/home/huawei` 计数 0 |
| 7 | 开关关闭时是否新增攻击面 | **通过** | 关闭态没有新增 transaction code / ioctl / 权限位 / fd 传递；`IpcRpcGetOps` 在产品静态产物中未导出（dynsym 0）、无动态重定位（0），库内为直接调用；与 TASK-4 关闭态结论一致 |

## 3. 问题一：静态形态是否与动态形态安全等价？

**是，且静态形态的可攻击面更小。** TASK-5 没有把「校验」与「取表」解耦，二者仍由同一函数、同一 `once_flag` 串起来。

| 关注点 | 静态形态（`IPC_RPC_STATIC_LINK`） | 动态形态（默认） | 是否等价/更强 |
|---|---|---|---|
| 取表路径 | `IpcRpcGetOps()` 库内直接调用（`:276`） | 绝对路径 `dlopen` → `dlsym` → `dladdr` 校验 → 调用 | 静态更强：无路径解析、无外部装载、无跨 DSO 信任边界 |
| 空表 | `CheckCapabilityTable` 第 1 关（`:71-74`） | 同函数 | 等价 |
| ABI 版本 | `abiVersion != IPC_RPC_OPS_ABI_VERSION`（`:76-79`） | 同函数 | 等价 |
| 9 槽完整性 | `IsOpsComplete()`（`:56-62`），`shutdownListenSocket` 等任一为空即拒绝 | 同函数 | 等价 |
| 失败语义 | 1 条 `IPC_RPC_LOG_LOAD_FAIL("<static link>", ...)` + `g_ops.store(nullptr)`（`:278-280`） | 1 条日志 + `g_ops.store(nullptr)`（`:341-343`） | 等价（fail-closed，均不重试） |
| 并发 | `std::call_once(g_loadOnce, LoadOnce)`（`:353`） | 同 | 等价 |
| 重入 | 静态分支不持锁、不触发外部库静态构造；`RPC_OPS` 为 `const` 聚合、全常量/函数指针初始化（`ipc_rpc_entry.cpp:117-128`），属常量初始化，不存在「表构造未完成即被读取」 | 动态分支注释明确禁止在 rpc 静态初始化期回调 `Get()`（`:288-296`） | 静态无新增重入面 |
| 可替换性 | 产品产物 `IpcRpc` dynsym 0、重定位 0，调用不可被先序 DSO 替换 | `RTLD_LOCAL` + `dlsym(handle)` + `dladdr` 校验 | 静态不可劫持；动态按 TASK-3 已闭环方案 |

**结论**：TASK-5 只把静态形态的**测试证据**补齐，没有改动上述任一判定。静态形态的「空表 / ABI / 完整性」三关与动态形态逐项同源，失败路径同样只缓存 `nullptr` 并由调用方走既有错误码；权限链在表为空时是**拒绝**而不是放行。

## 4. 问题二：测试 provider 是否可能进入产品？

**不能。测试 so 与失败用例二进制都不在产品安装面内，且都没有被任何产品目标依赖。**

| 检查面 | 证据 | 结论 |
|---|---|---|
| 是否安装 | `install_enable = false`（`interfaces/innerkits/ipc_single/BUILD.gn:288`；`git show HEAD:` 中同样为 `false`，属既有设置，非 TASK-5 新增） | 不进入任何镜像 |
| 可见性 | `visibility = ["$SUBSYSTEM_DIR/*"]`（`:250`） | 仅本仓目标可依赖 |
| 实际依赖方 | `grep -rn "ipc_single_test" --include=BUILD.gn .` 的命中全部位于 `./test/`（`unittest/*`、`fuzztest/*`、`moduletest/*`、`auxiliary/*`），`interfaces/`、`ipc/`、根 `BUILD.gn`、`bundle.json` 无产品侧引用 | 无产品依赖 |
| 安装包实盘 | `find out/x86_64_virt/packages -name "*ipc_single_test*"` 无输出 | 未被打包 |
| 测试 so 自身 | `libipc_single_test.z.so` 的 `DT_NEEDED` 为 12 条，`libipc_rpc` 0 条；`libipc_rpc` 字符串 0；`<static link>` 1（说明走静态分支） | 测试 so 是自包含的，不反向拉入 RPC 库 |
| 失败用例二进制 | `llvm-readelf -d IpcRpcStaticLinkFailureUnitTest \| grep NEEDED` 仅 `libhilog.so`、`libc.so`、`libc++.so`；源码自编译 `ipc_rpc_loader.cpp` 并自带 `extern "C" IpcRpcGetOps` | 不链接任何 IPC DSO，provider 只存在于该测试进程 |

补充说明（SC-04，Low，既有加固项）：`ipc_single_test` **没有** `testonly = true`（`:249-291` 内无该属性），而 `ohos_unittest` / `ohos_test` 模板会设置 `testonly = true`（`code/build/test.gni:172,174`）。当前「不安装 + 仅测试依赖 + 不在 `packages`」已足以阻止进入产品；`testonly` 只是 GN 层的额外护栏。由于该缺口在 TASK-5 之前就存在，不计为本次阻塞。

## 5. U-1（委托器定义下沉）的安全复核

U-1 的动机是链接期问题（header 内联定义会让只链核心库的用例引用 `DBinderDatabusInvoker` 构造/析构），但它改变了静态初始化的位置，因此按「时序 / TOCTOU / fail-open」三项单独复核：

1. **注册时机**：`DBinderDatabusDelegator_` 现在是 `dbinder_databus_invoker.cpp` 的静态成员定义（`.h:132` 声明、`.cpp:41-42` 定义）。该 TU 属于 `rpc_source`：拆分态归 `libipc_rpc.z.so`，静态态随 `ipc_single` 一起编入。两种形态下注册都发生在**承载实现的镜像被加载时**，而非「任何包含 header 的 TU 的静态初始化时」。这与拆分前的实际行为一致（拆分前该实现本就在 `libipc_single` 内），只是把「隐式弱定义的多份注册」收成「显式单份注册」。
2. **TOCTOU / 重入**：`InvokerDelegator` 构造函数只做 `InvokerFactory::Get().Register(prot, lambda)`；`Get()` 是函数局部静态（`invoker_factory.cpp:41-44`），不依赖 IPC 加载器，不会在 `call_once` 内回调 `IpcRpcOpsGet()`。拆分态下首次 DATABUS 使用会 `dlopen` `libipc_rpc`，`IPCThreadSkeleton::GetRemoteInvoker` 已在触发装载前把 `usingFlag_` 复位为 `INVOKER_IDLE_MAGIC`，装载返回后重新置位并二次 `newInstance`（`ipc_thread_skeleton.cpp:241-247`），不存在「半更新状态被外部静态构造观察到」的窗口；该机制由 TASK-2 引入，U-1 未改变它。
3. **fail-open**：注册缺失或 `newInstance` 返回 `nullptr` 时，`GetRemoteInvoker` 返回 `nullptr`，调用方按既有错误码失败（`ipc_object_stub.cpp:240-243` 的 `invoker == nullptr` 分支等）。没有「注册失败就退化为无校验 invoker」的路径。`InvokerFactory::Register` 在 `isAvailable_ == false` 或协议已注册时返回 `false`（`invoker_factory.cpp:51-58`），同样不会放行。
4. **静态析构顺序**：`InvokerDelegator::~InvokerDelegator()` 为空（`invoker_factory.h:68-70`），不会在 `InvokerFactory` 析构后反向访问；`InvokerFactory::~InvokerFactory` 仅清表并置 `isAvailable_ = false`。
5. **导出面影响**：测试 so 相对 TASK-5 前的冻结副本，符号差为 `+IpcRpcGetOps`、`- _ZGVN4OHOS21DBinderDatabusInvoker24DBinderDatabusDelegator_E`（后者的内联 guard 变量随 U-1 消失），没有新增 `DBinderDatabusInvoker` 的构造/析构导出；产品静态产物的 `ipcrpc` 计数仍为 0。

**判断**：U-1 把注册从「包含即注册」收紧为「库加载即注册」，方向与 BR-4（核心库不被 RPC 符号污染）一致；没有 fail-open，没有新增可利用的时序窗口。

## 6. 独立核验记录

| # | 检查项 | 命令 / 位置（本方执行） | 实际输出摘要 |
|---|---|---|---|
| 1 | 静态分支取表与失败语义 | `sed -n '272,283p' ipc/native/src/core/dbinder/source/ipc_rpc_loader.cpp` | `#ifdef IPC_RPC_STATIC_LINK` → `void LoadOnce()` → `const IpcRpcOps *ops = IpcRpcGetOps();` → `if (!CheckCapabilityTable(ops, failure))` → `IPC_RPC_LOG_LOAD_FAIL("<static link>", ...)` → `g_ops.store(nullptr, ...)`；成功 `g_ops.store(ops, ...)` |
| 2 | 共享校验函数三关 | `sed -n '70,85p'` 同文件 | `ops == nullptr` → `"IpcRpcGetOps returned null"`；`ops->abiVersion != IPC_RPC_OPS_ABI_VERSION` → ABI mismatch；`!IsOpsComplete(ops)` → null entries；否则 `true` |
| 3 | 9 槽完整性 | `grep -n "IsOpsComplete" -A 8` 同文件 | `:56-62`，逐项检查 `updateClientSession` / `startServerListener` / `shutdownSocket` / `grantPermission` / `removePermission` / `getPidAndUidFromServiceName` / `getLocalNodeDeviceId` / `shutdownListenSocket` / `getClientFd`，共 9 个 |
| 4 | 两形态同一 `once_flag` | `grep -n "call_once"` 同文件 | `:49 std::once_flag g_loadOnce;`、`:353 std::call_once(g_loadOnce, LoadOnce);` |
| 5 | 测试 so 的 ipcrpc 导出 | `llvm-nm -D --defined-only out/x86_64_virt/communication/ipc/libipc_single_test.z.so \| grep -i ipcrpc` | `T IpcRpcGetOps`（0x8a930）、`T _ZN4OHOS12IpcRpcOpsGetEv`（0x422a0）、`T _ZN4OHOS13IpcRpcOpsPeekEv`（0x42450） |
| 6 | 测试 so 导出总数 | 同文件 `llvm-nm -D --defined-only ... \| wc -l` | `839` |
| 7 | 测试 so 是否有版本脚本 | `llvm-readelf -V out/.../libipc_single_test.z.so` | 无输出（无 `.gnu.version_d` 节点、无版本脚本） |
| 8 | 测试 so 的 `IpcRpc` 重定位 | `llvm-readelf -r out/.../libipc_single_test.z.so \| grep IpcRpc` | 3 条 `R_X86_64_JUMP_SLOT`：`IpcRpcOpsPeek`、`IpcRpcOpsGet`、`IpcRpcGetOps`（即测试进程内可被先序 DSO 抢占） |
| 9 | 测试 so 的动态装载文案 | `llvm-strings out/.../libipc_single_test.z.so \| grep -Fc <串>` | `libipc_rpc`=0、`cannot resolve the directory`=0、`resolved entry is not provided`=0、`dlsym IpcRpcGetOps failed`=0、`<static link>`=1 |
| 10 | 测试 so 的动态装载调用点 | `llvm-objdump -d out/.../libipc_single_test.z.so \| grep -cE "callq?[[:space:]]+.*<sym"` | `dlopen`=2、`dlsym`=11、`dladdr`=4；TASK-5 前冻结副本为 `3 / 12 / 4`（loader 动态分支被 `IPC_RPC_STATIC_LINK` 编掉） |
| 11 | 测试 so 的依赖 | `llvm-readelf -d out/.../libipc_single_test.z.so \| grep -c NEEDED`；`... \| grep -c libipc_rpc` | NEEDED=12，`libipc_rpc`=0 |
| 12 | TASK-5 前后测试 so 符号差 | `comm -13 pre_syms.txt cur_syms.txt` / `comm -23 ...` | 新增 `IpcRpcGetOps`；移除 `_ZGVN4OHOS21DBinderDatabusInvoker24DBinderDatabusDelegator_E`（U-1 的内联 guard）；总数均为 839 |
| 13 | 产品静态产物（关闭态） | `llvm-nm -D --defined-only /tmp/opencode/split_off_libipc_single.z.so \| wc -l`；`... \| grep -ic ipcrpc`；`llvm-readelf -r ... \| grep -c IpcRpc`；`llvm-strings ... \| grep -Fc "<static link>"` | 导出 `245`；`ipcrpc`=`0`；重定位=`0`；`<static link>`=`1` |
| 14 | 产品拆分态核心库 | `llvm-nm -D --defined-only out/.../libipc_single.z.so \| wc -l`；`... \| grep -ic ipcrpc` | 导出 `235`；`ipcrpc`=`0` |
| 15 | 测试 so 的安装面 | `sed -n '249,291p' interfaces/innerkits/ipc_single/BUILD.gn`；`git show HEAD:interfaces/innerkits/ipc_single/BUILD.gn \| grep -n install_enable`；`grep -rn "ipc_single_test" --include=BUILD.gn .`；`find out/x86_64_virt/packages -name "*ipc_single_test*"` | `visibility`（`:250`）、`sources = common_source + ipc_rpc_entry.cpp`（`:265`）、`defines = ["IPC_RPC_STATIC_LINK"]`（`:266`）、`install_enable = false`（`:288`，HEAD `:206` 亦为 false）；依赖方全部在 `./test/`；packages 无命中 |
| 16 | 失败用例二进制依赖 | `llvm-readelf -d out/x86_64_virt/tests/unittest/ipc/ipc/core/IpcRpcStaticLinkFailureUnitTest \| grep NEEDED` | 仅 `libhilog.so`、`libc.so`、`libc++.so`（无 `libipc_single` / `libipc_single_test` / `libipc_rpc`） |
| 17 | 失败用例二进制文案与路径 | `llvm-strings <failure binary> \| grep -c ...` | `<static link>`=1、`capability table has null entries`=1、`/srv/workspace` 或 `/home/huawei`=0 |
| 18 | U-1 声明与定义 | `grep -n "DBinderDatabusDelegator_" ipc/native/src/core/dbinder/include/dbinder_databus_invoker.h .../source/dbinder_databus_invoker.cpp` | 声明 `dbinder_databus_invoker.h:132`（`static InvokerDelegator<DBinderDatabusInvoker> DBinderDatabusDelegator_;`）；定义 `dbinder_databus_invoker.cpp:41-42`（`{ IRemoteObject::IF_PROT_DATABUS }`） |
| 19 | 注册语义（先注册者胜出） | `cat ipc/native/src/core/invoker/source/invoker_factory.cpp` | `Register` 返回 `creators_.insert(std::make_pair(protocol, creator)).second`（`:58`）；`Get()` 为函数局部 `static InvokerFactory instance`（`:41-44`），不依赖 IPC |
| 20 | DATABUS miss 的取表重试 | `sed -n '200,255p' ipc/native/src/core/framework/source/ipc_thread_skeleton.cpp` | `:233 newInstance(proto)`；`:241-247` 仅当 `invoker == nullptr && proto == IF_PROT_DATABUS` 时复位 `usingFlag_` → `IpcRpcOpsGet()` → 再次 `newInstance`；仍为空则 `return nullptr`（fail-closed） |
| 21 | `ops == nullptr` 的调用点行为 | `grep -n "IpcRpcOpsGet()" -r ipc/native/src --include=*.cpp`；`sed -n '780,815p' ipc_object_stub.cpp` | `ipc_object_stub.cpp:244/283/786/806` 取表、`:245/284/787/807` 判空；`ipc_process_skeleton.cpp:545/1322`、`ipc_object_proxy.cpp:1073/1135/1175`、`ipc_thread_skeleton.cpp:243`；stub 的 4 处均在 `ops == nullptr` 时返回 `IPC_STUB_INVALID_DATA_ERR` 或 `""`，不会执行 `ops->grantPermission` / `ops->removePermission` |
| 22 | 构建证据 | `tail -3 /tmp/opencode/task5_unittest_build3.log`；`tail -3 /tmp/opencode/task5_static_test_build.log` | 两份日志均以 `=====build  successful=====` 结束（01:42:33 / 01:49:27）；build3 `[5/215] CXX .../ipc_single_test/ipc_rpc_entry.o`、`[159/215] LINK .../IpcRpcStaticLinkUnitTest`；static 日志 `[19/33] CXX .../ipc_single_test_static/ipc_rpc_loader.o`、`[26/33] CXX .../ipc_single_test_static/ipc_rpc_entry.o` |
| 23 | 运行证据 | `tail -5 /tmp/opencode/task5_testlogs/IpcRpcStaticLinkUnitTest.log`；`.../IpcRpcStaticLinkFailureUnitTest.log` | `[  PASSED  ] 4 tests.`；`[  PASSED  ] 1 test.` |
| 24 | 初始化项数量（注册时机旁证） | `llvm-readelf -S <file> \| grep .init_array`（size/8） | `libipc_rpc.z.so`=13、产品静态 `libipc_single`=15、产品拆分 `libipc_single`=7、测试 so=15 |
| 25 | `testonly` 模板 | `grep -n 'template("ohos_unittest")\|testonly = true' code/build/test.gni` | `:172` 模板定义，`:174` `testonly = true`；`ipc_single_test` 目标本身未设置该属性 |

## 7. 发现清单

| ID | 严重度 | 位置 / 证据 | 问题 | 影响 | 建议 | 状态 |
|---|---|---|---|---|---|---|
| SC-01 | **Medium**（非阻塞） | `llvm-nm -D --defined-only out/.../libipc_single_test.z.so \| grep -i ipcrpc` → `IpcRpcGetOps` / `IpcRpcOpsGet` / `IpcRpcOpsPeek` 三条 `T`；`llvm-readelf -V` 无输出；`llvm-readelf -r ... \| grep IpcRpc` → 3 条 `JUMP_SLOT`；对照产品静态产物 245 导出 / `ipcrpc` 0 / 重定位 0 | 测试 so 没有版本脚本，`IpcRpcGetOps` 等 3 个符号被导出且调用走 PLT，可被先序 DSO 抢占；产品静态产物用 `local: *` 把 `IpcRpcGetOps` 收为库内符号并直接绑定 | 新用例只能证明 loader/provider 的**逻辑**，不能证明产品产物的**符号不可见/不可抢占**。若未来 `libipc_single_map` 被误改或以别的方式把 `IpcRpcGetOps` 放回 dynsym，这 5 个用例仍会全绿 | 不改测试 so（它必须能链接 `IpcRpcGetOps`）；由 TASK-6 增加产品产物级断言：对关闭态 `libipc_single.z.so` 断言 `nm -D \| grep -ic ipcrpc == 0` 且 `readelf -r \| grep -c IpcRpc == 0`，并在 `design.md` 风险表登记「测试 so ≠ 产品产物导出面」 | 非阻塞（TASK-6 收口） |
| SC-02 | Low（非阻塞） | `test/unittest/ipc/native/core/ipc_rpc_static_link_failure_unittest.cpp:114-121` 仅断言 `IpcRpcOpsGet() == nullptr`、`IpcRpcOpsPeek() == nullptr`、`providerCalls == 1`；源码侧 `ipc_object_stub.cpp:245/284/787/807` 已 fail-closed | 没有运行期用例直接断言「能力表为空时，创建/删除会话名被拒绝且不会调用 `grantPermission`/`removePermission`」 | 权限负向链路目前只有源码级证据；若将来有人在 null 检查之前插入 `ops->grantPermission`，现有用例不会失败 | 新增一个「空表 + 真实 `IPCObjectStub`」用例（可用 `IPC_RPC_UNIT_TEST` 钩子把表置空），断言 `CreateSessionName` 返回 `""`、`RemoveSessionName` 返回错误码、provider 不被调用；或至少在 `execution-plan.md` 的 AC-2.3 映射里注明「权限负向为源码级复核」 | 非阻塞 |
| SC-03 | Low（非阻塞） | 失败用例源码 `extern "C" const IpcRpcOps *IpcRpcGetOps()` + 本地编译 `ipc_rpc_loader.cpp`；`llvm-readelf -d` → NEEDED 仅 `libhilog/libc/libc++` | 该二进制自带强符号 provider。当前安全的前提是它不链接任何 IPC DSO | 若以后有人给该目标加上 `ipc_single_test`/`libipc_single`，ELF 会优先使用可执行文件内的定义（符号抢占），测试仍然通过，但已不再验证库内 provider，形成「假绿」 | 在 `BUILD.gn` 目标处保留注释并加一条链接期/CI 断言（如检查 NEEDED 中不含 `libipc_*`），或在 `execution-plan.md` 标注该约束 | 非阻塞 |
| SC-04 | Low（非阻塞，既有） | `interfaces/innerkits/ipc_single/BUILD.gn:249-291` 无 `testonly`；`git show HEAD:` 同样无；`code/build/test.gni:174` 的 `ohos_unittest` 才有 `testonly = true` | 测试 so 只靠 `install_enable = false` / `visibility` / 依赖约定阻止进入产品，GN 层没有 `testonly` 护栏 | 防御纵深不足（不是可利用缺陷）：同仓产品目标在语法上仍可依赖它，构建期不会报错 | 后续单独变更给 `ipc_single_test` 加 `testonly = true`（需确认 `test/auxiliary`、`moduletest`、`fuzztest` 均为 testonly，已具备条件） | 非阻塞 |
| SC-05 | Low（非阻塞，既有设计） | `ipc/native/src/core/framework/source/ipc_process_skeleton.cpp:115-118` | 析构路径用 `IpcRpcOpsPeek()`；表为空时跳过 `shutdownListenSocket`，监听 socket 不关闭 | 资源清理降级，不是权限绕过，也不会放行任何操作。静态形态下能力表在加载期即通过校验，该分支实际不可达；仅在动态形态「从未加载/加载失败」时出现 | 维持现状（teardown 期间不允许触发装载是刻意设计）；在 `design.md` 记一句「加载失败时有意的清理降级」即可 | 非阻塞 |

## 8. 未验证项（显式声明）

1. **真实设备/模拟器实跑**：本机无可用 `hdc` 目标，未在真实进程里触发「关闭态首次 RPC」与并发首调；运行证据仅为作者保留的宿主态 `x86_64_virt` 日志。
2. **产品安装包实盘**：`make_packages` 未执行，`packages` 只能证明「没有测试 so」，不能证明最终镜像的完整安装清单。
3. **其它 `target_cpu`**：产物证据仅 `x86_64_virt`；版本脚本、`local: *`、静态分支的结论与架构无关，但未逐 target 复核。
4. **SELinux / 权限标签**：未实测静态单库与拆分双库的标签差异（本次不新增文件路径类别）。
5. **产品静态产物的 U-1 形态**：`/tmp/opencode/split_off_libipc_single.z.so`（01:16）早于 U-1（01:41），其委托器符号仍是内联形态；导出面结论（245 / `ipcrpc` 0）不受影响，但未对「U-1 之后重新构建的关闭态产物」逐字节复核。
6. **`init_array` 条目的符号归属**：只统计了条目数量，未对每条做 DWARF 回溯；注册时机的结论主要依据源码位置（`rpc_source`）与 `InvokerFactory` 的先注册者胜出语义。

## 9. 阻塞判定

- **`blocking_findings = 0`。** 未发现「削弱权限判定」「可注入/可替换能力表」「测试 provider 进入产品镜像」的路径：两形态共用同一校验函数与同一 `once_flag`，失败一律缓存 `nullptr`；测试 so 不安装、不被产品目标依赖、不在 `packages`；失败用例二进制不链接任何 IPC DSO；产品静态产物的 `IpcRpc` dynsym/重定位均为 0。
- **SC-01（Medium）是唯一需要治理层登记的项**：它不改变产品行为，也不构成可利用缺陷，但意味着「新用例全绿」不能替代「产品产物导出面为 0」的二进制断言。建议在 TASK-6 用构建期/产物级检查收口，并在 `design.md` 风险表留一行说明测试 so 与产品产物的导出面差异。
- **SC-02～SC-05 均为 Low**：分别是权限负向的运行期覆盖缺口、失败用例的符号抢占约束、`testonly` 防御纵深、以及既有 teardown 清理降级；均可在后续任务或单独变更中处理，不影响本任务合入。
- 本席位未执行构建；若后续 `ipc_single/BUILD.gn` 的 `install_enable`/`visibility`、`test/BUILD.gn` 的静态源列表、或 `ipc_rpc_loader.cpp` 的静态分支被再次修改，第 6 节第 1～5、9～17、21 行需要重新核验。