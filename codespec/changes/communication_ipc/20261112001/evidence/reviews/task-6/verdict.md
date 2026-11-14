---
task_id: TASK-6
risk: R0
mode: self-review
independent: false
verdict: PASS
blocking_findings: 0
---

# Task Review Verdict — TASK-6

## 审查方式

| 项 | 值 |
|----|----|
| 风险等级 | R0（仅证据汇总、构建与运行验证，不含产品代码或构建配置变更；`summarize.py` 与 `evidence/task-6/*` 为本 Task 交付物） |
| 评审模式 | **self-review**：未派三席独立 subagent。原因：R0 按 `delivery-levels`/review 指引不要求三席；本 Task 的输入是前五个 Task 已三席 PASS 的代码与配置，改动面只有测量脚本和证据文件 |
| 自审独立性声明 | 结论由实现作者复核，**不等价于独立评审**。AC-1.2 / AC-5.1 的关键数值均以脚本重跑 + 原始工具输出（`baseline.txt`/`after.txt`/`deps.txt`/`symbols.txt`）为据，读者可复现；若治理要求独立复核，需另派三席 |
| 复核轮次 | 1 轮；无阻塞项，无代码修订；仅文档数值一致性回填（T6-05） |
| Diff 范围 | `codespec/changes/ipc/draft-20261112-split-rpc-so/evidence/task-6/*`（新增脚本、原始输出、冻结件、`summary.md`）；`execution-plan.md` TASK-6 行与体积口径；`proposal.md` ROM 行；`design.md` 风险表（TASK-3/4/5 已落盘，本轮只复核） |
| 复现命令 | `python3 codespec/changes/ipc/draft-20261112-split-rpc-so/evidence/task-6/summarize.py`（仓库根执行） |
| 原始输出 | `/tmp/opencode/task6_summarize_run.log`（`EXIT=0`）、`/tmp/opencode/task6_fuzz_rebuild.log`、`/tmp/opencode/task6_fuzz2_rebuild.log`（均 `EXIT=0`） |

## 五维结论

| 维度 | 结论 | 依据 | 阻塞发现 |
|------|------|------|----------|
| ① 重复造轮子 | Pass | 未新增测量框架；`summarize.py` 只调用 `stat`/`llvm-size`/`llvm-readelf`/`llvm-nm`，与仓内既有验证脚本口径一致；证据以冻结件 + 可复现脚本交付，而非截图或手抄数字 | 0 |
| ② 性能/资源 | Pass | AC-1.2 两项阈值均 PASS（文件 −24.48% ≥ 10%、`.text` −28.75% ≥ 20%）；同时给出 IPC-only 与 RPC 进程两套 ALLOC 口径，未以镜像总量替代收益判据；fuzz 体积上升单独归因为测试件静态聚合 | 0 |
| ③ 漏改/完整性 | Pass（带 2 项非阻塞观察） | TASK-1~5 的 AC 追溯全部有证据；`packages/` 实盘安装、运行期 RSS/PSS、真实 `dlopen` 场景在 `summary.md` 第 11 节显式列为未验证，未包装成已通过 | 0 |
| ④ 外部接口影响 | Pass | 导出面变化量化到条（核心库 167→235、rpc 11、关闭态 245）；公开头文件签名、NAPI/C API/Rust/Taihe/ANI 未变；`bundle.json`、transaction code、错误码、`ioctl` 无新增 | 0 |
| ⑤ 安全/攻击面 | Pass | AC-5.1 源码级唯一新增 `dlopen` 调用点为 `ipc_rpc_loader.cpp`；核心库 `dlopen`/`dlsym` 未定义引用与基线持平（2/2）；失败路径为 fail-closed（既有错误码、单条日志、不重试）；`R2-03`（先 `dlopen` 后校验）等固有边界已登记风险表 | 0 |

## 阻塞发现

无。`blocking_findings: 0`。TASK-6 不引入产品代码/构建配置变更，前序 Task 的阻塞项均已在各自 verdict 闭环或登记。

## 证据复核

### 1. 四口径体积（复跑一致）

| 对象 | 文件 (B) | `.text` (B) | ALLOC (B) | LOAD MemSiz (B) |
|------|---------:|------------:|----------:|----------------:|
| baseline | 492,056 | 289,951 | 467,177 | 467,892 |
| on-single | 371,584 | 206,579 | 353,720 | 354,444 |
| on-rpc | 177,304 | 92,045 | 163,256 | 163,980 |
| off-single | 488,464 | 282,181 | 468,467 | 469,180 |

- 阈值判定：文件 −120,472 B（−24.48%）≥ 10% PASS；`.text` −83,372 B（−28.75%）≥ 20% PASS。
- 脚本重跑 `EXIT=0`，四张原始输出与 `summary.md` 数字逐项一致；冻结件 md5 `d2ee5266…`（基线）与 `f4f1df60…`（关闭态）核对一致。
- on-rpc 采用 U-1 修正后产物（`.text` 92,045 / ALLOC 163,256），旧记 90,717 / 161,464 已在 `summary.md` 标注作废，无残留引用。

### 2. 依赖方向

`deps.txt`：`libipc_single` NEEDED **不含** `libipc_rpc`；`libipc_rpc` NEEDED 含 `libipc_single`；`libdbinder` NEEDED 含 `libipc_rpc`。链为 `libdbinder → libipc_rpc → libipc_single`（+ `libipc_rpc → libipc_common`），无环。

### 3. 入口与导出面

`libipc_rpc.z.so` 定义 `IpcRpcGetOps` 1 条（`@@1.0`）；`libipc_single.z.so` 与关闭态均为 0 条；`libipc_single_test.z.so` 为 1 条（测试形态，无版本脚本，符合 TASK-5 SC-01 登记）。开启态导出 235、rpc 11、关闭态 245，增量来源已在 `design.md` 风险表与 `config.gni` 注释登记为已接受取舍。

### 4. 加载点

核心库 `dlopen`/`dlsym` 动态未定义条数 baseline/on-single/off-single 均为 2/2，与基线持平；源码级核查确认唯一新增调用点为 `ipc_rpc_loader.cpp`，其余为既有 `ipc_trace.cpp`、`ipc_thread_pool.cpp`。`IpcRpcOpsGet()` 为 `std::call_once` 惰性单次，未进入 RPC 路径的进程不触发。

### 5. Fuzz 影响

两个 fuzz 目标改为静态聚合后均重建 `EXIT=0`：596,216 → 725,456 B、596,240 → 725,536 B（约 +21.7%）。已在 `summary.md` 说明测试件不参与设备内存核算，结论仅限「可构建性未被破坏」。

### 6. 安装声明

`ipc_rpc_core_module_info.json` 声明 `system/lib64/platformsdk/` 与 `updater/lib64/platformsdk/`，`install_enable = true`，与 `ipc_core` 同目录同机制。`packages/` 实盘未产出（`make_packages` 环境阻塞），证据边界已写明。

## 非阻塞观察（本轮登记）

| 编号 | 摘要 | 处置 |
|------|------|------|
| T6-01 | `test/unittest/common/BUILD.gn` 的 `ipc_unittest` 模板 defines 未复用 `ipc_rpc_abi_defines`，测试 so 与生产库的宏集合可能漂移（**既有问题**，非本变更引入） | 登记为后续清理；本变更已把生产侧统一到 `ipc_rpc_abi_defines`，测试 so 走 `ipc_single_test` 自身 defines |
| T6-02 | `ipc_single_test` 复用 `ipc_rpc_abi_defines` 后新增 `ENABLE_IPC_PROXY_DFX_BACKTRACE`（此前该宏只在生产库）——**计划外变更** | 接受：行为与生产库更一致，仅影响测试件；已登记，不改变产品产物 |
| T6-03 | `libipc_single_map` 在拆分态仍列 `DBinderSoftbusClient::*` 等模式；符号不存在时 lld 静默忽略，靠成功构建兜底 | 接受：同一 map 需同时服务开启/关闭两种形态，删除条目会破坏关闭态；已由 TASK-6 构建通过间接验证 |
| T6-04 | `ipc_object_stub.cpp::CreateSessionName` 用 `ERR_NONE` 与 `IPC_RPC_SOFTBUS_SUCCESS` 比较（两者均为 0） | 接受为既有风格，不改；常量耦合已由唯一真源 `ipc_rpc_ops.h` 收敛 |
| T6-05 | `execution-plan.md` 问题闭环 B9 行仍写 `+52,696 B / +10.71%`，与 `proposal.md` 现值 `+56,832 B / +11.55%` 不一致 | **本轮已修复**：B9 与 R8 标注「TASK-3 时点值」并补 U-1 后最终值 |

## 未验证项（与 `summary.md` 第 11 节一致）

| 项 | 原因 | 后续 |
|----|------|------|
| RSS/PSS 真机测量 | 本机无 `hdc` 设备、无 `qemu-system-x86_64` | 设备可用时按 `proposal.md` 方法复测 |
| 真实 `dlopen`/`dlsym` 加载 `libipc_rpc.z.so` 的运行期场景 | 只有产物级证据 | 设备/模拟器可用时补用例 |
| 关闭开关态「字面运行」 | 需切开关全量重建 | 可选补强 |
| `packages/` 实盘安装 | `make_packages` 被 preheat CMakeCache 路径不一致打断 | 清理 out 后重建镜像阶段校验 |
| arm64 链接选项一致性（A4） | 本机仅 `x86_64_virt` | arm64 构建时核对 |
| 端到端死亡通知/权限链路运行期 | 需设备 | 设备可用时执行 |

## 代码与规格一致性结论

- AC-1.1、AC-1.2、AC-2.1、AC-2.2、AC-2.3、AC-3.1、AC-3.2、AC-3.3、AC-4.1、AC-5.1、AC-6.1 在 `execution-plan.md` 追溯表均为单值 `Pass`，逐条对应到上表证据或已登记的环境阻塞口径。
- **表述约束：** 批次结论为「6 个目标二进制全绿 + 仓内 59 个二进制除环境豁免项外通过」；**不得**表述为「四种形态均已运行期验证」，也不得把 `packages/` 实盘安装计为已验证。
- 合入约束不变：TASK-2/3/4/5 必须同批合入；TASK-6 仅为证据与文档。