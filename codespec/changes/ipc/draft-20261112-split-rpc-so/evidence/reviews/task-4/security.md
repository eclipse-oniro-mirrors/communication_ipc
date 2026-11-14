---
task_id: TASK-4
risk: R2
seat: security
verdict: PASS
blocking_findings: 0
---

# TASK-4 安全与攻击面评审（⑤）

## 结论

**`verdict = PASS`，`blocking_findings = 0`。**

TASK-4 只做「构建期开关 + 关闭态取表方式切换」，**没有引入新的校验绕过、库劫持或提权路径**：

1. 两种形态共用同一个 `CheckCapabilityTable()`（`ipc_rpc_loader.cpp:70-85`），静态分支（`:277`）与动态分支（`:334`）的「空表 / ABI 版本 / 条目完整」三关完全一致，关闭态没有更宽松的判定；两形态都由同一个 `std::call_once`（`:353`）驱动，失败只打 1 条日志并把表缓存为 `nullptr`，不存在「失败后重试成功」或「部分表被发布」的窗口。
2. 关闭态的 `IpcRpcGetOps` 引用被完全本地化：`.dynsym` 中 `IpcRpc` 命中 **0**，动态重定位中 `IpcRpc` 命中 **0**，`libipc_rpc` 字符串命中 **0**，`dlsym` 符号名与路径解析/映像校验文案全部消失（只余 1 条 `<static link>` + 3 条共享失败文案）——即**无法被先序 DSO 抢占，也无法被诱导去装载外部库**。
3. 关闭态 `libipc_single` 的 `DT_NEEDED` 为 **12** 条、`libipc_rpc` 命中 **0**；`libdbinder` 为 **9** 条、`libipc_rpc` 命中 **0**，其 8 个 `DBinderSoftbusClient::*` 未定义符号全部能在关闭态 `libipc_single` 的导出集中找到。依赖方向与拆分前一致。

未计入阻塞、但需登记的是**回退形态的导出面并不等于拆分前**（SC-01，Medium）：关闭态导出 **245** 条，而拆分前记录值为 167（详见发现清单）。这是作者已在 `execution-plan.md` 中登记的「偏离 3」，本席位独立复核后同意「不阻塞」，但它与 `BR-6`「产物结构…回退到拆分前状态」的字面表述存在偏差，应作为已知偏离进入设计风险表或由条件化版本脚本收口。

本席位为只读评审，未执行 `./build.sh`；未使用 `out/x86_64_virt`（当时正在回切默认态重建），产物证据取自作者保留的 `/tmp/opencode` 冻结副本。

## 1. 审查范围与证据口径

| 类别 | 内容 |
|---|---|
| 变更文件（TASK-4） | `config.gni`、`interfaces/innerkits/ipc_single/BUILD.gn`、`interfaces/innerkits/ipc_core/BUILD.gn`、`interfaces/innerkits/libdbinder/BUILD.gn`、根 `BUILD.gn`、`libipc_single_map`（仅注释）、`ipc_rpc_loader.cpp`（静态分支）、`ipc_rpc_ops.h`（入口声明） |
| 安全关键读点 | 开关默认值与作用域、两形态取表路径、能力表校验、失败语义、动态装载代码是否残留在关闭态产物、导出面、依赖方向、日志内容 |
| 产物证据 | `/tmp/opencode/split_off_libipc_single.z.so`（488,464 B）、`split_off_libdbinder.z.so`（166,600 B）、`split_off_exports.txt`（245 行），均为关闭态构建（`task4_green5_off.log`，EXIT 0）的冻结副本；开启态仅用 `task4_green4_open.log`/`task4_green4_enabled.txt` 的构建与 NEEDED 记录 |
| 工具 | `code/prebuilts/clang/ohos/linux-x86_64/llvm/bin/{llvm-nm,llvm-readelf,llvm-strings,llvm-objdump,llvm-cxxfilt}` |
| 未采用 | `out/x86_64_virt` 下的现行产物（评审时正在重建回默认开关，时间戳不可靠）；作者在 `execution-plan.md` 中的结论文字（仅作交叉对照，不作为证据） |

## 2. 维度结论

| 维度 | 结论 | 证据 |
|---|---|---|
| 校验语义两形态等价 | 通过 | `CheckCapabilityTable()` 单点实现（`ipc_rpc_loader.cpp:70-85`），静态 `:277`、动态 `:334` 调用；静态失败 `:278-281` 单条日志 + `g_ops.store(nullptr)`；动态失败路径同样只存 `nullptr`；两形态共用 `:353` 的 `call_once` |
| 关闭态「无外部装载」是否成立 | 通过（带 SC-03） | 产物中 `libipc_rpc` 字符串 0、`IpcRpcGetOps` 动态符号 0、`IpcRpc` 重定位 0、路径解析/映像校验文案 0；但 loader 之外仍有 2 个 `dlopen`（既有软总线裸名加载、自身路径重开） |
| 关闭态符号可抢占性 | 通过 | 版本脚本 `local: *` 生效，`IpcRpcOpsGet` 等 loader 符号不在 `.dynsym`；对 `IpcRpcGetOps` 的调用为库内直接调用（重定位 0），不可被先序 DSO 替换 |
| 依赖方向与符号闭合 | 通过 | 关闭态 single NEEDED 12 条（与开启态逐条相同）、`libipc_rpc` 0；dbinder NEEDED 9 条、`libipc_rpc` 0；dbinder 的 8 个 `DBinderSoftbusClient::*` 未定义符号全部由 single 导出满足 |
| 回退形态导出面（是否等于拆分前） | 部分通过（SC-01 Medium） | 关闭态导出 245；拆分前记录值 167；`libipc_single_map` 161→235 条（+74），新增含授权表/身份字段/裸数据改写入口；开关不改变这张表 |
| 开关配置误用 | 通过（带 SC-02 Low） | 开关只在 5 个构建文件出现（9 处），C++ 源码 0 处；`ipc_rpc` 目标、`ipc_rpc_core`、根 deps、dbinder deps 四处均有 gate；默认 `true`（`:25`） |
| 日志/信息泄露 | 通过 | 关闭态失败日志路径为字面量 `<static link>`，产物内无 `/srv/workspace`、`/home/huawei`、`/system`、`/vendor`、`/data` 路径串；无 uid/pid/token/会话名 |
| 关闭态自动化覆盖 | 部分通过（SC-04 Low） | loader 单测始终按动态形态编译（`ipc_rpc_loader.cpp:366` 的 `!defined(IPC_RPC_STATIC_LINK)`），静态分支只经编译与链接期校验，无运行期用例 |

## 3. 两形态安全等价性（核心判断）

| 关注点 | 关闭态（`IPC_RPC_STATIC_LINK`） | 开启态（默认） | 是否等价 |
|---|---|---|---|
| 取表入口 | `IpcRpcOpsGet()`（`:276`）→ 库内直接调用 `IpcRpcGetOps()` | 解析自身目录 → 绝对路径 `dlopen` → `dlsym` → `dladdr` 校验 → `IpcRpcGetOps()` | 等价（表内容来源不同，但来源可信性在关闭态更强） |
| 空表判定 | `:277` → `failure="IpcRpcGetOps returned null"` | `:334` 同函数同文案 | 等价 |
| ABI 版本判定 | 同函数，精确比较 `IPC_RPC_OPS_ABI_VERSION` | 同 | 等价 |
| 条目完整性 | 同函数 `IsOpsComplete()` 9 项非空 | 同 | 等价 |
| 失败语义 | 1 条 `ZLOGE`（`:278`）→ `g_ops=nullptr` → 调用方走既有错误码 | 1 条 `ZLOGE`（`:341`）→ `g_ops=nullptr` | 等价（fail-closed） |
| 并发 | `std::call_once(g_loadOnce, LoadOnce)`（`:353`） | 同 | 等价 |
| 重入 | 静态分支不持锁、不触发外部库静态构造 | 动态分支注释明确禁止在 rpc 静态初始化期回调 `Get()` | 关闭态无新增重入面 |
| 可替换性 | `IpcRpcGetOps` 不在 `.dynsym`，调用为重定位 0 的库内引用 | 入口符号经 `RTLD_LOCAL` + `dlsym(handle)` + `dladdr` 校验 | 关闭态不可劫持；开启态按 TASK-3 已闭环方案 |

**结论**：TASK-4 没有把任何校验从「关闭态」挪走或放宽；它只是把「同一张表从哪个地址取」换成编译期绑定。**关闭态的安全性不低于开启态**（少了跨 DSO 信任边界）。

## 4. 关闭态攻击面独立复核

1. **动态装载代码已整段消失**：`strings` 对 `libipc_rpc`、`cannot resolve the directory`、`resolved entry is not provided` 均 0 命中，且不存在独立的 `IpcRpcGetOps` 符号名字面量（仅剩校验失败文案 `IpcRpcGetOps returned null`）；`<static link>` 恰好 1 条；仅余 3 条共享校验文案。说明 `#ifndef IPC_RPC_STATIC_LINK` 覆盖的路径解析、打开、符号解析、映像校验代码均未编入。
2. **入口符号不可见、不可抢占**：`llvm-readelf --dyn-syms | grep -ic ipcrpc` = 0；`--relocs | grep -c IpcRpc` = 0。即使攻击者能在同进程先序装载同名/伪造 DSO，也没有可被满足的动态引用或可被覆盖的导出符号。
3. **但「无 dlopen」不等于产物无动态装载**：关闭态产物仍有 `dlopen` 调用点 2 个、`dlsym` 8 个、`dladdr` 3 个，全部来自既有代码——`dbinder_softbus_client.cpp:31,152`（`SOFTBUS_PATH_NAME = "libsoftbus_client.z.so"`，裸名 + 搜索路径）与 `ipc_thread_pool.cpp:46,51`（`dladdr(InitIpcSo)` 后按 `dli_fname` 重开）。二者都不是本次新增，但会削弱「关闭态 = 无动态装载」的直觉预期（SC-03）。
4. **导出面**：关闭态 245 条，`comm` 相对 HEAD 版本脚本新增 74 条去重名（含 `IPCProcessSkeleton::AttachOrUpdateAppAuthInfo`、`QueryCommAuthInfo`、`QueryAppInfoToStubIndex`、`DetachAppAuthInfoBySocketId`、`DBinderSessionObject::SetPeerUid/SetPeerPid`、`MessageParcel::GetRawData/RestoreRawData`）。这些符号在默认开启态同样导出（TASK-3 S-02），关闭开关**不会**把它们收回（SC-01）。
5. **无新增权限/协议面**：无新 transaction code、无新 ioctl、无新权限位、无新 fd 传递；开关只存在于构建文件，运行期不可切换，故不存在「被降权进程改开关」的路径。
6. **日志**：失败路径参数为字面量 `<static link>`，不含构建路径或安装路径；产物内绝对路径扫描 0 命中。

## 5. 独立核验记录

| # | 命令 / 操作（本方执行） | 实际输出摘要 |
|---|---|---|
| 1 | `grep -n "CheckCapabilityTable\|call_once\|g_loadOnce" ipc/native/src/core/dbinder/source/ipc_rpc_loader.cpp` | `:49` once_flag；`:70` 定义；`:277` 静态调用；`:334` 动态调用；`:353` `call_once(g_loadOnce, LoadOnce)` |
| 2 | `sed -n 272,284p` 同文件 | 静态分支：`IpcRpcGetOps()` → `CheckCapabilityTable` → 失败 `ZLOGE("<static link>")` + `g_ops.store(nullptr)`；成功 `g_ops.store(ops)` |
| 3 | `llvm-nm -D --defined-only /tmp/opencode/split_off_libipc_single.z.so \| wc -l` | `245`（开启态记录值 235；拆分前记录值 167） |
| 4 | `llvm-readelf --dyn-syms <关闭态 single> \| grep -ic ipcrpc` | `0`（`IpcRpcGetOps`/`IpcRpcOpsGet` 均未导出） |
| 5 | `llvm-readelf --relocs <关闭态 single> \| grep -c IpcRpc` | `0`（库内引用已静态解析，无跨 DSO 绑定） |
| 6 | `llvm-strings <关闭态 single> \| grep -c <各串>` | `libipc_rpc`=0；`cannot resolve the directory`=0；`resolved entry is not provided`=0；`<static link>`=1；`capability table ABI version mismatch`/`...null entries`/`IpcRpcGetOps returned null` 各 1 |
| 7 | `llvm-objdump -d <关闭态 single>`，分别 `grep -c "callq.*<dlopen"` / `<dlsym` / `<dladdr` | `dlopen`=2、`dlsym`=8、`dladdr`=3（均非 loader 路径；来源见第 4 节第 3 点） |
| 8 | `llvm-readelf -r <关闭态 single>`，分别统计 `dlopen` / `dladdr` / `dlsym` 的 `R_X86_64_JUMP_SLOT` | 各 1 个槽（libc 符号解析槽，非新增依赖） |
| 9 | `llvm-readelf -d <关闭态 single> \| grep NEEDED` | 12 条：utils / ipc_common / sec_shared / backtrace_local / ffrt / hilog / hitracechain / selinux / hisysevent / libc / libc++ / ubsan_minimal；`libipc_rpc` 0 条 |
| 10 | `llvm-readelf -d /tmp/opencode/split_off_libdbinder.z.so \| grep NEEDED` | 9 条，含 `libipc_single.z.so`，`libipc_rpc` 0 条（开启态为 10 条且含 `libipc_rpc`） |
| 11 | `llvm-nm -D -u <关闭态 dbinder> \| grep -E "DBinderSoftbusClient\|DBinderDatabusInvoker"` 与导出集 `comm` | 未定义 8 条；与关闭态 single 导出集比对 `missing=0 / matched=8` |
| 12 | `awk` 统计 `libipc_single_map` 与 `git show HEAD:` 版本 | 当前 235 条；HEAD 161 条（+74）；`/* used by libipc_rpc */` 块 `:207-281`，74 条；`local: *` 仍在文末（`libipc_rpc_map:49-50` 亦然） |
| 13 | 导出名集合比对（`nm -D` 去 `@@1.0` + `comm` vs HEAD 版本脚本） | 关闭态去重名 221；相对 HEAD 新增 **74**；HEAD 有 14 条在产物中无匹配（`unsigned int` 旧重载、`virtual thunk to` 拼写、`CALLING_USER_INFO_ENABLED` 等特性宏关闭所致），与开关无关 |
| 14 | `llvm-readelf -V <关闭态 single>` | `.gnu.version_d` 仅 `1.0` 一个版本节点（+BASE）；`FLAGS` 仅 `BIND_NOW`，无 `SYMBOLIC` |
| 15 | `llvm-strings <关闭态 single> \| grep -Ec "/srv/workspace\|/home/huawei\|/system/\|/vendor/\|/data/"` | `0`（无构建路径、无安装路径） |
| 16 | `grep -rn "ipc_feature_rpc_split_enabled" --include=*.gn --include=*.gni --include=*.cpp --include=*.h` | 9 处 / 5 个构建文件（`config.gni`、根 `BUILD.gn`、`ipc_core/BUILD.gn`×2、`ipc_single/BUILD.gn`×4、`libdbinder/BUILD.gn`）；源码 0 处 |
| 17 | `sed -n 21,25p config.gni`；`sed -n 122,143p`、`:191` `ipc_single/BUILD.gn`；`:87-92` `libdbinder/BUILD.gn`；`:30-32` 根 `BUILD.gn` | 默认 `true`；关闭态 `sources = common_source + ipc_rpc_entry.cpp` 且 `defines += ["IPC_RPC_STATIC_LINK"]`；`ipc_rpc` 目标整体 gate；dbinder 依赖 gate；根 `ipc_rpc_core` gate |
| 18 | `grep -c "libipc_rpc.z.so" /tmp/opencode/task4_green5_off.log`；`grep -n "ipc_rpc_entry.o" ...`；`grep -n "SOLINK.*libipc_rpc" /tmp/opencode/task4_green4_open.log` | 关闭态日志命中 0；`[31/77] CXX .../ipc_single/ipc_rpc_entry.o`；开启态 `[58/77] SOLINK communication/ipc/libipc_rpc.z.so`；两次构建均 `EXIT=0` |
| 19 | `grep -rn "IPC_RPC_STATIC_LINK"`（全仓，排除 loader 自身） | 仅 `ipc_single/BUILD.gn:127,142`（注释与 define）与 `ipc_rpc_ops.h:105`（注释）；未渗入生产代码分支 |
| 20 | `sed -n 366,376p ipc_rpc_loader.cpp`；`test/unittest/ipc/native/core/BUILD.gn:182-183` | 测试钩子仅在 `IPC_RPC_UNIT_TEST && !IPC_RPC_STATIC_LINK` 下编译；单测目标直接编译 loader.cpp 且不定义该宏 → 静态分支无 UT |

## 6. 发现清单

| ID | 严重度 | 位置 | 问题 | 影响 | 建议 | 状态 |
|---|---|---|---|---|---|---|
| SC-01 | **Medium**（非阻塞） | `interfaces/innerkits/ipc_single/libipc_single_map:207-281`；`execution-plan.md`「偏离/修正 3」 | 关闭态导出 **245** 条（相对拆分前记录值 167 为 +78 个 dynsym / +74 个去重名），因为 74 条 RPC 依赖符号写在唯一一份版本脚本中，无法按开关分支。回退只恢复 `DT_NEEDED` 与运行路径，**不恢复拆分前的导出面**。新增面含 `IPCProcessSkeleton::AttachOrUpdateAppAuthInfo`/`QueryCommAuthInfo`/`QueryAppInfoToStubIndex`/`DetachAppAuthInfoBySocketId`、`DBinderSessionObject::SetPeerUid/SetPeerPid`、`MessageParcel::GetRawData/RestoreRawData` | 与 `BR-6`/AC-4.1「回退到拆分前状态」的字面要求存在偏差；回退形态仍保留与开启态相同的同进程符号改写面（需能注入同进程 DSO 才可利用，且该面在默认形态本来已存在） | 二选一：(a) 用 GN 选择两份版本脚本（`libipc_single_map` / `libipc_single_map_pre_split`），关闭态严格回到拆分前清单；(b) 若判定两份清单漂移风险更高，则在 `design.md` 风险表新增一行，明确「关闭态导出面 = 245，不随开关回收」并给出依据，同时把该偏离写进 `spec.md` 的 AC-4.1 说明 | 非阻塞（作者已登记，建议补设计层登记） |
| SC-02 | Low（非阻塞） | `config.gni:21-25` | 注释称 `Set to false to build the pre-split shape`，未说明关闭态仍导出 245 条符号、仍保留 2 个 `dlopen` 调用点；该描述会让运维/产品把「关开关」理解为攻击面完全回到拆分前 | 安全预期偏差（不是可利用缺陷） | 在注释中补一句「仅回退装载方式与依赖结构；导出面与既有裸名 `dlopen` 不随开关回收，见 SC-01」 | 非阻塞 |
| SC-03 | Low（既有/非本次引入） | `ipc/native/src/core/dbinder/source/dbinder_softbus_client.cpp:31,152`；`ipc_thread_pool.cpp:46,51` | 关闭态产物仍含裸名 `dlopen("libsoftbus_client.z.so")`（搜索路径解析，无路径固定/加载后校验），以及 `dladdr` 后按 `dli_fname` 重开自身库 | 与本次 loader 已收口的「绝对路径 + `dladdr` 校验」形成口径不一致；利用需能控制搜索路径或写先序目录，属既有面，不因 TASK-4 扩大 | 登记为残余风险（`design.md` 风险表 / TASK-6），后续按与 loader 相同的「绝对路径 + 映像校验」模式加固 | 非阻塞 |
| SC-04 | Low（非阻塞） | `ipc_rpc_loader.cpp:366`；`test/unittest/ipc/native/core/BUILD.gn:182-183`；`ipc_single/BUILD.gn:242` | 关闭态 `LoadOnce()` 静态分支没有任何单测覆盖（UT 固定按动态形态编译）；其正确性目前只依赖编译期符号解析与 `ipc_single` 的 `-Wl,--no-undefined` | 若未来有人把 `IpcRpcGetOps()` 调用移出 `#ifdef`，开启态会在链接期失败（有保护），但关闭态的失败语义（单日志 + 缓存 `nullptr`）无回归网 | 由 TASK-5 增加一个以 `IPC_RPC_STATIC_LINK` 编译的 loader 用例，至少覆盖「表不完整 → 1 条日志 + `Peek()==nullptr` + 第二次调用不再取表」 | 非阻塞 |

## 7. 未验证项（显式声明）

1. **拆分前基线二进制**：工作区已无拆分前产物，`167` 来自 TASK-3/提案记录值，无法用同一工具链对同一文件重新测量；SC-01 的「+78/+74」以当前 HEAD 版本脚本与关闭态产物的集合比对为依据。
2. **设备/模拟器实跑**：本机无 `hdc`、无可用模拟器，未在真实进程里触发「关闭态首次 RPC」与并发首调。
3. **`packages/` 实盘安装**：同 TASK-3，`make_packages` 仍被环境阻塞，安装面只到 module_info/依赖声明级。
4. **其它 `target_cpu`**：产物证据仅 `x86_64_virt`；结论（版本脚本、`local: *`、静态分支）与架构无关，但未逐 target 复核。
5. **SELinux**：未实测关闭态单库与开启态双库的标签差异（本次不新增路径类别）。
6. **`dlopen`/`dlsym` 调用点的源码归属**：2 个 `dlopen` 可由源码唯一对应；8 个 `dlsym` 落在 `dbinder_softbus_client.cpp` 的软总线符号解析簇（源码该文件共 11 处 `dlsym(`，未逐地址做 DWARF 回溯确认调用点计数差异）。

## 8. 阻塞判定

- **`blocking_findings = 0`。** 未发现「削弱权限判定」「可劫持能力表」「可提权」的路径：关闭态入口符号既未导出也无动态重定位，装载路径整段未编入；两形态校验函数、失败语义、并发语义完全一致；开关不进入运行期，无运行期可利用面。
- SC-01（Medium）是本次唯一需要治理层决策的项：它不改变「谁能提供能力表」，因此不构成安全阻塞；但它确实让「关开关 = 回到拆分前」在**导出面**上不成立，建议在合入前把结论落到 `design.md` 风险表/AC-4.1 说明，或在 TASK-6 用条件化版本脚本收口。SC-02～SC-04 均为 Low，可随 TASK-5/TASK-6 处理。
- 本席位复核所依赖的构建证据为作者保留的两次 `EXIT=0` 日志与冻结产物；由于本席位不执行构建，若后续 `config.gni` 默认值或 `ipc_single/BUILD.gn` 分支被再次修改，需重新核验第 5 节第 1～12 行。