---
task_id: TASK-4
risk: R2
independent: true
verdict: PASS
blocking_findings: 0
---

# Task Review Verdict — TASK-4

## 审查方式

| 项 | 值 |
|----|----|
| 宿主与委派 | host = opencode；`can_delegate = true`、`max_parallel = 3`；按固定三席并行派发 `subagent(agent=general)`，三席均为只读，只允许写各自证据文件 |
| 评审席位 | arch（① ②）、scope（③ ④）、security（⑤） |
| 评审轮次 | 三席各 1 轮（无阻塞项，无返工轮次），评审后由作者按发现清单做定向修订并重建 |
| Diff 范围 | `config.gni`、`BUILD.gn`、`interfaces/innerkits/ipc_core/BUILD.gn`、`interfaces/innerkits/ipc_single/BUILD.gn`、`interfaces/innerkits/libdbinder/BUILD.gn`、`interfaces/innerkits/ipc_single/libipc_single_map`、`ipc_rpc_loader.cpp`、`ipc_rpc_ops.h`、`ipc_rpc_loader.h` |
| 证据文件 | `evidence/reviews/task-4/arch.md`、`scope.md`、`security.md` |
| 构建证据 | 开启态 `/tmp/opencode/task4_green4_open.log`（`EXIT=0`）；关闭态 `/tmp/opencode/task4_green5_off.log`（`EXIT=0`）；关闭态冻结产物 `/tmp/opencode/split_off_libipc_single.z.so`、`split_off_libdbinder.z.so`、`split_off_exports.txt`；开启态快照 `task4_green4_enabled.txt`、关闭态快照 `task4_green5_disabled.txt` |

评审基于作者交付的原始代码与原始命令输出，未采信作者结论。三席各自独立读取源码、`git show HEAD:` 基线、冻结产物与 `llvm-nm/readelf/size/objdump` 输出；因评审期间 `out/x86_64_virt` 正被作者重建为默认开启态，三席统一改用冻结件，未执行 `./build.sh`。TASK-1/2/3 产物只作上下文。

## 五维结论

| 维度 | 评审者 | 结论 | 证据文件 | 阻塞发现 |
|------|--------|------|----------|----------|
| ① 重复造轮子 | arch | Pass（带 1 项 Medium 观察） | `arch.md` | 0 |
| ② 性能/资源 | arch | Pass（带 4 项 Low 观察） | `arch.md` | 0 |
| ③ 漏改/完整性 | scope | Pass（带 2 项 Medium 观察） | `scope.md` | 0 |
| ④ 外部接口影响 | scope | Pass（带 2 项 Low 观察） | `scope.md` | 0 |
| ⑤ 安全/攻击面 | security | Pass（带 1 项 Medium、3 项 Low 观察） | `security.md` | 0 |

## 阻塞发现

无。三席均无 Critical / High，`blocking_findings: 0`。

关键判定（三席独立复核一致）：

| 判定项 | 结论 | 依据 |
|--------|------|------|
| 开关是否真的生效 | 成立 | 开启态产出 `libipc_rpc.z.so`（173,160 B）且 `libdbinder` NEEDED 含它；关闭态 `find out -name 'libipc_rpc*'` 命中 0，`libdbinder` NEEDED 无它，`strings libipc_single.z.so \| grep -c libipc_rpc` = **0**，开启态同口径 = **1** |
| 关闭态是否靠 `#ifdef` 删实现（D-06） | **不违反** | 关闭态 `.gnu_debugdata` 重建后可见 `IpcRpcGetOps`、`RPC_OPS`，证明 4 个 RPC TU + entry TU 与 loader 在同一 so；被排除的只有 `dlopen`/路径解析/映像校验这段**加载机制**，功能实现零删除 |
| 关闭态依赖结构是否回退 | 成立 | 关闭态核心库 12 条 `DT_NEEDED` 与开启态**同序同名**、与拆分前基线逐条一致；`libdbinder` 10→9 条 |
| 关闭态是否拒绝外部依赖 | 成立 | `libdbinder` 关闭态 8 个强未定义 `DBinderSoftbusClient::*` 符号在核心库导出中 **8/8 命中、0 缺失** |
| 删除 `ipc_rpc` 目标名是否破坏下游 | 不破坏 | 全仓及 `code/` 根检索 `ipc_single:ipc_rpc` / `ipc_core:ipc_rpc_core` 的外部引用 **0 命中** |
| 安全校验链是否两形态等价 | 成立 | `CheckCapabilityTable()` 在静态分支（`ipc_rpc_loader.cpp:277`）与动态分支（`:334`）均先于任何 ops 使用执行，失败即 `ZLOGE` + `g_ops.store(nullptr)`，无降级放行；关闭态 `IpcRpcGetOps` 不导出（`local: *` 收口），同进程无法绕过 loader 取表 |

## 非阻塞发现与处置

| 编号 | 席别 | 严重度 | 摘要 | 处置 |
|------|------|--------|------|------|
| A-01 | arch | Medium | `defines` 清单在 `ipc_single`/`ipc_rpc`/`ipc_single_test` 三个 target 内手抄三份；`FREEZE_PROCESS_ENABLED` 会改 `ipc_types.h` 枚举与 `ipc_skeleton.h` 声明，漏改会造成跨 DSO 静默 ODR/枚举错位，而 `abiVersion` 覆盖不到 | **本轮已修复**：新增 `config("ipc_rpc_abi_defines")` 作为唯一真源，三个 target 统一 `configs +=`；顺带补齐 `ipc_single_test` 原先缺失的 `ENABLE_IPC_PROXY_DFX_BACKTRACE`（默认 `false`，本轮行为不变）。`design.md` 风险表已登记 |
| A-02 | arch | Medium | AC-4.1 要求「关闭开关后全部用例通过」，但 TASK-4 验证项全是构建/产物检查；`ipc_single_test` 未定义 `IPC_RPC_STATIC_LINK`、未编入 entry TU | 由 **TASK-5** 按 D-07 补齐并回填关闭态单测原始输出；`verdict.md` 在 TASK-5 关闭前不得引用 AC-4.1 为已满足 |
| A-03 / S-01 / SC-01 | arch+scope+security | Medium | 关闭态导出面 245 vs 拆分前 167（+78），`BR-6` 在导出面上不成立；代价 `.plt` +1,200 B、`JUMP_SLOT` +75、ALLOC +1,290 B | **接受并登记**：两形态共用同一张 `libipc_single_map`，复制第二份 190 行 map 的漂移风险更高；无符号丢失、无 `DT_NEEDED` 变化、无行为变化。已写入 `design.md` 风险表与 `config.gni` 注释，TASK-6 体积表单列「关闭态 vs 拆分前」并注明来源 |
| A-04 | arch | Low | 关闭态并非指令级回退：每次 RPC 入口多一次 `call_once` 快路径 + acquire load + 经表间接调用 | **文档已修正**：`ipc_rpc_loader.h` 增加 Build shapes 段，明确关闭态同样经能力表间接调用；TASK-6 列为已知变量 |
| A-05 | arch | Low | plan 目标句「不 `dlopen`」按字面不成立（仓内既有 `InitIpcSo`、SoftBus client 仍有 `dlopen`） | **文档已修正**：目标句改为「loader 不再执行 `dlopen`，仓内既有 `dlopen` 不属本开关范围」 |
| A-06 | arch | Low | `IsOpsComplete()` 第二次枚举能力表字段，与结构体定义构成两份真源 | 遗留（非阻塞）：属 TASK-1/3 范围，本轮不返工；TASK-6 已知变量登记，后续以 X-macro / 字段掩码收敛 |
| A-07 | arch | Low | 拆分前基线 `.so` 未留存，−3,592 / −7,770 / +1,290 只能引用作者一次性测量 | TASK-6 必须先把基线产物与 `llvm-size -A` 原始输出冻结进 evidence 目录 |
| A-08 | arch | Low | `ipc_rpc_loader.h` 注释称首次调用触发 load、并给出无出处的 2.7 ns | **本轮已修复**：改为 Build shapes 说明，删掉具体延迟数值，改为定性表述 |
| A-09 | arch | Low | 两形态失败处理 4 行重复 | 接受现状：重复量小且日志上下文不同；若再加第三种形态再抽 `PublishFailure()` |
| S-02 | scope | Medium | 关闭态测试装配缺 entry TU 与 `IPC_RPC_STATIC_LINK`；若门禁要求 TASK-4 独立满足 AC-4.1 则升级为阻塞 | 由 **TASK-5** 承接；已登记批次约束（TASK-2/3/4/5 同批合入） |
| S-03 | scope | Low | `ipc_rpc_loader.h` 的测试钩子声明用 `#ifdef IPC_RPC_UNIT_TEST`，定义用 `#if defined(...) && !defined(IPC_RPC_STATIC_LINK)`；TASK-5 若同时定义两宏会「有声明无定义」 | **本轮已修复**：头文件 guard 同步为 `#if defined(IPC_RPC_UNIT_TEST) && !defined(IPC_RPC_STATIC_LINK)`，并加注释说明必须与 `.cpp` 一致 |
| S-04 | scope | Low | `make_packages` 被 preheat 遗留 CMakeCache 阻塞，`packages/` 实盘安装未验证 | TASK-6 重取；在此之前不宣称「已安装」 |
| S-05 | scope | Low | 两个私有头位于 `src/core/dbinder/include/`，经 `ipc_util_config` 对下游可 include | 保留：不在 `bundle.json` 的 `header_base`、不安装到 SDK、关闭态不导出、开启态仅导出 1 条；本 Task 不扩大改动面 |
| SC-02 | security | Low | `config.gni` 注释称关闭即「pre-split shape」，会让运维误以为攻击面完全回退 | **本轮已修复**：注释补充「只回退链接结构，不回退导出面；既有裸名 `dlopen` 不随开关回收」 |
| SC-03 | security | Low | 关闭态仍有既有裸名 `dlopen("libsoftbus_client.z.so")` 与线程池自重开 | 非本次引入；`design.md` 风险表登记为残余风险，后续按 loader 同款「绝对路径 + 映像校验」加固 |
| SC-04 | security | Low | 关闭态 `LoadOnce()` 静态分支无运行期用例 | 由 **TASK-5** 增加以 `IPC_RPC_STATIC_LINK` 编译的 loader 用例，覆盖「表不完整 → 1 条日志 + `Peek()==nullptr` + 第二次调用不再取表」 |

## 评审后修订说明

评审结论为 PASS、无阻塞；作者按上表对 **A-01、A-04、A-05、A-08、S-03、SC-02** 做了定向修订，改动性质：

| 文件 | 变更 | 功能性 |
|------|------|--------|
| `interfaces/innerkits/ipc_single/BUILD.gn` | 新增 `config("ipc_rpc_abi_defines")`；三个 target 由手抄 `defines` 改为 `configs +=` | 是。展开后的宏集合与原 `ipc_single`/`ipc_rpc` 完全一致；`ipc_single_test` 补上默认关闭的 `ENABLE_IPC_PROXY_DFX_BACKTRACE`。已重建验证 |
| `ipc/native/src/core/dbinder/include/ipc_rpc_loader.h` | 测试钩子 guard 同步；主注释改写为两种构建形态 | 否（guard 只在同时定义 `IPC_RPC_UNIT_TEST`+`IPC_RPC_STATIC_LINK` 时变化，当前无目标这样编译） |
| `config.gni` | 注释补充回退范围 | 否 |
| `design.md` | 风险表新增 2 行（导出面未回退、跨 DSO 宏漂移） | 否 |
| `execution-plan.md` | TASK-4 目标句修正（A-05） | 否 |

重建证据见 `execution-plan.md` TASK-4 的 Actual Result「评审后修订」段与 `/tmp/opencode/task4_postfix_build.log`。

## 批次约束

- TASK-2 / TASK-3 / TASK-4 / TASK-5 **必须同批合入**：TASK-4 单独合入时两种构建都自洽，但测试装配尚未跟上（`ipc_single_test` 仍按动态形态编译），13 个既有单测在开启态失败。
- `AC-4.1` 的「关闭态全部用例通过」证据由 TASK-5 提供；在 TASK-5 关闭前，本文件不宣称 AC-4.1 已满足。
- 运行期/设备验证（`AC-1.1`、`AC-2.1`、`AC-2.2`、`AC-2.3`、`AC-5.1` 的执行面）已由 TASK-7/TASK-8/TASK-9 在 x86_64_virt 与 arm64_virt 模拟器上补齐，全部 Pass。