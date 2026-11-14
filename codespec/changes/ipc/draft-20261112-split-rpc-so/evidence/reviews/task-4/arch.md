---
task_id: TASK-4
risk: R2
seat: arch
verdict: PASS
blocking_findings: 0
---

# TASK-4 Arch Review

评审对象：TASK-4「新增 `ipc_feature_rpc_split_enabled` 开关与静态回退」（`codespec/changes/ipc/draft-20261112-split-rpc-so/execution-plan.md:448-552`）。

席位边界：只评审 ① 重复造轮子 / ② 性能与资源。本次为只读评审，**除本文件外未修改仓库任何文件**，未执行 `git add/commit/checkout`，未改源码或构建文件，**未执行 `./build.sh`**；构建结论来自作者留存日志原文 + `/tmp/opencode` 下冻结产物的独立测量 + 当前 `out/x86_64_virt` 开启态产物交叉核对（该 out 已被作者重建为开启态，时间 01:13/01:14，故关闭态的 ninja 清单无法复检，改用产物级证据替代，见核验 #6/#7/#8）。

## 结论

**PASS，阻塞发现 0 条（无 Critical / High）。**

作者对 TASK-4 的 8 项声明均可独立复现：开关只存在于构建文件、四个 `ipc_rpc*` 引用点全部被 gate、关闭态确实把 4 个 RPC TU 与 entry TU 编入同一 so（以 `.gnu_debugdata` 符号 + 反汇编证明，而非仅凭 `strings`）、关闭态动态加载段整体被编译掉、关闭态导出面相对基线零丢失、`DT_NEEDED` 与开启态核心库逐条相同且双向单向性成立、体积数字逐项吻合。

2 条 Medium 与 7 条 Low 均为**潜伏风险或口径问题**，不影响本任务判据：(a) 三份手抄 `defines` 清单一旦漂移会造成跨 DSO 的静默 ODR/枚举错位（当前三处一致）；(b) AC-4.1 要求的「关闭态全部用例通过」在 TASK-4 内无任何测试证据（作者已披露，TASK-5 承接，批次 verdict 前必须关闭）。二者均不构成「作者结论不成立」。

## 维度结论

| 维度 | 结论 | 依据摘要 |
|------|------|---------|
| ① 重复造轮子（实现与加载器） | 通过 | (a) 关闭态是把同一批 TU 编入 `ipc_single`（`ipc_single/BUILD.gn:122-130`），没有任何实现被复制第二份；(b) 仓内不存在第二份 loader：`git grep ipc_rpc_loader.cpp --include=BUILD.gn` 只命中 `ipc_single:61` 与两个 test 目标，`ipc_rpc` 的 sources 是 `entry + rpc_source`（`:201`），不含 loader；(c) `common_source = core_source + rpc_source`（`:88`）被测试目标复用而非复制；(d) 路径解析未重复造轮子：既有 `BrokerRegistration::GetObjectSoPath`（`iremote_broker.cpp:144-155`，`iremote_broker.h:118` private）取的是「任意对象所在 so」，且带 `ZLOGD` 与注册表语义，与 loader 取「自身 so 目录」不是同一职责，loader 仅 8 行 `dladdr` 包装（`ipc_rpc_loader.cpp:117-124`），抽象合并会引入反向依赖 |
| ① 重复造轮子（构建清单） | 通过（带 Medium 观察） | 开关单点定义在 `config.gni:25`，全仓 `if` 引用 4 处（`BUILD.gn:30`、`ipc_core/BUILD.gn:76`、`ipc_single/BUILD.gn:122/141/191`、`libdbinder/BUILD.gn:87`），无 per-product 覆盖文件、无第二套开关名；但 `defines` 清单在 3 个 target 内手抄 3 份（`ipc_single/BUILD.gn:144-162`、`:221-237`、`:273-289`），见 A-01 |
| ② 性能（设备内 IPC 热路径） | 通过 | 唯一新增的取表入口 `IPCThreadSkeleton::GetRemoteInvoker` 只在 `invoker == nullptr && proto == IF_PROT_DATABUS` 的首次未命中处调用 `IpcRpcOpsGet()`（`ipc_thread_skeleton.cpp:241-247`），命中缓存后不进入；设备内 Binder 路径无任何新增分支或跨 DSO 调用，AC-5.1/BR-7 方向成立 |
| ② 性能（关闭态 RPC 调用路径） | 通过（带 Low 观察） | 关闭态每次 RPC 入口多一次 `call_once` 快路径 + acquire load + 经能力表的间接调用，不存在与原实现的指令级等价；相对一次跨设备往返（ms 级）可忽略，BR-6 的「运行行为回退」在功能语义上成立，见 A-04 |
| ② 资源（关闭态 ROM/导入面） | 通过（带 Low 观察） | 冻结件实测 488,464 B / `.text` 282,181 / ALLOC 468,467，与作者三处数字完全一致；但相对拆分前基线 492,056 / 289,951 / 467,177 是 **−3,592 / −7,770 / +1,290**：`.text` 降、ALLOC 升、文件降，且 `.plt` 5,184→6,384、`JUMP_SLOT` 323→398，属「导出面变宽换来的 PLT 化」，见 A-03 |
| ② 资源（安装配对） | 通过（带 Low 观察） | `ipc_rpc` 只在 `ipc_rpc_core` 中安装（`ipc_core/BUILD.gn:76-96`，system+updater 的 `platformsdk`），与 `ipc_core` 的 `install_images`/`relative_install_dir` 完全一致；`ipc_single`/`ipc_rpc` 自身 `install_enable = false`；组件级 `ipc_components` 同时依赖两者。缺 rpc 库时按 fail-closed 返回既有错误码而非崩溃（设计已登记残余风险 `design.md:390`）；当前配对关系仅由组件级 `ipc_components` 保证，未做构建期强约束，无新增发现 |

## 独立核验记录

| # | 检查项 | 命令 | 实际输出摘要 |
|---|--------|------|-------------|
| 1 | 开关是否渗入 C++ 源码 | `git grep -n "ipc_feature_rpc_split_enabled" -- '*.cpp' '*.h'`；再以 `-- '*.gn' '*.gni'` 对照 | C++/头文件 **0 命中**；`.gn/.gni` 9 命中：`config.gni:25`（`= true`）+ `BUILD.gn:30` + `ipc_core:76` + `ipc_single:122/141/191` + `libdbinder:87` + 2 行注释 |
| 2 | 所有 `ipc_rpc*` 引用是否都被 gate | 逐处读 `BUILD.gn:30-32`、`ipc_core:73-96`、`ipc_single:191`、`libdbinder:87-93`；并在 `code/` 根（排除 `out`）检索 `ipc_single:ipc_rpc` / `ipc_core:ipc_rpc_core` | 4 处引用全部位于 `if` 内；**仓外引用 0 命中**，故关闭态删除 `ipc_rpc` 目标名不会破坏下游（回答作者 Review Handoff 第 2 问） |
| 3 | 开启态产物与依赖方向（现状 out 交叉核对） | `llvm-size -A`、`llvm-readelf --dyn-syms`、`llvm-readelf -d` 于 `out/x86_64_virt/communication/ipc/*.so` | single 371,584 B / `.text` 206,579 / ALLOC 353,720 / 导出 235；rpc 173,160 B / `.text` 90,717 / ALLOC 161,464 / 导出 11；single 的 12 条 NEEDED 无 rpc；rpc 的 NEEDED **含 `libipc_single`**；`libdbinder` NEEDED 含 rpc（10 条） |
| 4 | 关闭态体积（冻结件） | `llvm-size -A /tmp/opencode/split_off_libipc_single.z.so`；`llvm-readelf -S` 按 `A` 标志求和 | 文件 488,464 B；`.text` 282,181；`Total` 484,363；ALLOC 段合计 **468,467**（`.gnu_debugdata` 15,896 不计）；与作者 Actual Result 的 488,464 / 282,181 / 468,467 **逐项一致** |
| 5 | 关闭态 `DT_NEEDED` 与基线结构 | `llvm-readelf -d` 关闭态 single、关闭态 libdbinder、开启态 single | 关闭态 single 12 条与开启态 single **同序同名**，无 `libipc_rpc`；关闭态 libdbinder 9 条，**无** `libipc_rpc`（开启态 10 条、含）；AC-4.1「依赖结构与拆分前一致」成立 |
| 6 | 关闭态是否真把 entry TU 编入（而非 `#ifdef` 删实现） | `llvm-objcopy --dump-section .gnu_debugdata=… && xz -d`，再 `llvm-nm -C` 该 mini-ELF | `t IpcRpcGetOps`（local，0x6dae0）、`b OHOS::(anonymous namespace)::RPC_OPS`、`IpcRpcOpsGet`/`IpcRpcOpsPeek` 均在；证明 4 个 RPC TU + entry TU 与 loader 处于同一 so，`IPC_RPC_STATIC_LINK` 只切换取表方式（对作者「不违反 D-06」的自查判定：**成立**）。注：mini-ELF 的 `t`/`b` 字母是 `.gnu_debugdata` 重建时的占位类型，仅用于「符号存在」判定，不用于段属性/绑定语义推断 |
| 7 | 关闭态动态加载段是否真被编译掉 | 同上 debugdata 检索 loader 内部符号；`llvm-strings \| grep -c` | `GetOwnLibraryPath`/`ResolveRpcLibraryPath`/`DirectoryOfLibraryPath`/`JoinLibraryPath`/`NamesSameFile`/`VerifyLoadedImage`/`OpenRpcLibrary`/`DescribeLoaderError` 合计 **0**；`libipc_rpc` 字符串 **0**；`capability table` 仅 2（共享校验文案），与 `#ifndef` 边界（`ipc_rpc_loader.cpp:93-263`）吻合 |
| 8 | 关闭态是否仍做 ABI 版本 + 表完整性校验 | `llvm-objdump -d --start-address=0x2da90` 冻结件 | `call IpcRpcGetOps` → `testq/je`（空表）→ `cmpl $1,(%rax)`（ABI=1）→ `8(%rax)`…`72(%rax)` **9 次空指针比较** → 写 `g_ops`(0x73eb8)；全程无 `dlopen`。静态分支（`ipc_rpc_loader.cpp:273-283`）与动态分支共用 `CheckCapabilityTable()`（`:70-85`） |
| 9 | 关闭态导出面是否丢基线符号 / 新增多少 | `comm -23`、`comm -13`（`/tmp/opencode/base_single.txt` 167 条 vs `split_off_exports.txt` 245 条）；`git diff -U0 libipc_single_map` | 丢失 **0**；新增 **78**（74 个唯一名 + 4 组 ctor/dtor 别名）；map 本次 `+('";' 结尾)` 74 行、删除 0 行，与 74 条 `/* used by libipc_rpc */` 名单对应 |
| 10 | 关闭态残留的 `dl*` 导入是否本次引入 | `llvm-readelf -r \| grep -E "dlopen\|dlsym\|dladdr\|dlerror"`；再定位调用点 | 仅 4 条 `JUMP_SLOT`；调用点分别属拆分前既有代码：`ipc_thread_pool.cpp:46-54`（`InitIpcSo` 构造器）、`iremote_broker.cpp:147`、`dbinder_softbus_client.cpp:152-157`。非 loader 残留 |
| 11 | 关闭态内部调用是否被 PLT 化 | `llvm-readelf -r \| grep -c JUMP_SLOT`、`llvm-size` 取 `.plt` | 关闭态 `JUMP_SLOT` 398、`.plt` 6,384；开启态 single 333/5,344 + rpc 207/3,328；基线 323/5,184（基线件已不在本机，取 TASK-3 实测值）。增量约 +75 槽与 +78 条新导出同源 |
| 12 | ① 是否新增第二份 loader / 第二份实现 | `grep -rn "ipc_rpc_loader.cpp\|ipc_rpc_entry.cpp" --include=BUILD.gn .`；读 `ipc_single:49-61`、`:201`、`:88` | loader 仅 `ipc_single:61` + test 两处；`ipc_rpc` 只编 `entry + rpc_source`；`common_source` 为 `core_source + rpc_source` 的复用；无重复实现 |
| 13 | ① 路径解析是否可复用既有 helper | 对照 `iremote_broker.cpp:144-155`、`iremote_broker.h:112-118` 与 `ipc_rpc_loader.cpp:112-152` | 既有 helper 为 private 成员、入参是业务对象指针、附带注册表与日志语义；loader 需「自身 DSO 路径 + 绝对路径校验」，语义不同。判定：**不构成重复造轮子**，无需合并 |
| 14 | 作者构建日志是否与声明一致（不重跑构建） | `grep -n "gn-args\|build success" /tmp/opencode/task4_green5_off.log`、`task4_green4_open.log` | 关闭态日志含 `--gn-args ipc_feature_rpc_split_enabled=false`，`build success`（EXIT 0）；开启态日志 `build success`。**本席位未重跑构建**，`out/` 已回到开启态，故关闭态 ninja 清单不可复检（已由 #6/#7/#8 产物级证据替代） |

## 发现清单

| 编号 | 严重度 | File:Line | 问题 | 证据 | 建议 | 状态 |
|------|--------|-----------|------|------|------|------|
| A-01 | Medium | `interfaces/innerkits/ipc_single/BUILD.gn:144-162`、`:221-237`、`:273-289`；受影响的头文件 `ipc_types.h:141-143`、`message_option.h:29-31`、`ipc_skeleton.h:225-250` | `defines` 清单在 `ipc_single` / `ipc_rpc` / `ipc_single_test` 三个 target 内**手抄三份**。`FREEZE_PROCESS_ENABLED` 直接改变头文件内容：`ipc_types.h` 的枚举成员 `RPC_BASE_INVOKER_NOT_SUPPORT_ERR` 是否出现、`MessageOption::TF_UPDATE_TXN` 是否存在、`IPCSkeleton` 的 freeze 接口是否声明。一旦某个 target 漏改/多加，核心 TU 与 rpc TU 会以不同宏编译同一头文件 → **静默 ODR/枚举错位**，而 `abiVersion` 只校验能力表结构（`ipc_rpc_ops.h:58`/`:78`），**覆盖不到该错位** | 三处现存内容完全一致（`FREEZE`/`CALLING_USER_INFO` 各 3 处；`MEMORY_USAGE_ENABLED`/`CALLING_USER_INFO_ENABLED` 另由 public config `ipc_all_deps_config`（`:25-37`）统一提供，重复但无害）；默认构建下 `ipc_feature_freeze_enabled=false`（`config.gni:33`），属潜伏风险 | 抽一个 `config("ipc_rpc_abi_defines")`（含 FREEZE/TRACE/DFX/HISYSEVENT 全部分支），由三个 target 统一 `configs +=`；`FREEZE` 这类会改头文件内容的宏尤其不能逐 target 抄。另建议在 `ipc_rpc_ops.h` 顶部把「跨 DSO 参与 ODR 的宏集合」列为清单，后续新增宏必须同时进该 config | 未修复（非阻塞） |
| A-02 | Medium | `test/BUILD.gn:89-110`、`interfaces/innerkits/ipc_single/BUILD.gn:256-289`、`test/unittest/ipc/native/core/BUILD.gn:181-183`；AC 来源 `spec.md`（AC-4.1 / BR-6） | AC-4.1 要求「关闭开关后**全部用例通过**」，但 TASK-4 的全部验证项都是构建/产物检查，**没有任何测试执行证据**；且当前 `ipc_single_test`（`test/BUILD.gn:89-110`、`ipc_single/BUILD.gn:256-289`）用 `common_source`（含 loader）却**未定义 `IPC_RPC_STATIC_LINK`、未编入 `ipc_rpc_entry.cpp`**，走的是动态分支且无 companion so，开启态下 13 个既有用例失败（作者 Actual Result 已披露）。即：AC-4.1 的「用例通过」与 BR-6 的「运行行为回退」在本任务内**无证据**，不能据 TASK-4 宣称批次可交付 | 核验 #2/#12：`ipc_single_test` sources=`common_source`（`:265`），defines 无 `IPC_RPC_STATIC_LINK`（`:273-289`）；`ipc_rpc_entry.cpp` 只出现在 `ipc_single` 的两个分支（`:129`、`:201`），不在 test 目标；`test/unittest/.../BUILD.gn:181-183` 也只补 loader + 用例 | 由 TASK-5 按 D-07 补齐 `IPC_RPC_STATIC_LINK` + entry TU + static 形态用例，并**在 TASK-4 回填「关闭态 ipc_unittest 通过」的原始输出**（或明确标注该证据由 TASK-5 提供）。批次 `verdict.md` 在 TASK-5 关闭前不得引用 AC-4.1 为已满足 | 已披露，TASK-5 承接 |
| A-03 | Low | `interfaces/innerkits/ipc_single/BUILD.gn:122-130`、`libipc_single_map:207-281`；`config.gni:25` | 关闭态导出面比拆分前**宽 78 条**（245 vs 167），而这 78 条在关闭态**没有任何跨 DSO 消费者**，纯属为开启态准备的同一张 map 在回退形态的残留。代价可测：`.plt` 5,184→6,384（+1,200 B）、`JUMP_SLOT` 323→398（+75，内部调用被 PLT 化）、ALLOC 467,177→468,467（+1,290 B），文件 492,056→488,464（−3,592 B，`.text` −7,770 抵消了元数据增长）。即 BR-6 的「回退到拆分前状态」在**依赖结构**上成立，在**指令序列/体积**上不成立 | 核验 #4/#9/#11：245=167+78、`comm -23`=0、`.plt`/`JUMP_SLOT`/ALLOC 三组数字 | 作者已在「偏离/修正 3」登记并给出取舍（维护第二份 190 行 map 的漂移风险更高），本席位**认可该取舍**；建议在 TASK-6 的体积对照表里把「关闭态 vs 拆分前」单列一行，说明 +1,290 B 的来源是导出面而非实现，避免下游误读为「回退形态更小」 | 已登记，认可 |
| A-04 | Low | `ipc/native/src/core/dbinder/source/ipc_rpc_loader.cpp:273-283`、`ipc_thread_skeleton.cpp:241-250` | 关闭态并非原实现的指令级回退：首个 RPC 调用需 `call_once` 进入 `LoadOnce()` → `IpcRpcGetOps()` → 版本/完整性校验；其后每次入口为 `call_once` 快路径 + acquire load + 经 `g_ops` 的**间接调用**，而拆分前这些调用是对 `DBinderSoftbusClient`/`DBinderDatabusInvoker` 的直接调用。作者只在状态所有权一节写了「加载动作退化」，未在性能口径中说明该间接层 | 反汇编核验 #8（静态分支确实存在 call + 9 次判空 + 间接调用路径）；`g_ops` 为 `std::atomic`（`:50`），编译期无法去虚化 | 无需改代码（相对 ms 级 RPC 往返可忽略）。建议在 `ipc_rpc_loader.h` 的返回约定里补一句「关闭态同样经能力表间接调用，不是直接调用」，并在 TASK-6 的关闭态性能对照中把它列为已知变量 | 未修复（非阻塞） |
| A-05 | Low | `execution-plan.md`（TASK-4 目标行：「不产生第二个 so、**不 `dlopen`**」）vs `ipc_thread_pool.cpp:46-54`、`dbinder_softbus_client.cpp:152-157`、`iremote_broker.cpp:147` | 「不 dlopen」按字面不成立：关闭态产物仍有 `dlopen`/`dlsym`/`dladdr`/`dlerror` 4 条 `UND` + `JUMP_SLOT`，来自拆分前既有代码（`InitIpcSo` 构造器自加载、SoftBus client 懒加载）。作者的验证项已正确收窄为「无 `libipc_rpc` 字符串/加载路径残留」，结论没错，但目标句仍会被后续读者当作「产物无 dlopen」引用 | 核验 #10：4 条 `JUMP_SLOT` 与三个调用点定位；核验 #7：`libipc_rpc` 字符串 0 | 目标句改为「不产生第二个 so、**loader 不再执行 dlopen**（仓内既有 `InitIpcSo`、`DBinderSoftbusClient` 的 dlopen 不受影响）」，与验证口径一致 | 文档口径 |
| A-06 | Low | `ipc/native/src/core/dbinder/source/ipc_rpc_loader.cpp:56-66` vs `ipc/native/src/core/dbinder/include/ipc_rpc_ops.h:76-97` | `IsOpsComplete()` 用 9 个 `!= nullptr` 表达式**第二次枚举**能力表字段，与 `IpcRpcOps` 的字段集合构成两份真源；`ipc_rpc_entry.cpp:116-127` 的指定初始化又是第三处字段顺序约束。新增字段时若漏改 `IsOpsComplete`，缺项会被当作「表完整」发布 → 调用点空指针调用（正是该校验要防的故障） | 三处字段数均为 9；`ipc_rpc_ops.h:77` 与 `ipc_rpc_entry.cpp:116` 均以注释要求「顺序/集合必须一致」，属人工约束 | 加编译期护栏：`static_assert(sizeof(IpcRpcOps) == sizeof(uint32_t) + 9 * sizeof(void *))` 不足以防漏项，建议改为「表中放一个 `constexpr uint32_t fieldMask`」或让 `IsOpsComplete` 通过 `memcpy` 到 `void*[9]` 数组后统一判空（字段数由 `sizeof` 推导），并在 TASK-5 的单测里对 `RPC_OPS` 逐字段判非空。实现属 TASK-1/3 范围，本任务不返工 | 遗留（非阻塞） |
| A-07 | Low | `execution-plan.md`（体积参考段） | 基线数字 492,056 / 289,951 / 467,177 在树内**不可复现**：基线 `.so` 未留存（本机仅有其他 workspace 的 arm64/wukong 副本），本席位只能核对「关闭态与开启态」实测值，无法重算基线差值，故 −3,592 / −7,770 / +1,290 只能引用作者一次性测量 | 核验 #4 的成功项全部为现测；基线 323 `JUMP_SLOT` 亦只能引用 TASK-3 的实测值 | TASK-6 体积/内存对照必须**先把基线产物冻结进 evidence 目录**（含 `.so` 与 `llvm-size -A` 原始输出），否则收益结论不可复验 | 证据可复现性 |
| A-08 | Low | `ipc/native/src/core/dbinder/include/ipc_rpc_loader.h:24-38` | 头文件注释与实际两形态不符：写「Returns the RPC capability table, **triggering a process-wide load on first use**」、并给出「**~2.7 ns measured on x86_64-virt**」；关闭态不做任何 load（`#ifdef IPC_RPC_STATIC_LINK` 下是同 so 直接调用），且 2.7 ns 无留存测量方法与原始数据 | 核验 #6/#8：关闭态无 dlopen、直接 `call IpcRpcGetOps`；注释未提及 `IPC_RPC_STATIC_LINK` 形态 | 补一段「Build shapes」说明：开启态 = 首次 `dlopen` + 校验；关闭态 = 同 so 直接调用，无 load、无日志路径。延迟数字要么附测量脚本/原始输出，要么删掉具体数值改为定性描述 | 文档准确性 |
| A-09 | Low | `ipc/native/src/core/dbinder/source/ipc_rpc_loader.cpp:296-346` vs `:273-283` | 两种形态的失败处理重复：都是「`IPC_RPC_LOG_LOAD_FAIL(path, failure)` + `g_ops.store(nullptr)`」，约 4 行重复。当前重复量小且两侧日志上下文不同（`<static link>` vs 期望路径），合并收益有限 | 两处 `return` 分支结构一致 | 可保持现状。若后续再加第三种形态，抽 `PublishFailure(const char *context, const std::string &failure)` 收敛，避免「一处忘了 store(nullptr)」导致失败被缓存成空表以外的状态 | 可接受 |

## 阻塞判定

| 项 | 判定 |
|----|------|
| Critical | 无 |
| High | 无 |
| 阻塞发现数 | **0** |
| 结论 | **PASS**（A-01/A-02 为 Medium，A-03~A-09 为 Low，按规则均不计阻塞） |

说明：

1. **A-02 不改变本任务判决但改变批次判决**：TASK-4 的判据是「开关生效 + 关闭态产物结构回退 + 不靠 `#ifdef` 删实现」，三项均已实测成立；AC-4.1 的「用例通过」属测试装配（D-07 / TASK-5），作者已主动披露且未宣称通过，故不构成虚假结论。但批次 `verdict.md` 必须等 TASK-5 关闭后才能引用 AC-4.1，本席位在此显式标注。
2. **对作者两个「需 reviewer 判断」项的明确回答**：(a) 关闭态**不需要**保留 `ipc_rpc` 目标名——全仓及 `code/` 根均无 `ipc_single:ipc_rpc` / `ipc_core:ipc_rpc_core` 的外部引用，删除目标名不会破坏下游；(b) loader 的条件编译**不构成** D-06 禁止的「用代码分支删除实现」——entry TU 与 4 个 RPC TU 在关闭态产物内确实存在（`.gnu_debugdata` 符号 + 反汇编为证），被排除的只有 `dlopen`/路径解析/映像校验这段**加载机制**，功能实现零删除。
3. **① 维度无重复造轮子**：实现、loader、路径解析、能力表四层均为单点实现；唯一「重复」是构建脚本里的 defines 清单（A-01）与 map 的 78 条导出（A-03），前者是维护性风险、后者是已登记取舍。

## 未独立验证项

| 项 | 原因 | 承接方 |
|----|------|--------|
| `./build.sh` 双形态重跑 | 评审约束明确禁止构建；且 `out/x86_64_virt` 已被作者重建为开启态，重跑会覆盖现有产物 | 作者日志 + 冻结件已覆盖结论；如需第三方复核由 TASK-6 执行 |
| 关闭态 `ipc_single.ninja` / 编译命令行 | 同上，ninja 文件已被开启态重建覆盖（`grep ipc_rpc_entry` 0 命中） | 已用 `.gnu_debugdata` 符号 + 反汇编替代（核验 #6/#7/#8） |
| AC-4.1「关闭态全部用例通过」 | 无 `hdc`/模拟器，且 TASK-5 测试装配未完成 | TASK-5（A-02） |
| 真机/镜像内 `platformsdk` 实盘安装 | `make_packages` 受 preheat 遗留 CMakeCache 阻塞（同 TASK-3） | TASK-6 |
| 拆分前基线体积重算 | 基线 `.so` 未留存 | TASK-6（A-07） |
| 非 x86_64 产品（arm/arm64）的开关行为 | 本机仅有 x86_64_virt 产物；arm64 专属 `ldflags` 只在 `ipc_single` target 内（`interfaces/innerkits/ipc_single/BUILD.gn:168-175`），`ipc_rpc` 未继承 | TASK-6 多产品抽检 |