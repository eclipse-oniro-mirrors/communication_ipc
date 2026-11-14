---
task_id: TASK-3
seat: arch
verdict: PASS
blocking_findings: 0
---

# TASK-3 Arch Review

评审对象：TASK-3「RPC 实现迁出为 `libipc_rpc.z.so` + C 能力表入口」（`execution-plan.md:331-441`）。本席位只评审 ① 重复造轮子 / ② 性能与资源，指令为只读评审，除本文件外未修改仓库任何文件；未在设备/模拟器实机运行（本机无 `hdc`、无 `qemu-system-x86_64`），所有结论均来自本机命令原始输出与 `文件:行号`。

## 结论

**PASS，阻塞发现 0 条（无 Critical / High）。**

拆分方案本身在 ① 维度成立：没有另起炉灶重写 RPC 实现，四个 RPC TU 原样迁入 `libipc_rpc`，核心库侧只新增 9 个薄适配函数与一张指定初始化的能力表；`DBINDER_*` 常量、`IRemoteInvoker` 虚函数等既有设施被复用而非复制。② 维度存在**作者未在 Actual Result 中陈述的真实代价**：整机镜像净增 52,656 B（+10.70%）、RPC 用户进程 alloc 段净增 46,133 B（+9.87%），与 `proposal.md:147` 期望的「核心库下降量大于新增库体积」相反；同时核心库永久新增 74 条私有符号导出，无独立版本节点隔离。这两项按 Medium 记录，不影响本任务 PASS，但需要在 TASK-6 的收益结论和后续 ABI 治理中被显式承接。

## 维度结论

| 维度 | 结论 | 依据摘要 |
|------|------|---------|
| ① 重复造轮子 | 通过（无阻塞） | (a) `rpc_source`(`interfaces/innerkits/ipc_single/BUILD.gn:49-54`) 的 4 个 TU 未做任何复制/改写，`ipc_rpc` 直接以同一批源文件编译（`:186`），并通过公开导出复用核心库实现，未重写 `MessageParcel`/`IPCProcessSkeleton` 等基础件；(b) 核心库测得的 RPC 实现类符号为 0（`DatabusSocketListener`=0、`DBinderSoftbusClient`=0、`DBinderDatabusInvoker`=0、`DBinderBaseInvoker`=0），rpc 侧分别为 33/17/79/52，`core_source` 是真实裁剪而非「留一份在核心库」；(c) 能力表 9 个表项全部被核心侧使用（无死表项），入口适配函数均为 1-2 行转发，未引入第二套实现；(d) `DBINDER_PKG_NAME`/`DBINDER_SOCKET_NAME_PREFIX`/`DBINDER_UID_PID_SEPARATOR` 已单点收敛到 `ipc_rpc_ops.h:47-49`，HEAD 中的三份 `static const std::string` 已删除；(e) 加载器与仓内既有 3 处 `dlopen` 语义差异大（`call_once` + ABI 版本校验 + 表完整性校验 + 失败缓存 + 关闭路径分离），单独实现优于抽象合并 |
| ② 性能 | 通过（带 Medium/Low 观察） | (a) 加载期成本一次性且被 `std::call_once` 收敛，10 处 `IpcRpcOpsGet()` 全在 DATABUS/RPC 入口，2 处 `Peek()` 全在关闭路径；(b) 新增的跨 DSO 间接层确有代价：74 条新导出符号 100% 被 rpc 以 `JUMP_SLOT` 引用，其中 29 条在 `libipc_single` 内部也退化为 PLT 调用（基线中 `IPCProcessSkeleton::GetCurrent` 既不在 dynsym、也无 `JUMP_SLOT`，为直接调用；现状两侧均为 `@@1.0` + `JUMP_SLOT`）。绝对量级为每次调用一次 PLT 跳转（ns 级），相对一次 RPC 往返（ms 级）可忽略，故记 Low 而非阻塞 |
| ② 资源（ROM/镜像） | 不通过作者的「净收益」表述（Medium） | 实测基线 492,056 B → 371,536 B + 173,176 B = 544,712 B，净 **+52,656 B（+10.70%）**；`.text` 289,951 → 295,440（**+5,489**）；元数据合计 **+31,068 B**（dynsym +7,008、dynstr +15,364、`.plt` +3,488、`.rela.plt` +5,208）。`execution-plan.md` 的 Actual Result 只报 single 侧 -24.49% / -29.40%，未陈述该净增，且与 `proposal.md:147` 的预期方向相反。见 A2 |
| ② 资源（RAM，进程视角） | 不通过作者的「内存下降」表述（Medium，同 A2） | 按「进程加载的 DSO 集合」折算 ALLOC 段合计（口径见核验记录 #2）：RPC 用户进程 467,177 → 351,633 + 161,677 = 513,310 B（**+46,133 B / +9.87%**）；仅用 IPC 的进程 467,177 → 351,633 B（**-115,544 B / -24.73%**）。即「按进程角色分化」：IPC-only 进程降、RPC 消费进程升，计划未做该分化陈述 |
| ABI/导出面（附带） | 通过（带 Medium/Low 观察） | 核心库 `@@1.0` 导出 167 → 235（+74 新导出，9 条实现随迁出而消失，其余为链接器自动生成的 thunk）；新增导出全部被 rpc 真实引用（74/74），无过度导出；`libdbinder` 侧 8 个未定义符号与 rpc 导出一一对应。风险在「永久 + 无版本节点隔离」，见 A1；map 残留与两侧不对称见 A5 |

## 独立核验记录

| # | 检查项 | 命令 | 输出摘要 |
|---|--------|------|---------|
| 1 | 构建产物体积（整机镜像口径） | `ls -l` 基线 `out/x86_64_virt/packages/phone/system/lib64/platformsdk/libipc_single.z.so` 与现状 `out/x86_64_virt/communication/ipc/libipc_{single,rpc}.z.so` | `173176 libipc_rpc.z.so` / `371536 libipc_single.z.so` / `492056`（基线 single）→ 净 +52,656 B |
| 2 | 段级体积与 ALLOC 段合计 | `llvm-size -A` 三个文件；`llvm-readelf -S -W` 按 `Flg` 含 `A` 的段求和 | 基线 `.text 289951`、`.plt 5184`、`.rela.plt 7752`、`.dynsym 10824`、`.dynstr 21669`；现状 single `.text 204723`、`.plt 5328`、`.rela.plt 7968`、`.dynsym 12360`、`.dynstr 25609`；rpc `.text 90717`、`.plt 3344`、`.rela.plt 4992`、`.dynsym 5472`、`.dynstr 11424`。ALLOC 段合计：基线 `467177`、现状 single `351633`、rpc `161677` |
| 3 | 作者体积声明是否成立 | 同上换算 | single -120,520 B / -24.49%、`.text` -85,228 B / -29.40%，与 Actual Result 一致；但缺净增口径（见 #1） |
| 4 | 导出声面 | `llvm-readelf --dyn-syms -W ... \| grep -c '@@1.0'` | 基线 167 → 现状 single 235、rpc 12（12 = `IpcRpcGetOps` + 8 个 `DBinderSoftbusClient` + `DBinderRemovePermission` + `DBinderDatabusInvoker` C1/C2 两个 ctor） |
| 5 | 新增 74 条导出是否都被真实使用（反过度导出） | map 新增块（`libipc_single_map:202-276`）与 rpc 的 `JUMP_SLOT` 名单排序求交 | `rpc<-74: 74`、`single<-74: 29`；即 74/74 全被 rpc 引用，无冗余导出 |
| 6 | 「核心库已无 RPC 实现」反虚假核验 | `llvm-nm`/`llvm-readelf` 符号计数 | single：`DatabusSocketListener`=0、`DBinderSoftbusClient`=0、`DBinderDatabusInvoker`=0、`DBinderBaseInvoker`=0；rpc：33/17/79/52 |
| 7 | 依赖方向单向性 | `llvm-readelf -d` 三个 .so | rpc NEEDED 含 `libipc_single.z.so`+`libipc_common.z.so`；single NEEDED 无 rpc；`libdbinder.z.so` NEEDED 含 `libipc_rpc.z.so`；`libdbinder` UND ∩ rpc 导出 = 8，且逐个与 `DBinderSoftbusClient` 8 个成员对应 |
| 8 | 拆分是否新增间接层 | `llvm-readelf -r` 统计 `JUMP_SLOT`，并对比基线 | 基线 single 323 条（`.plt` 5,184 = 324 项）→ 现状 single 332 + rpc 208；基线中 `_ZN4OHOS18IPCProcessSkeleton10GetCurrentEv` 在 dynsym 与 `JUMP_SLOT` 中均 0 命中（直接调用），现状两侧均为 `@@1.0` + `JUMP_SLOT` |
| 9 | map 残留与两侧一致性 | `libipc_single_map` 引号内条目 vs single `@@1.0` 导出名求差集，再与 rpc 导出求交 | 差集 26 条，剔除注释词（`AS IS`/`C++`/`License`）后 23 条，其中 **9 条** 恰为 rpc 侧导出（8 个 `DBinderSoftbusClient` + `DBinderDatabusInvoker()`），即 split-on 构建下这 9 条是空声明；`DBinderRemovePermission` 只在 rpc map 出现（`libipc_rpc_map:39`），single map 无 |
| 10 | 安装与版本脚本 | `cat ipc_rpc_core_module_info.json`；`sed -n '32,53p' libipc_rpc_map` | `dest=["system/lib64/platformsdk/libipc_rpc.z.so","updater/lib64/platformsdk/libipc_rpc.z.so"]`、`install_enable: true`、无 `symlink_ext`；rpc map 为 `1.0 { global: IpcRpcGetOps; extern "C++" {...}; local: *; }` |
| 11 | 入口表是否死表 / 是否有第二份实现 | `grep -rn` 对 9 个 ops 指针逐个检索核心库调用点；`sed -n '117,137p' ipc_rpc_entry.cpp` | 9 个指针全部有核心侧使用点；`RPC_OPS`(`:117-128`) 为指定初始化、`IpcRpcGetOps`(`:134-137`) 仅 `return &OHOS::RPC_OPS;`，适配函数均为薄转发 |
| 12 | 加载器与关闭路径分布 | `grep -rn "IpcRpcOpsGet\|IpcRpcOpsPeek"` | `Get()` 10 处（`ipc_object_proxy.cpp:1073,1135,1175`、`ipc_object_stub.cpp:244,283,786,806`、`ipc_process_skeleton.cpp:545,1322`、`ipc_thread_skeleton.cpp:243`）；`Peek()` 2 处（`dbinder_session_object.cpp:44`、`ipc_process_skeleton.cpp:115`），均位于 `~`/`Close*` 路径 |
| 13 | 本次改动是否删除既有导出（跨仓兼容） | 基线 `@@1.0` 与现状 single `@@1.0` 求差集；在 `foundation/`、`base/`、`commonlibrary/` 检索 `DBinderSoftbusClient`/`DBinderDatabusInvoker` | 消失的 9 条正是迁出的 RPC 符号；外部检索仅命中 `relational_store/rdbmock`、`kv_store/kvstoremock` 里的**头文件副本**，无任何外部仓调用 `DBinderSoftbusClient::GetInstance()`——`libdbinder` 是唯一真实消费者且已在本 diff 补 `:ipc_rpc` 依赖（`interfaces/innerkits/libdbinder/BUILD.gn:88-89`） |

## 发现

| 编号 | 严重度 | 维度 | File:Line | 问题 | 证据 | 建议 |
|------|--------|------|-----------|------|------|------|
| A1 | Medium | ② 资源/ABI 面 | `interfaces/innerkits/ipc_single/libipc_single_map:202-276`、`:152-156` | 核心库永久新增 74 条私有实现符号的**公开导出**，且它们落在既有 `1.0` 节点内（无独立版本节点、无 `local:` 分组隔离）。`libipc_single` 的 `innerapi_tags = ["chipsetsdk","platformsdk","sasdk"]`，意味着这 74 条会进入 SDK 的 innerapi 导出集合；后续任何一次「私有方法签名调整」（例如 `IPCProcessSkeleton` 内部函数增删参）都会同时构成对 `libipc_rpc` 的 ABI 破坏，而在导出声面上看不出「这些是给兄弟 DSO 用的私有接口」 | `llvm-readelf --dyn-syms`：基线 `@@1.0` 167 条 → 现状 single 235 条、rpc 12 条；`git diff -U0 libipc_single_map` 新增 74 条引号条目（`grep -c '";$'` = 74）、删除 0 条；74/74 被 rpc `JUMP_SLOT` 引用 | 二选一：(a) 在 `libipc_single_map` 中为这 74 条单开一个命名节点（如 `IPC_RPC_PRIVATE { global: ...; } 1.0;`），使「兄弟 DSO 私有接口」与公开 API 在 `objdump/readelf` 层面可区分，并可整体替换；(b) 若短期不改 map，至少在 `ipc_rpc_ops.h` 顶部与 map 注释中把它登记为「跨 DSO 私有 ABI，签名变更需同步重建 libipc_rpc」，并在 TASK-6 的验收项中加入「`libipc_rpc` 与 `libipc_single` 必须同版本发布」 |
| A2 | Medium | ② 资源（ROM/RAM） | `codespec/changes/ipc/draft-20261112-split-rpc-so/execution-plan.md:331-441`（Actual Result）、`proposal.md:147` | Actual Result 只报核心库下降（-24.49% / `.text` -29.40%），**未报整机镜像与 RPC 进程的净增**。实测整机净 **+52,656 B（+10.70%）**、RPC 用户进程 alloc 段净 **+46,133 B（+9.87%）**、`.text` 净 **+5,489 B**；这与 `proposal.md:147` 期望的「核心库下降量大于新增库体积」相反。收益只在「仅用 IPC 的进程」上成立（-115,544 B / -24.73%），而代价落在「所有加载 `libdbinder` 的 RPC 进程」上——`libdbinder` 新增 `DT_NEEDED` 使 rpc 库被**静态依赖**而非纯懒加载（核验 #7） | 核验 #1/#2：`173176+371536-492056 = 52656`（+10.70%）；alloc 段合计 467,177 → 513,310；`.dynsym` +7,008、`.dynstr` +15,364、`.plt` +3,488、`.rela.plt` +5,208，元数据合计 +31,068 B | 在 Actual Result / TASK-6 结论中补齐「按进程角色分化」的三行口径：① 整机镜像净增；② IPC-only 进程净降；③ RPC 进程净增（含 `libdbinder` 强依赖导致的常驻）。若净增与预期相反且不可接受，再评估「rpc 库只保留入口 + 按需 `dlsym` 细分」或「合并 `libipc_common` 以减少重复 weak 符号（当前两 DSO 重复 weak/模板符号 81 个、各约 11 KB）」。另可将 `proposal.md:147` 的成功判据修正为「IPC-only 进程收益」，避免下游按错误前提验收 |
| A3 | Low | ② 性能（间接层） | `libipc_single_map:202-276`、`libipc_rpc_map:32-53` | 「导出即可抢占（preemptible）」带来的副作用未见作者说明：74 条中 29 条在 `libipc_single` 内部也变成了 PLT 调用（基线里这些调用是同 DSO 直接调用）。绝对代价小，且只影响 RPC/DBinder 路径（设备内 Binder 热路径不触及这些符号），但属于「同一份代码在不同构建形态下热路径指令序列不同」，调试/性能归因时容易被误判为回归 | 核验 #8：基线 `IPCProcessSkeleton10GetCurrentEv` 在 dynsym/`JUMP_SLOT` 均 0 命中，现状 single 与 rpc 均为 `@@1.0` + `JUMP_SLOT`；`JUMP_SLOT` 323 → 332(single) + 208(rpc)；`.plt` 5,184 → 5,328 + 3,344 | 在 TASK-6 性能对照中把「PLT 化」列为已知变量（预期落在测量噪声内）；无需改代码。若后续担心热路径，可对 `IPCProcessSkeleton::GetCurrent` 等极热函数加 `-fno-semantic-interposition`（仅在本 DSO 内部），但这会与「导出给 rpc 用」冲突，**不建议**在本变更内做 |
| A4 | Low | ② 性能（构建形态一致性） | `interfaces/innerkits/ipc_single/BUILD.gn:157-163` vs `:174-238` | `ipc_single` 在 arm64 上有专属链接选项 `-Wl,--emit-relocs`、`-Wl,--no-relax`、`-mno-fix-cortex-a53-843419`，`ipc_rpc` 的 target 未继承。这意味着两个库在 arm64 上的重定位保留与 relaxation 行为不一致；`--emit-relocs` 缺失会影响基于 reloc 的插桩/热补丁/覆盖统计工具对 rpc 库的可用性 | `sed -n '157,163p'` 与 `sed -n '174,238p'`：前者在 `ipc_single` target 内且带 `target_cpu == "arm64"` 条件，后者（`ipc_rpc`）全文仅 `ldflags = [ "-Wl,--no-undefined" ]`（`:227`）；本机为 x86_64_virt，**无法实证** arm64 行为差异 | 若 rpc 库也需要被同一套 reloc 工具链处理，把该 `if (is_ohos && is_clang && target_cpu == "arm64" ...)` 块抽成 `config` 并对两个 target 同时 `configs +=`；若明确不需要，在 `ipc_rpc` target 注释里写明「不参与 reloc 工具链」的理由，避免后续被当作遗漏 |
| A5 | Low | ① 重复造轮子（声明同步） | `interfaces/innerkits/ipc_single/libipc_single_map:19-26`、`:157`；`libipc_rpc_map:39` | 两处 map 手工维护同一批符号：(a) single map 保留的 8 个 `DBinderSoftbusClient` 条目 + `DBinderDatabusInvoker()` 在 split-on 构建下已无定义（实现已迁到 rpc），属空声明，靠 lld「未匹配名静默忽略」才不报错；(b) 两侧不对称：rpc 导出 `DBinderRemovePermission`（`libipc_rpc_map:39`，仅 fuzz/UT 使用），single map 无该条；(c) 无任何漂移守护，只能靠两种构建形态的 `-Wl,--no-undefined` 间接兜底 | 核验 #9：差集 23 条中有 9 条恰为 rpc 导出（`count=9`，逐条列出含 `DBinderSoftbusClient::GetInstance()` 等）；`libipc_rpc_map` 注释自述「Keep this list in sync with the matching block in libipc_single_map」，但 split-off 形态在本树内尚无对应 BUILD.gn 开关（`ipc_single` 恒为 `sources = core_source`，`interfaces/innerkits/ipc_single/BUILD.gn:122`） | 短期：在 single map 的那 9 条上加注释「split-on 时由 libipc_rpc 提供；split-off（TASK-5 静态直连）时由本库提供」，并把 `DBinderRemovePermission` 补进 single map 的同一块，消除不对称。中期：TASK-5 落地 `IPC_RPC_STATIC_LINK` 时，用一个脚本/单测断言「两 map 的 DBinder 块集合相同」 |
| A6 | Low | ① 文档准确性 | `ipc/native/src/core/dbinder/source/ipc_rpc_entry.cpp:24-26` | 注释称能力表是「function-local static」（函数内静态，首次调用时初始化），实际 `RPC_OPS` 是 namespace 作用域 `const`（`ipc_rpc_entry.cpp:117`），走常量初始化、在 DSO 加载期即就绪。语义上更安全（避免 `dlopen` 期间的重入初始化），但注释描述的实现机制与代码不符，会误导后续维护者「不敢在加载期引用」 | `sed -n '20,30p' ipc_rpc_entry.cpp` 显示注释原文；`grep -n "RPC_OPS"` 显示 `:117 const IpcRpcOps RPC_OPS = {`，无函数包裹；`:134-137` 仅 `return &OHOS::RPC_OPS;` | 注释改为「namespace-scope `const` with constant initialization；不依赖首次调用，也不在 `dlopen` 期间执行任何动态初始化」，与代码一致 |
| A7 | Low | ① 命名可读性 | `interfaces/innerkits/ipc_single/BUILD.gn:88`、`:249` | `common_source = core_source + rpc_source` 的命名（"common"）与实际语义（「测试用的全量集合」）不符，且在 split 语境下 "common" 极易与 `ipc_common`（另一个真实库）混淆。当前仅 `ipc_single_test`（`:249`）使用，含义靠注释维系 | `grep -rn "common_source" --include=BUILD.gn .` 仅 `:88` 定义与 `:249` 使用；`:86-87` 注释「Kept whole for the test variant」 | 若 TASK-5 还要再加使用点，改名为 `all_source` 或 `test_source`；否则保持现状亦可（纯可读性） |
| A8 | Low | ① 文档一致性（跨 Task） | `codespec/changes/ipc/draft-20261112-split-rpc-so/design.md:134-145` vs `ipc/native/src/core/dbinder/include/ipc_rpc_ops.h:61-65` | 设计中能力表的签名与实现不一致：设计写 `Get()`/`Peek()`（`design.md:144-145`），实现为 `IpcRpcOpsGet()`/`IpcRpcOpsPeek()`；设计写 `uid_t`（`design.md:134`），实现为 `int32_t`（`ipc_rpc_ops.h:61`）；`getPidAndUidFromServiceName` 设计为指针出参（`design.md:136`），实现为引用出参（`ipc_rpc_ops.h:65`）。属 TASK-1 文档漂移，本任务未引入 | `grep -n` 两侧声明逐项对照（见 File:Line） | 由 TASK-1 责任人在归档前同步设计文本；本任务不阻塞 |

## 阻塞判定

| 项 | 判定 |
|----|------|
| Critical | 无 |
| High | 无 |
| 阻塞发现数 | **0** |
| 结论 | **PASS**（A1/A2 为 Medium，A3-A8 为 Low，按规则均不计阻塞） |

说明：A2 虽指出作者的 ROM/RAM 陈述不完整且方向与预期相反，但（1）TASK-3 的完成判据是「四个 RPC TU 编入独立库、入口唯一、依赖单向」，这些均已实测成立；（2）体积/内存净增属**方案固有代价**而非实现缺陷，且 `proposal.md:147` 本身就把该维度标为 `review-required` 待度量；（3）纠正口径属文档与 TASK-6 测量职责，不构成代码返工。故按 Medium 记录，不影响 PASS。

未在本机验证的项（不计为缺陷，供人工承接）：`packages/` 实盘安装（`make_packages` 被 preheat 遗留 CMakeCache 路径打断，与作者记录的环境阻塞一致）；arm64 上的链接选项/reloc 行为（A4）；真实设备上的 RPM/时延对照（TASK-6）。

---

## 复验（第 2 轮）

本轮复审对象：作者对上轮 A1/A2/M-1/M-2/A5/B-1/S-01 的修复回填（`execution-plan.md:740-751`）。方法：**重新执行**体积、导出、依赖、构建日志与单测命令，仅把原始输出作为证据；仍为只读评审，除本文件外未修改仓库任何文件。本机无 `hdc`、无 `qemu-system-x86_64`，未做实机/模拟器运行，亦不作为缺陷。

### 一、上轮问题闭环判定

| 上轮项 | 首轮严重度 | 本轮判定 | 独立证据（本轮命令原始输出 / 文件:行号） |
|--------|-----------|---------|------------------------------------------|
| A2 体积口径 | Medium | **已闭环** | `ls -l`：基线 492,056 / single 371,584 / rpc 173,160 → 净 +52,688 B（+10.71%），与 `execution-plan.md:444` 完全一致；`llvm-readelf -S -W` 按 `Flg` 含 `A` 求和：467,177 / 353,098 / 161,464 → ALLOC 净 +47,385（+10.14%），与 `:416`/`:444` 一致。`proposal.md:102` 已写「整机镜像总量不作为判据」，`:148` 已写「收益对象是不加载 RPC 的进程」 |
| M-1 安装 tag | Medium | **已闭环** | `ipc_rpc_core_module_info.json` 实测 `innerapi_tags: ["platformsdk_indirect"]`、`dest: system/lib64/platformsdk/libipc_rpc.z.so` |
| M-2 / A5 导出面 | Medium | **已闭环**（不对称项） | `llvm-nm -D --defined-only libipc_rpc.z.so` = **11** 条；`DBinderRemovePermission` 在 rpc 导出=0、`libdbinder` 未定义引用=0；`libipc_single_map:19-21` 已为 8 条 `DBinderSoftbusClient` 加「Split on / Split off」注释 |
| B-1 fuzz 链接 | High | **已闭环** | `test/BUILD.gn` 静态库补 `ipc_rpc_loader.cpp`；两个 fuzz 改依赖 `test:ipc_single_test_static`；三份日志（`task3_fix_round2.log`、`task3_fix_round2_fuzz.log`、`task3_fuzz_regress.log`）均 `x86_64_virt build success`、`grep -c "error:"` = 0/0/0；`check_rpc_symbols_resolved.sh` → `RESULT: PASS (211 undefined, all covered by NEEDED closure)` |
| A1 私有导出隔离 | Medium | **未闭环（维持 Medium）** | 见下节 |

### 二、A1 未闭环的具体证据

作者在「问题闭环」表中把 **A1 与 M-1 合并**为一行，复验依据只引 `ipc_rpc_core_module_info.json` 的 `innerapi_tags`（`execution-plan.md:747`）。该证据只能证明 **rpc 库自身**不是直接对外接口，不能证明核心库那 74 条私有实现符号已被隔离，属**证据与结论不匹配**。逐条实测：

- `llvm-readelf --dyn-syms -W libipc_single.z.so | grep -c '@@1.0'` → **235**（基线 167，净 +68；与首轮同值，未变）。
- 74 条仍全部位于**同一个** `1.0 { global: ... local: *; }` 节点内：`libipc_single_map:15`（`1.0 {`）、`:205-282`（「used by libipc_rpc」块）、`:283`（`};`），**无**新的命名节点、**无** `local:` 分组隔离。
- `git diff --stat -- interfaces/innerkits/ipc_single/libipc_single_map` → `1 file changed, 79 insertions(+)`，即 74 条符号 + 5 行注释/空行，**版本节点结构零变更**。
- `grep -n "跨 DSO|私有 ABI|同版本" ipc_rpc_ops.h libipc_single_map libipc_rpc_map` → **无输出**；`ipc_rpc_ops.h:30-33` 仅有 `IPC_RPC_OPS_ABI_VERSION`（管的是能力表布局），未登记「74 条为兄弟 DSO 私有接口、两侧需同版本发布」。
- `ipc_single_module_info.json` 的 `innerapi_tags` 仍为 `["chipsetsdk","platformsdk","sasdk"]`，74 条继续进入 SDK innerapi 导出集合。

**结论：** A1 的两个可选动作（(a) 单开版本节点；(b) 在 `ipc_rpc_ops.h`/map 中登记跨 DSO 私有 ABI + 同版本发布约束）**一项也未落实**，仅新增了一行「used by libipc_rpc」说明性注释。维持 Medium，不计阻塞（ABI 治理属后续版本约束，不影响本任务功能闭环），但 `execution-plan.md:747` 的「已修复」状态应改回「部分处理 / 待 TASK-6 承接」。

### 三、S-01 架构复核（own-dir + `dlinfo` 校验）

本席位直接读 musl 源码核对语义，不采信实现注释：

| 事实 | 证据 |
|------|------|
| `dladdr` 的 `dli_fname` 来自 DSO 加载时记录的路径串 | `ldso/linux/dynlink.c:890-898` `path_open` 以 `"%.*s/%s"` 拼绝对路径；`:3123` `pathname = buf`；`:3201-3207` `p->name = p->buf; strcpy(p->name, pathname)` |
| 只有最终以相对路径打开时才 realpath | `ldso/linux/dynlink.c:1195-1200`（本方案拼出的是绝对路径，故 `dli_fname` 保持「打开时字符串」语义） |
| `dlinfo` 只支持 `RTLD_DI_LINKMAP`，且直接把 `struct dso *` 当 `struct link_map *` 返回 | `src/ldso/dlinfo.c:8-13` |
| `l_name` 与 `struct dso::name` 偏移一致 | `include/link.h` 的 `struct link_map`（`l_addr`,`l_name`,...）对应 `ldso/dynlink.c:61-66`（`base` 后紧跟 `name`） |

**判定：** own-dir 方案方向正确——`dli_fname` 是「打开核心库时使用的路径」而非 realpath，取 dirname 后拼 `libipc_rpc.z.so` 能定位同目录伴随库；`dlinfo` 全等校验在当前 musl（同仓同镜像）下可用，但**依赖 musl 私有结构布局**（非 POSIX/glibc 语义），且 `l_name` 可能是相对路径或 ADLT 形式（见 A9）。`ResolveRpcLibraryPath()` 返回空串即 `LoadOnce` 不进入 `dlopen`（`ipc_rpc_loader.cpp:120-129`、`:190-240`），符合 `design.md:245` 的 fail-closed 决议，**未引入裸名回退**。

本轮新识别 4 条与该方案相关的残余风险（R1/R2/R3/R4），均不构成本任务功能判据的阻塞。

### 四、本轮实测原始输出

```text
$ ls -l .../packages/phone/system/lib64/platformsdk/libipc_single.z.so \
        out/x86_64_virt/communication/ipc/libipc_{single,rpc}.z.so
492056  .../packages/.../platformsdk/libipc_single.z.so   (基线)
371584  out/x86_64_virt/communication/ipc/libipc_single.z.so
173160  out/x86_64_virt/communication/ipc/libipc_rpc.z.so

$ llvm-size -A <each>            # .text / Total
289951 / 483201   baseline
205955 / 365410   single
 90717 / 167732   rpc

$ llvm-readelf -S -W <each> | ALLOC 段求和（Flg 含 A）
467177  baseline
353098  single
161464  rpc

$ llvm-readelf --dyn-syms -W libipc_single.z.so | grep -c '@@1.0'   → 235
$ llvm-nm -D --defined-only libipc_rpc.z.so | wc -l                 → 11
$ llvm-readelf -d libipc_single.z.so | grep -c libipc_rpc           → 0
$ llvm-readelf -d libdbinder.z.so | grep ipc_
  NEEDED  libipc_rpc.z.so
  NEEDED  libipc_single.z.so
$ llvm-nm -D --undefined-only libdbinder.z.so | grep -c DBinderRemovePermission → 0
$ grep -c '";$'（libipc_single_map 的 used-by-rpc 块）               → 74

$ tail -5 task3_fix_round2.log / task3_fix_round2_fuzz.log / task3_fuzz_regress.log
  x86_64_virt build success（三份均是）；grep -c "error:" → 0 / 0 / 0

$ ld-musl-x86_64.so.1 --library-path lib.unstripped/clang:common/common/libc \
      tests/unittest/ipc/ipc/core/IpcRpcLoaderUnitTest
  [==========] 12 tests from 1 test suite ran. (20 ms total)
  [  PASSED  ] 12 tests.
```

说明：`llvm-size` 的 `Total` 与 `ls -l` 的 stripped 体积不同（前者按节求和，后者含 ELF 头/节表/对齐），故净增口径统一用 `ls -l`：`371,584 + 173,160 - 492,056 = +52,688 B（+10.71%）`。单测需用 musl loader 显式执行（产物 interpreter 为 `/lib/ld-musl-x86_64.so.1`，宿主不存在），直接执行报 `No such file or directory`。

### 五、残余发现（第 2 轮）

| 编号 | 严重度 | 维度 | File:Line | 问题 | 证据 | 建议 |
|------|--------|------|-----------|------|------|------|
| A1 | Medium | ② 资源/ABI 面 | `libipc_single_map:15`、`:205-282`、`:283`；`ipc_rpc_ops.h:30-33` | 74 条私有实现仍作为公开 `@@1.0` 导出，无独立版本节点或 `local:` 分组；无「跨 DSO 私有 ABI / 同版本发布」登记。`execution-plan.md:747` 以 M-1 的 `innerapi_tags` 证据声称已修复，属证据错配 | 本轮实测 `@@1.0` 235（基线 167）、`git diff --stat` 79 insertions（74 符号 + 5 注释）、`grep 跨DSO/私有ABI/同版本` 无命中 | 落实上轮 (a) 或 (b)；把 `:747` 状态改为「部分处理」，由 TASK-6 承接同版本发布校验 |
| R1 | Medium | ② 资源/部署形态 | `ipc_single_module_info.json`（`dest=system/lib64/chipset-sdk/...`、`softlink_create_path=platformsdk`）；`ipc_rpc_core_module_info.json`（仅 `platformsdk`）；`third_party/musl/scripts/paths.json:6-8` | 安装元数据与搜索顺序存在耦合：single 声明装到 `chipset-sdk`（搜索顺序早于 `platformsdk`），rpc 只装 `platformsdk`。当前 `ipc_single`/`ipc_rpc` 均 `install_enable=false`（`BUILD.gn:169`/`:235`），实盘仅由 `ipc_core`/`ipc_rpc_core` 预置到 `platformsdk`，故现状可用；一旦某产品直接安装 single 的 `chipset-sdk` dest，核心库 own-dir 变为 `chipset-sdk`，rpc 路径不存在 → **该目录下 RPC 恒不可用**（fail-closed） | `find packages -name 'libipc_single.z.so'` 仅 2 个 `platformsdk` 副本；`paths.json`: `/system/lib64/chipset-sdk`（第 8 行）先于 `/system/lib64/platformsdk`（第 9 行） | 让 rpc 与 single 的 **实际**安装目录由同一变量派生（或都装 `platformsdk`），并在 TASK-6 加「两库 realpath 目录相同」的断言；`ipc_single` 的 `dest` 元数据与 `install_enable=false` 的口径需一致化 |
| R2 | Medium | ② 性能/加载可靠性 | `ipc_rpc_loader.cpp:137-149`；`third_party/musl/ldso/linux/dynlink.c:3126-3156` | `libdbinder.z.so` 已 `DT_NEEDED` `libipc_rpc`，rpc 常在核心库 loader 之前被动态链接器预加载。此时 `dlopen(绝对路径)` 命中 `find_library_by_fstat` 分支，musl **直接返回已映射 dso 且不回写 `p->name`**；若首次加载路径串与 own-dir 拼出的路径不全等（不同目录、symlink、相对路径、其他 namespace），`l_name` 全等校验永久失败 → 即使正确 rpc 已在内存中，RPC 仍恒不可用 | `llvm-readelf -d libdbinder.z.so` 实测两条 NEEDED；`dynlink.c:3139-3156` 分支仅在 `!p->shortname` 时设置 `shortname`，不修改 `p->name` | 校验改为「realpath/`st_dev`+`st_ino` 等价」而非字符串全等，或对 `find_library_by_fstat` 命中的情况允许 `l_name` 与 expected 的 realpath 比较；TASK-6 需在「rpc 被 `libdbinder` 预加载」的真实进程里验证首次 RPC |
| R4 | Medium | ② 验证覆盖（性能/资源结论的可信度） | `test/unittest/ipc/native/core/ipc_rpc_loader_unittest.cpp:167-181`、`:104-107` | 12 个用例的 `SetUp` **总是**注入 4 个 hook（含 `verifyHook`/`pathHook`），生产分支 `dladdr`+`dlinfo`+`l_name` 全等**零覆盖**；`TestVerifyLibraryPath` 忽略 `handle`/`path` 入参直接返回全局布尔，无法发现偏移/语义错误。故「12/12 通过」不能作为 S-01 生产路径已验的证据 | `:179` `IpcRpcTestHooks hooks = { TestOpenLibrary, TestResolveSymbol, TestResolveLibraryPath, TestVerifyLibraryPath }`；`:104-107` 空实现入参 | 增加一个不装 hook 的用例：`dlopen` 本进程已加载的测试 so，断言 `IsLoadedFromExpectedPath` 对同一文件的不同路径表述的行为，并覆盖 `dlinfo` 失败分支；TASK-6 用真机进程验证 `l_name` |
| R3 | Low | ② 资源/安全边界 | `ipc_rpc_loader.cpp:151-177` | 路径校验发生在 `dlopen` **之后**：错误库的构造函数/静态注册已执行，且 `RTLD_NODELETE` 使其无法卸载。校验失败仅停用能力表，错误映像仍常驻并可能留下全局注册副作用 | `:164` 先 `dlopen`，`:172` 才校验；`:48` `RTLD_NOW | RTLD_NODELETE` | 安全席位已覆盖信任边界；资源侧建议在注释中写明「校验失败不回收映像」，并在 TASK-6 观测失败进程的 maps 是否残留 rpc |
| A9 | Low | ② 性能/路径解析 | `ipc_rpc_loader.cpp:85-100` | `dli_fname` 为相对路径（如 `./libipc_single.z.so`）或 ADLT 形式 `"<adlt名>:<path>"` 时，`rfind('/')` 得到的目录不可用，`dlopen` 失败 → fail-closed（不崩溃，但 RPC 不可用） | `:92-97` 仅按 `/` 切分，无 `realpath`/ADLT 前缀处理 | 在返回前对 `dli_fname` 做 `realpath`（绝对路径时通常等价），并对 `:` 前缀做显式拒绝或剥离；低优先级 |
| B9 | Low | ② 文档一致性 | `proposal.md:148` | ROM 行仍写 `+52,656 B（+10.70%）`，与本轮实测 `+52,688 B（+10.71%）` 差 32 B（早期构建残留数字） | `ls -l` 三份产物实测；`execution-plan.md:416/:444` 已是 52,688/10.71% | 同步为 52,688 / +10.71%，避免下游引用两个版本 |

上轮 A3（PLT 化）、A4（arm64 链接选项）、A6（注释与实现不符）、A7（`common_source` 命名）、A8（设计签名漂移）本轮**未再变化**，维持 Low，不重复展开。

### 六、阻塞判定（第 2 轮）

| 项 | 判定 |
|----|------|
| Critical | 无 |
| High | 无 |
| Medium | A1（未闭环）、R1、R2、R4 |
| Low | R3、A9、B9（A3–A8 维持） |
| 阻塞发现数 | **0** |
| 结论 | **PASS（维持）** |

理由：本轮所有可执行项（体积、导出、依赖、构建日志、单测）均**独立复跑通过**，A2/M-1/M-2/B-1 确已闭环；S-01 的 own-dir + `dlinfo` 方向经 musl 源码核对成立，未引入裸名回退或新的阻塞缺陷。A1 仍为「私有 ABI 未隔离」的治理缺口，R1/R2/R4 为**部署形态与验证覆盖**的残余风险，均不改变 TASK-3「四个 RPC TU 编入独立库、入口唯一、依赖单向、fail-closed」的功能结论，故不判 FAIL。

**TASK-4/6 必须承接（不计入本轮阻塞）：** ① R1——断言 `libipc_single` 与 `libipc_rpc` 的实际安装目录相同（或统一到 `platformsdk`）；② R2——在 `libdbinder` 已预加载 rpc 的进程里验证 `l_name` 全等校验仍通过（否则改为 inode/realpath 等价）；③ R4——补充不注入 hook 的真实 `dladdr`/`dlinfo` 用例；④ A1——同版本发布校验或版本节点隔离。

---

## 复验（第 3 轮）

本轮复审对象：作者对 R2/R5/R4/A1/R1 的修复回填（`ipc_rpc_loader.cpp`、`ipc_rpc_ops.h`、`ipc_rpc_loader_unittest.cpp`、`execution-plan.md`）。方法：**在固定快照上重新执行**体积、导出、符号解析、单测与探针命令，只采信本机命令原始输出与 `文件:行号`，不采信 `execution-plan.md` 中的作者总结；musl 语义直接读源码核对。仍为只读评审，除本文件外未修改仓库任何文件、未提交 git。本机无 `hdc`、无 `qemu-system-x86_64`，未做实机/模拟器运行，亦不作为缺陷。

### 一、结论

**PASS（维持），阻塞发现 0 条（无 Critical / High / Medium）。**

上轮 5 个待复验项**全部闭环**：R5 的「musl 随机句柄使 `dlinfo(RTLD_DI_LINKMAP)` 恒失败」经源码 + 自编译探针双重证实，改用 `dladdr(入口符号)` 方向正确；R2 的「预加载后 `l_name` 拼写不一致导致永久拒绝」已由 `stat()` 的 dev/ino 兜底解决，并经四种路径拼写实测同 inode；R4 的无 hook 生产路径已由 3 个新用例覆盖且 15/15 实跑通过；A1 已在 `ipc_rpc_ops.h:33-49` 登记为「Registered private ABI」；R1 在安装元数据层已完全一致（两库同 `dest`、同目录），实盘安装仍受环境阻塞。

本轮新增 4 条 **Low** 观察（R6–R9），均属文档/证据口径，不影响功能结论：R6 私有 ABI 段落的措辞与 `ipc_core` 实测 tags 不符；R7 R4 的负例只覆盖「文件不存在」而非「同名且真实存在的不同文件」；R8 `execution-plan.md` 中 5 处体积/符号数字仍绑定 00:33 的早期构建，且「ALLOC」实为 `Total − .gnu_debugdata`；R9 本轮存在「源码 15 例 / 二进制 14 例」的短暂错位窗口，属证据时效流程风险。

体积结论**未变但数字更新**：整机三产物净 **+52,696 B（+10.71%）**、`.text` 净 **+7,345（+2.53%）**、LOAD `MemSiz` 净 **+48,740（+10.42%）**；仍为纯增量，`proposal.md:147` 的「核心库下降量大于新增库体积」预期依旧不成立，该权衡已在 `proposal.md:102/148` 改为「收益对象是不加载 RPC 的进程」，本轮不再重复计为发现。

### 二、快照固定（本轮全部数字绑定此表）

评审期间作者于 **00:42–00:47** 再次重编，上轮记录与 `execution-plan.md` 中的 371,536 / 173,176 / `.text` 205,955 / ALLOC 353,098 等数字对应更早构建，**本轮一律作废、不引用**。本轮证据固定为：

| 产物 / 源文件 | mtime（full-iso） | 体积 | md5 |
|---|---|---|---|
| 基线 `out/x86_64_virt/packages/phone/system/lib64/platformsdk/libipc_single.z.so` | 2026-11-12 01:29:54.660230346 | 492,056 B | `d2ee52661cd246e2a986643de9ba0426` |
| 现状 `out/x86_64_virt/communication/ipc/libipc_single.z.so` | 2026-11-13 00:44:50.621050567 | 371,584 B | `37efeee84fef786973c5ca2f8c2bfd39` |
| 现状 `out/x86_64_virt/communication/ipc/libipc_rpc.z.so` | 2026-11-13 00:44:51.742064436 | 173,168 B | `8d6a5d1d8dcb992981f51d8704a01531` |
| `ipc_rpc_loader.cpp` | 2026-11-13 00:42:55.844620722 | 11,737 B | — |
| `ipc_rpc_ops.h` | 2026-11-13 00:43:08.785782241 | 4,970 B | — |
| `ipc_rpc_loader_unittest.cpp` | 2026-11-13 00:45:57.957889239 | 11,178 B | — |
| `execution-plan.md` | 2026-11-13 00:47:37.438127872 | 99,768 B | — |
| UT 二进制 `out/x86_64_virt/tests/unittest/ipc/ipc/core/IpcRpcLoaderUnitTest` | 2026-11-13 00:47:13.182825726 | 327,032 B | — |
| 构建日志 `task3_fix_round7.log`（`ipc_components` + UT） | 2026-11-13 00:45:20.007416583 | 107,960 B | — |
| 构建日志 `task3_fix_round8.log`（仅 UT 目标） | 2026-11-13 00:47:30.771044973 | 100,937 B | — |

`packages/phone/{system,updater}/lib64/platformsdk/` 内**仍无** `libipc_rpc.z.so`，且 `libipc_single.z.so` 仍是基线的 492,056 B（2026-11-12 01:29:54），故 `packages/` 不作为本轮任何验收依据。

### 三、逐项复验

| 上轮项 | 上轮结论 | 本轮证据（命令 + 输出摘要 + 文件:行号） | 本轮判定 |
|---|---|---|---|
| R5 musl 随机句柄致 `dlinfo` 恒失败 | Medium（待作者决定校验手段） | 源码：`musl_template.gni:545-546` 两开关；`soft_musl_ldso_shared.ninja` 含 `DHANDLE_RANDOMIZATION`/`DLOAD_ORDER_RANDOMIZATION` 且 `dynlink_rand.o` 参与链接；`dynlink_rand.c:95-118` `gen_handle` 用 `getrandom`；`dynlink.c:5509-5510` `handle = assign_valid_handle(p)`；`dlinfo.c:5-14` 仅 `__dl_invalid_handle` 后直传；`dynlink.c:6443-6449` 线性比对 `struct dso *`。探针：`probe_dlopen_handle_pie` 3 次，handle 每次随机（`0xcc3ae6…`/`0x58b248…`/`0xc66a41…`）且 `dlinfo ret=-1`；改用 `dladdr(入口符号)` 后 `rc=1` 并给出 `dli_fname` | **闭环**：`dlinfo` 方案不可用、`dladdr` 方向正确，实现 `ipc_rpc_loader.cpp:171-177` 注释与代码一致 |
| R2 预加载后 `l_name` 拼写不一致永久拒绝 | Medium | 生产代码改为「字符串相等 → 否则 `stat` dev/ino 等价」：`ipc_rpc_loader.cpp:186-197`、`:151-163`。探针 `probe_dli_fname_pie` 四种拼写（绝对、裸名经 `LD_LIBRARY_PATH`、symlink 目录 `opencode_link`、`./` 拼写）均 `dladdr rc=1`，`dli_fname` **保持原拼写**，但 `stat` 全部 `dev=2097180 ino=23859815`。新增相对 own-path 直接拒绝：`ipc_rpc_loader.cpp:94-115`（`ownPath[0] != '/'` 即返回空） | **闭环**：`stat` 兜底对四种拼写均成立，`./`/symlink 不会误拒（本轮实测强于上轮） |
| R4 生产 `dladdr`/`dlinfo` 路径零覆盖 | Medium | `--gtest_list_tests` 列出 15 例，全跑 `[ PASSED ] 15 tests`，其中不装 hook 的 3 例：`OwnLibraryDirectoryIsResolvedWithoutHooks`（`:306-323`）、`RelativeOwnLibraryPathIsRejected`（`:325-337`）、`PreloadedImageWithEquivalentPathIsAccepted`（`:339-363`）全部 OK | **闭环**（残例覆盖缺口见 R7） |
| A1 74 条私有导出未隔离、未登记 | Medium（上轮未闭环） | `ipc_rpc_ops.h:33-49` 已有「Registered private ABI (not a public interface)」登记：列出符号组、同构建同目录、拒绝版本节点隔离及理由、ADLT 不支持；`:40-42` 明写 mixing images 为 packaging error | **已承接**（治理登记到位；措辞问题转 R6，无机械守护另行承接） |
| R1 两库安装目录耦合可能错位 | Medium | `ipc_core_module_info.json`：`dest` = `system/lib64/platformsdk/libipc_single.z.so` + `updater/...`，tags `["llndk","platformsdk","sasdk"]`；`ipc_rpc_core_module_info.json`：`dest` = `system/lib64/platformsdk/libipc_rpc.z.so` + `updater/...`，tags `["platformsdk_indirect"]`；`ipc_core/BUILD.gn:47-52` 与 `:85-86` 同为 `platformsdk`；`ipc_single_module_info.json` `install_enable=false`（`ipc_single/BUILD.gn:169`） | **闭环（元数据层）**；实盘安装未验证 |

#### R5 细节（源码 + 探针双证）

`HANDLE_RANDOMIZATION` 打开后 `dlopen` 返回的是 `getrandom` 生成的不透明值而非 `struct dso *`，而 `dlinfo` 先走 `__dl_invalid_handle` 在 `head` 链表里线性查找 `struct dso *`，必然不命中并 `return -1`；`dlinfo.c` 本身不做任何 handle 反解。这与探针 6/6 次（含本轮 3 次）观测一致。因此「`dlinfo` 取 `link_map` 再做路径校验」在本镜像上**从原理到实现都不可用**，改用 `dladdr(入口符号地址)` 是唯一可行且不依赖 handle 的路径。`ipc_rpc_loader.cpp:171-177` 的注释与实现一致，不是事后合理化。

探针口径说明：早期用 `crt1.o` + 默认 PIE 链接的探针有约 1/3 概率启动即段错误（自链接错误，非被测代码缺陷），改用 `Scrt1.o` 重建后 6/6 稳定，**本文只引用稳定版结果**。

#### R2 细节（`stat` 兜底为何成立）

`dladdr` 的 `dli_fname` 是「首次打开该 DSO 时使用的路径串」，不是 realpath；本轮实测裸名仍被 loader 回写成绝对路径 `/tmp/opencode/libprobe_dep.so`，而 `./` 与 symlink 目录两种拼写**原样保留**。因此字符串全等在预加载场景（首次由 `libdbinder` 的 `DT_NEEDED` 以搜索路径拼写映射）确实会失败，必须退化为 `st_dev`+`st_ino` 比较；`NamesSameFile`（`:155-163`）正是该语义，且「同名不同文件」会因 inode 不同而被拒，安全性未被放宽。附带收益：`ownPath[0] != '/'` 时直接返回空（`:104-105`），堵住了 CWD 相对路径被拼成「看似绝对」的风险（上轮 A9 的同类问题在 own-path 侧已关闭）。

#### A1 承接判定

上轮要求二选一：(a) 单开版本节点；(b) 在 `ipc_rpc_ops.h`/map 中登记私有 ABI + 同版本发布约束。作者选了 (b)，且内容比要求更完整（含「为何拒绝版本节点隔离」「ADLT 不支持」「map 需同步修改」三条）。因此 **A1 不再计为未闭环**；残留两点转 R6（措辞与实测 tags 不符）与「无机械守护」（TASK-6 承接）。`libipc_single_map` 的版本节点结构本轮确实未变（`1.0 {` 在 `:16`、used-by-rpc 块 `:205-282`、`};` 在 `:283`），这与「选择 (b)」一致，不是遗漏。

### 四、体积与资源对照（四口径，均在本轮快照上重测）

| 口径 | 基线 | single | rpc | 合计 | 净变化 |
|---|---|---|---|---|---|
| 文件大小（`ls -l`，stripped，镜像口径） | 492,056 | 371,584 | 173,168 | 544,752 | **+52,696（+10.71%）** |
| `llvm-size` `Total` | 483,201 | 366,036 | 167,736 | 533,772 | +50,571（+10.47%） |
| `Total − .gnu_debugdata` | 467,177 | 353,720 | 161,464 | 515,184 | +48,007（+10.28%） |
| ELF `LOAD p_memsz` 之和（真实常驻映射） | 467,892 | 354,444 | 162,188 | 516,632 | **+48,740（+10.42%）** |

单库视角与代码段：

| 指标 | 基线 | single | 变化 | rpc |
|---|---|---|---|---|
| 文件大小 | 492,056 | 371,584 | **-120,472（-24.48%）** | 173,168 |
| `.text` | 289,951 | 206,579 | **-83,372（-28.75%）** | 90,717 |
| `LOAD p_memsz` | 467,892 | 354,444 | -113,448（-24.25%） | 162,188 |

`.text` 合计 297,296 → 净 **+7,345（+2.53%）**。四口径的合计净增不同，是因为 `Total` 未计 ELF 头/节表/对齐，而 `Total − .gnu_debugdata` 又扣掉了压缩调试段、`LOAD memsz` 还包含 bss；**对外引用应以 `ls -l` 为准，内存占用以 `LOAD memsz` 为准**，两者不可混用（R8 即此问题）。

元数据增量（single + rpc vs 基线）：

| 节 | 基线 | single + rpc | 增量 |
|---|---|---|---|
| `.plt` | 5,184 | 8,672 | +3,488 |
| `.rela.plt` | 7,752 | 12,960 | +5,208 |
| `.dynsym` | 10,824 | 17,832 | +7,008 |
| `.dynstr` | 21,669 | 36,915 | +15,246 |
| 合计 | — | — | **+30,950** |

导出面精确复核（修正上轮口径）：single `@@1.0` **167 → 235**，逐名求差集为**新增 78 名、消失 10 名**（净 +68）；上轮「+74 新导出 / 9 消失」与实际差集不符（167+74-9=232≠235），本轮以差集为准。78 个新名中 **74 个**被 rpc 的 `JUMP_SLOT` 引用；未被 `JUMP_SLOT` 引用的 4 个是 `_ZN4OHOS14InvokerRawDataC2Em`/`D2Ev`、`_ZN4OHOS20DBinderSessionObjectC2E…`/`D2Ev`（ctor/dtor）。rpc `llvm-nm -D --defined-only` = **11**，rpc `JUMP_SLOT` = **207**。single 新增未定义导入仅 `stat` 与 `_ZNSt3__h11__call_onceERVmPvPFvS2_E`；`libc.so` 实测 `T stat`（`0x118410`），故 `stat` 可直接解析，不引入新依赖。rpc 无 `dlinfo`/`dladdr`/`stat` 未定义（loader 只在 single 内，未链接进 rpc）。

### 五、本轮实测原始输出

```text
$ ls -l --time-style=full-iso <3 产物> ; md5sum <3 产物>
492056  2026-11-12 01:29:54.660230346  .../platformsdk/libipc_single.z.so   d2ee52661cd246e2a986643de9ba0426
371584  2026-11-13 00:44:50.621050567  out/x86_64_virt/communication/ipc/libipc_single.z.so   37efeee84fef786973c5ca2f8c2bfd39
173168  2026-11-13 00:44:51.742064436  out/x86_64_virt/communication/ipc/libipc_rpc.z.so      8d6a5d1d8dcb992981f51d8704a01531

$ llvm-size -A <each>   # 节名 / size
baseline  .text 289951  .plt 5184  .rela.plt 7752   .dynsym 10824  .dynstr 21669  .gnu_debugdata 16024  Total 483201
single    .text 206579  .plt 5344  .rela.plt 7992   .dynsym 12384  .dynstr 25614  .gnu_debugdata 12316  Total 366036
rpc       .text  90717  .plt 3328  .rela.plt 4968   .dynsym  5448  .dynstr 11301  .gnu_debugdata  6272  Total 167736

$ llvm-readelf -l -W <each> | grep '^  LOAD'      # FileSiz / MemSiz
baseline  0x027044/0x027044  0x0480f0/0x0480f0  0x002b70/0x002b70  0x00000d/0x000710   MemSiz 合计 467892
single    0x02073c/0x02073c  0x033bf0/0x033bf0  0x002358/0x002358  0x000009/0x000208   MemSiz 合计 354444
rpc       0x00f364/0x00f364  0x016f70/0x016f70  0x001390/0x001390  0x00000c/0x000328   MemSiz 合计 162188

$ llvm-readelf --dyn-syms -W libipc_single.z.so | grep -c '@@1.0'      → 235   （基线 167）
$ llvm-nm -D --defined-only libipc_rpc.z.so | wc -l                     → 11
$ llvm-readelf -r -W libipc_rpc.z.so | grep -c JUMP_SLOT               → 207
$ 新增导出 78 / 消失 10 / 其中被 rpc JUMP_SLOT 引用 74
$ 未被引用的 4 个：InvokerRawDataC2/D2、DBinderSessionObjectC2/D2
$ llvm-nm -D --undefined-only libipc_rpc.z.so | wc -l                  → 215（其中 weak 4：__at_fini/__cxa_finalize/__deregister_frame_info/__register_frame_info）
$ llvm-nm -D --undefined-only libipc_rpc.z.so | egrep 'dlinfo|dladdr| stat$'   → 无输出
$ llvm-nm -D libc.so | grep ' T stat$'                                  → 0000000000118410 T stat
$ single 新增未定义导入：stat、_ZNSt3__h11__call_onceERVmPvPFvS2_E（仅此 2 条）

$ bash /tmp/opencode/check_rpc_symbols_resolved.sh
identical to libipc_single.z.so weak-undefined set
=== 5. unresolved = undefined - defined-by-NEEDED ===
none
RESULT: PASS (211 undefined, all covered by NEEDED closure)

$ tail -3 task3_fix_round7.log / task3_fix_round8.log
round7: =====build  successful=====  2026-11-13 00:45:20   (grep -c "error:" → 0)
round8: =====build  successful=====  2026-11-13 00:47:30   (grep -c "error:" → 0)

$ ld-musl-x86_64.so.1 --library-path <system/lib64>:<chipset-sdk-sp>:<libc>:<communication/ipc> \
      tests/unittest/ipc/ipc/core/IpcRpcLoaderUnitTest --gtest_list_tests
IpcRpcLoaderTest.
  LoadFailureIsCachedOnce / LoadUsesResolvedAbsolutePath / UnresolvableLibraryPathIsRejected
  PathVerificationFailureIsRejected / LoadSuccessIsCached / ConcurrentFirstUseReturnsSameOps
  AbiVersionMismatchIsRejected / MissingSymbolIsRejected / NullFunctionPointerIsRejected
  GetOpsNullResultIsRejected / PeekAfterGetReturnsSameOps / PeekDoesNotLoad
  OwnLibraryDirectoryIsResolvedWithoutHooks / RelativeOwnLibraryPathIsRejected
  PreloadedImageWithEquivalentPathIsAccepted
  （共 15 例）

$ 同上，全跑
[==========] 15 tests from 1 test suite ran. (20 ms total)
[  PASSED  ] 15 tests.

$ ld-musl-x86_64.so.1 --library-path /tmp/opencode /tmp/opencode/probe_dlopen_handle_pie /tmp/opencode/libprobe_dep.so
dlopen(/tmp/opencode/libprobe_dep.so) handle = 0xcc3ae62557c8ad73
dlinfo(RTLD_DI_LINKMAP) ret = -1, map = 0
dladdr(main) rc = 1, fname = /tmp/opencode/probe_dlopen_handle_pie
（另两次 handle 0x58b2485ca06a6a8a / 0xc66a418bec9b6162，dlinfo 均 -1）

$ ld-musl-x86_64.so.1 --library-path /tmp/opencode /tmp/opencode/probe_dli_fname_pie <四种拼写> probe_dep_symbol
绝对       handle=0x5840728da0a36342  dladdr rc=1 fname=/tmp/opencode/libprobe_dep.so        stat dev=2097180 ino=23859815
裸名       handle=0x4851f258ec1f12b3  dladdr rc=1 fname=/tmp/opencode/libprobe_dep.so        stat dev=2097180 ino=23859815
symlink 目录 handle=0x6cf61a96e59767cd dladdr rc=1 fname=/tmp/opencode_link/libprobe_dep.so  stat dev=2097180 ino=23859815
`./` 拼写   handle=0x42d3746e50a440e6  dladdr rc=1 fname=/tmp/opencode/./libprobe_dep.so     stat dev=2097180 ino=23859815
```

说明：`dli_fname` 保留打开时的拼写（裸名被回写为绝对路径，`./` 与 symlink 目录原样保留），四种拼写指向同一 inode，这正是 `NamesSameFile`（`ipc_rpc_loader.cpp:155-163`）必须存在的原因，也证明它不会误拒合法预加载。

### 六、新发现（第 3 轮，均为 Low）

| 编号 | 严重度 | 维度 | File:Line | 问题 | 证据 | 建议 |
|---|---|---|---|---|---|---|
| R6 | Low | ① 文档准确性（ABI 表述） | `ipc/native/src/core/dbinder/include/ipc_rpc_ops.h:40-42` | 登记段写「Those symbols are neither an innerapi nor an SDK surface」，但承载它们的 `libipc_single.z.so` 实测 tags 为 `llndk/platformsdk/sasdk`，且 74 条就落在对外 `1.0` 版本节点内。读者可能据此误判「这些符号不会随 SDK 发布」，而实际会随 `libipc_single` 一起进入 innerapi 导出集合。语义应区分「已导出」与「承诺为受支持 API」 | `ipc_core_module_info.json`：`innerapi_tags=["llndk","platformsdk","sasdk"]`；`libipc_single_map:16`（`1.0 {`）与 `:205-282`（used-by-rpc 块）同属一个节点；single `@@1.0` = 235 | 改为「These symbols are exported by the same `1.0` node as the public API, but are **not a supported API/ABI promise**: they exist only for the sibling `libipc_rpc.z.so` from the same build and may change without notice」；同时把 `:40` 的「neither an innerapi nor an SDK surface」限定为「not a *contract*」 |
| R7 | Low | ② 验证覆盖（R4 残余） | `test/unittest/ipc/native/core/ipc_rpc_loader_unittest.cpp:358-360` | 注释称「A different file with the expected basename must still be rejected」，但负例构造的是 `libipc_rpc_absent_probe.z.so`——该名字**根本不存在**，命中的是 `stat()` 失败分支，而非「同名且真实存在的不同文件」的 inode 不相等分支。`NamesSameFile` 的关键安全性（同名不同 inode 必须拒绝）仍未被断言 | 该文件在本机不存在（`ls` 报 No such file）；`NamesSameFile`（`ipc_rpc_loader.cpp:155-163`）在两个 `stat` 都成功时才比较 dev/ino | 用两个真实存在的同名 `.so`（例如在临时目录各放一个 `libipc_rpc.z.so` 副本）构造用例，断言 inode 不同即返回 false；再加「symlink 指向同一文件 → true」正例 |
| R8 | Low | ② 证据时效 / 口径一致性 | `codespec/changes/ipc/draft-20261112-split-rpc-so/execution-plan.md:411`、`:416`、`:421`、`:438`、`:444` | Actual Result 与验证表仍绑定 00:33 早期构建：rpc 173,160（现 173,168）、single `.text` 205,955（现 206,579）、`.text` 净 +6,721（现 +7,345）、single「ALLOC」353,098（本轮同口径 353,720、真实 `LOAD memsz` 354,444）、rpc「ALLOC」161,464（同口径不变、真实 `LOAD memsz` 162,188）、RPC 进程「ALLOC」514,562（同口径 515,184、真实 516,632）。更关键的是「ALLOC」实为 `Total − .gnu_debugdata`，与真实 `LOAD MemSiz` 每库差 715–724 B，下游按此做内存核算会系统性偏低 | 本轮 `llvm-size` / `llvm-readelf -l` 四口径实测（见第四节）；两库 `Total−debugdata` 与 `LOAD memsz` 之差分别为 724/724 B，基线 715 B | TASK-6 统一回填为「文件 / `Total` / `Total−debugdata` / `LOAD MemSiz`」四行，并把「ALLOC」一律改名为 `LOAD MemSiz`；引用时同时给出产物 mtime 与 md5 |
| R9 | Low | ② 证据时效（流程） | `/tmp/opencode/task3_fix_round7.log`、`ipc_rpc_loader_unittest.cpp`、UT 二进制、`task3_fix_round8.log` | 本轮存在「源码已 15 例、二进制仍 14 例」的窗口：round7 构建于 00:45:20，早于 `unittest.cpp` 最后一次修改（00:45:57），直到 round8（二进制 00:47:13 / 日志 00:47:30）才对齐。若在窗口内取二进制跑结果作为证据，会得到「少一例但全绿」的假阳性 | 四个 mtime 的先后顺序如上；当前二进制 `--gtest_list_tests` 确为 15 例且 `PASSED 15 tests` | TASK-6 在采集证据前断言「被测二进制 mtime ≥ 其全部源文件 mtime」，并在证据里记录二进制 md5；构建脚本可用 `--gtest_list_tests` 的用例数做交叉校验 |

上轮 A3（PLT 化）、A4（arm64 链接选项）、A6（注释与实现不符）、A7（`common_source` 命名）、A8（设计签名漂移）、B9（`proposal.md:148` ROM 数字）本轮**未变化，维持 Low**。其中 B9 的数字应随 R8 一并更新为 **+52,696 B（+10.71%）**（`proposal.md:148` 现为 +52,656 / +10.70%）。

### 七、阻塞判定（第 3 轮）

| 项 | 判定 |
|---|---|
| Critical | 无 |
| High | 无 |
| Medium | 无（A1/R1/R2/R4/R5 本轮均已闭环或已承接） |
| Low | R6、R7、R8、R9；维持 A3、A4、A6、A7、A8、B9 |
| 阻塞发现数 | **0** |
| 结论 | **PASS（维持）** |

理由：TASK-3 的功能判据（4 个 RPC TU 编入独立库、入口唯一、依赖单向、fail-closed、构建与单测通过）本轮全部**独立复跑成立**；上轮 5 项待验均有可执行证据闭环。R6–R9 全部是措辞、用例构造与数字回填问题，不涉及行为缺陷，也不改变 fail-closed 语义，按规则不计阻塞。

### 八、TASK-4/6 承接清单

1. **R1 实盘**：在 `make_packages` 可用后，断言 `realpath(dirname(libipc_single))` == `realpath(dirname(libipc_rpc))`，且两文件同时存在于 `system` 与 `updater`；当前仅有 `*_module_info.json` 声明级证据。
2. **R2 运行期**：在 `libdbinder` 已通过 `DT_NEEDED` 预加载 `libipc_rpc` 的真实进程里触发首次 RPC，确认 `stat` 兜底命中且能力表发布成功（本轮仅本机探针 + 单测）。
3. **R4/R7 用例**：补「同名且真实存在的不同文件」负例与「symlink 同文件」正例；保留现有 3 个无 hook 用例。
4. **A1/R6 守护**：加脚本断言「`libipc_single_map` 的 used-by-rpc 块集合 == rpc 实际引用的 single 导出集合 == 78 条新增减去 ctor/dtor」，防 map 与代码漂移；同时按 R6 修正措辞。
5. **R8 回填**：`execution-plan.md:411/416/421/438/444` 换成第四节四口径数字，并把「ALLOC」改名 `LOAD MemSiz`。
6. **R9 流程**：证据采集前校验产物 mtime ≥ 源文件 mtime，记录产物 md5。
7. **A3/A4**：TASK-6 性能对照把 PLT 化列为已知变量（预期落在噪声内）；arm64 产品上确认 `ipc_rpc` 是否需要 `--emit-relocs/--no-relax`（本机 x86_64_virt 无法实证）。
8. **A6/A7/A8/B9**：注释与命名清理、设计签名同步、`proposal.md` ROM 数字更新，均不阻塞。

### 九、未在本机验证 / 无法验证项

- `packages/phone/{system,updater}/lib64/platformsdk/libipc_rpc.z.so` 未产出（`make_packages` 环境阻塞），实盘安装与镜像内布局未验证；`packages/` 内 single 仍为基线旧件。
- 无 `hdc`、无 `qemu-system-x86_64`：真实设备/模拟器上的首次 RPC、预加载路径、失败进程 maps 残留均未运行验证。
- arm64 链接选项（A4）本机为 x86_64_virt，无法实证。
- 上述未验证项均**不计为缺陷**，已在第八节列入承接。