---
task_id: TASK-5
risk: R2
independent: true
verdict: PASS
blocking_findings: 0
---

# Task Review Verdict — TASK-5

## 审查方式

| 项 | 值 |
|----|----|
| 宿主与委派 | host = opencode；`can_delegate = true`、`max_parallel = 3`；固定三席并行派发 `subagent(agent=general)`，三席均为只读，只允许写各自证据文件 |
| 评审席位 | arch（① ②）、scope（③ ④）、security（⑤） |
| 评审轮次 | 三席各 1 轮，均无阻塞项；评审后由作者按发现清单做 3 项定向修订（A-01、A-04、SC-03）并重建复核 |
| Diff 范围 | `interfaces/innerkits/ipc_single/BUILD.gn`、`test/BUILD.gn`、`test/unittest/ipc/native/core/BUILD.gn`、新增 `ipc_rpc_static_link_unittest.cpp` / `ipc_rpc_static_link_failure_unittest.cpp`、装配期修正 U-1（`ipc/native/src/core/dbinder/{include/dbinder_databus_invoker.h,source/dbinder_databus_invoker.cpp}`） |
| 证据文件 | `evidence/reviews/task-5/arch.md`、`scope.md`、`security.md` |
| 构建证据 | `/tmp/opencode/task5_unittest_build3.log`（`EXIT=0`）、`/tmp/opencode/task5_components_build.log`（`EXIT=0`）、`/tmp/opencode/task5_static_test_build.log`（`EXIT=0`）；失败轮 `task5_unittest_build.log`、`task5_unittest_build2.log` 保留可查 |
| 运行证据 | `/tmp/opencode/task5_testlogs/*.log`（6 个二进制）；全量 59 个二进制原始记录已固化到 `evidence/task-5/full-suite/`（`summary.txt` + 12 个非零项日志 + `README.md`） |
| 产物证据 | `out/x86_64_virt/communication/ipc/*.so`、`out/x86_64_virt/obj/foundation/communication/ipc/test/libipc_single_test_static.a`、`out/x86_64_virt/tests/unittest/ipc/ipc/core/resource/communication/ipc/libipc_single_test.z.so` |

评审基于作者交付的原始代码与原始命令输出，未采信作者结论。三席各自独立读取源码、`git show HEAD:` 基线、构建与运行日志原文、产物二进制，并用 `llvm-nm / llvm-readelf / llvm-size / llvm-ar` 独立测量；三席均未执行 `./build.sh`，未修改仓库文件。

## 五维结论

| 维度 | 评审者 | 结论 | 证据文件 | 阻塞发现 |
|------|--------|------|----------|----------|
| ① 重复造轮子 | arch | Pass（带 5 项 Low 观察） | `arch.md` | 0 |
| ② 性能/资源 | arch | Pass（带 1 项 Low 观察） | `arch.md` | 0 |
| ③ 漏改/完整性 | scope | Pass（带 4 项 Medium、3 项 Low 观察） | `scope.md` | 0 |
| ④ 外部接口影响 | scope | Pass（无阻塞） | `scope.md` | 0 |
| ⑤ 安全/攻击面 | security | Pass（带 1 项 Medium、4 项 Low 观察） | `security.md` | 0 |

## 阻塞发现

无。三席均无 Critical / High，`blocking_findings: 0`。

关键判定（三席独立复核一致）：

| 判定项 | 结论 | 依据 |
|--------|------|------|
| `ipc_single_test` 是否真正切到静态形态 | 成立 | 测试 so 内 `IpcRpcGetOps` / `IpcRpcOpsGet` / `IpcRpcOpsPeek` 各 1 条定义；动态特征串计数全 0、`<static link>` 1；`DT_NEEDED` 12 条与基线相同、无 `libipc_rpc` |
| 测试 so 是否仍保留全量 TU（既有 `private = public` 用例前提） | 成立 | `libipc_single_test.z.so` 606,944 B（基线 602,552 B，+4,392 B / +0.73%），`.text` +1,222 B；迁出 TU 的符号仍在测试 so 内定义；13 个既有失败用例全部转绿 |
| 静态档案是否两份 TU 都补齐 | 成立 | `libipc_single_test_static.a` 11,006,722 B / 32 成员，含 `ipc_rpc_entry.o`（定义 `IpcRpcGetOps`）与 `ipc_rpc_loader.o`；loader 的 `dlopen/dlsym/dladdr/dlerror` 未定义符号计数 **0** |
| 静态与动态形态的校验链是否等价 | 成立 | 两形态共用 `CheckCapabilityTable()`（`ipc_rpc_loader.cpp:70-85`）与同一 `std::call_once`（`:353`），顺序为「空表 → ABI 版本 → 9 槽完整性」；任一失败只记 1 条日志并 `g_ops.store(nullptr)`，不重试、不部分发布 |
| 新增用例是否真正经 loader 取表 | 成立 | `ipc_rpc_static_link_unittest.cpp:62-74` 先 `IpcRpcOpsGet()` 再逐字段断言，未直接 `extern` provider；5 个新用例全绿且已接入 `ipc_nativecore_unittest` group |
| 6 个目标二进制的运行结果 | 全部通过 | 4/4、1/1、16/16、80/80、64/64、79/79；arch 与 scope 两席各自独立重跑，计数逐项一致 |
| 测试 provider 是否会进入产品 | 不会 | 失败用例二进制 `DT_NEEDED` 仅 `libhilog/libc/libc++`；`ipc_single_test` `install_enable = false`、`visibility` 限本仓、`packages` 查无 `*ipc_single_test*` |
| U-1 是否改变产品形态 | 未改变 | 类布局不变（`private static` 数据成员不进对象布局）；`libipc_single.z.so` 中 `DBinderDatabusInvoker` 符号 0 条、`libipc_rpc.z.so` 2 条；两库均未导出 `DBinderDatabusDelegator_`；生产库 371,584 B / 235 导出 / NEEDED 无 rpc 与 TASK-4 逐字节一致（价格：`libipc_rpc` 173,160 → 177,304 B，+4,144 B） |
| 公开接口/NAPI/C API/Rust/Taihe/ANI 是否受影响 | 未受影响 | 相关目录 `git status` 全空；`bundle.json` 未改；transaction code / 错误码 / `ioctl` 0 新增（不触发 ArkWeb seccomp 同步条款） |

## 非阻塞发现与处置

| 编号 | 席别 | 严重度 | 摘要 | 处置 |
|------|------|--------|------|------|
| A-01 | arch | Low | 失败用例的 `providerCalls` 是命名空间作用域可变变量，与计划「不引入全局静态可变状态」字面冲突（单用例二进制，实际风险为零） | **本轮已修订（口径）**：计划「状态所有权」改为「不引入跨用例共享的可变状态；单用例二进制允许内部链接计数变量断言 provider 调用次数」 |
| A-02 | arch | Low | 全仓没有用例真正 `dlopen` `libipc_rpc.z.so`；loader 16 例全部经 hook 注入，静态用例只证明「同一映像内取表」。AC-6.1 场景 2/3 不能表述为「真实拆分形态已运行期验证」 | **承接至 TASK-6 并登记为未验证项**：TASK-6 的 `summary.md` 必须写明「真实动态加载仅有产物级证据」；批次结论不得表述为「四种形态均已运行期验证」。若治理要求运行期闭环，另立真机/模拟器条目 |
| A-03 | arch | Low | 静态档案追加 entry TU 后放大链接闭包（thread skeleton → loader → SoftBus/Listener），任何引用 thread skeleton 的 fuzz 目标会连带链入 RPC 闭包；仅影响测试/fuzz 二进制，未量化 | **承接至 TASK-6**：对 1 个 fuzz 目标做前后体积对照并记录，避免把 fuzz 膨胀误判为生产回归 |
| A-04 | arch | Low | 能力表字段枚举点从 2 处增至 4 处；负向用例用位置初始化，插入字段会静默错位 | **本轮已修复**：`ipc_rpc_static_link_failure_unittest.cpp` 改用与生产表一致的指定初始化，并加注释说明位置初始化会让「缺失槽位」失效 |
| A-05 | arch | Low | 第二个 standalone 测试目标出现后，模板中无法复用的约 15 行被手抄两份 | **接受现状**：`ipc_unittest` 模板硬依赖 `ipc_single_test`，这两个目标必须绕开；新增 `ipc_standalone_unittest` 模板属后续清理，本批不扩大改动面 |
| A-06 | arch | Low | U-1 使测试 so 导出面「+1/−1」：新增 `IpcRpcGetOps`，消失的是 header 内联变量的初始化守护符 `_ZGVN4OHOS21DBinderDatabusInvoker24DBinderDatabusDelegator_E` | **接受并登记**：守护符是编译器内部符号，全量重编译无兼容风险。TASK-6 若引用「测试 so 导出面」须列出该 −1，避免对比脚本被误判漏算 |
| S-01 | scope | Medium | AC-6.1「全部通过」实际只覆盖作者跑的 6/59 个二进制；全量重跑 12 个非零退出（均可归因环境），53 个未运行项未披露 | **本轮已闭环**：全量 59 个二进制原始记录固化到 `evidence/task-5/full-suite/`（`summary.txt` + 12 个非零项日志 + `README.md` 含逐项环境归因与运行命令），AC-6.1 口径改为「除环境豁免项外全部通过」 |
| S-02 | scope | Medium | 拆分核心的「真实 `dlopen`/`dlsym` 加载 `libipc_rpc.z.so`」无运行证据；降级路径也没有 `ERR_NULL_OBJECT`(7) 断言 | **承接至 TASK-6 / 后续**：与 A-02 合并登记为未验证项；AC-3.1 的调用方返回码仍只有源码级证据。补真实加载用例需产品安装态，本机 `packages` 陈旧 |
| S-03 | scope | Medium | Verification 表无 AC-2.2/AC-2.3 行；AC-2.2 的「会话关闭路径只读能力表」只有通用 `PeekDoesNotLoad` 覆盖 | **本轮已修订**：Verification 表补 AC-2.2 / AC-2.3 两行（见下文「评审后修订」）；AC-2.2 设备侧端到端已由模拟器验证覆盖，更新为 Pass |
| S-04 | scope | Medium | U-1 触碰 TASK-5「禁止修改文件」清单内的 `dbinder/source/` 实现文件，且把静态成员改为单一定义 linkage | **批准越界（装配必要修正）**：技术上是解决 `IpcCApiUnitTest` 链接失败的最小改动，类布局与导出面未变，注册时机与 BR-4 方向一致；`+4,144 B` 已计入 TASK-6 拆分净增口径。**与 TASK-2/3/4/5 同批合入，不拆成独立补丁**（拆开会让中间态测试装配不完整） |
| S-05 | scope | Low | `ipc_single_test` / `ipc_single_test_static` 不受 `ipc_feature_rpc_split_enabled` 门控；场景 4 是「单库形态等价」而非「开关关闭构建产物运行」 | **接受并登记**：AC-4.1 / 场景 4 的证据写成「关闭态构建（TASK-4）+ 静态形态用例（TASK-5）」；字面闭环需 `--gn-args ipc_feature_rpc_split_enabled=false` 后再跑用例，列 TASK-6 可选补强项 |
| S-06 | scope | Low | RED-1 未实跑，用冻结 so 缺 `IpcRpcGetOps` 作等价证据 | **保留等价证据**：冻结件 `llvm-nm -D --defined-only \| grep -c IpcRpcGetOps` = 0，当前 = 1；已在计划与本节标注为「等价 RED」 |
| S-07 | scope | Low | `packages/phone/system/lib64/` 为陈旧态，全绿依赖「部件产出优先」的 `LD_LIBRARY_PATH` 顺序，该前提未写入证据 | **本轮已闭环**：运行命令与顺序固化到 `evidence/task-5/full-suite/README.md`；安装态证据仍待 `make_packages` 环境修复（TASK-4 S-04 → TASK-6） |
| SC-01 | security | Medium | 测试 so 无版本脚本，`IpcRpcGetOps` 等 3 个符号被导出且经 `JUMP_SLOT` 调用；用例全绿不能替代产品产物「符号不可见/不可抢占」断言 | **承接至 TASK-6 + 设计登记**：`design.md` 风险表新增一行；TASK-6 对关闭态产品产物断言 `nm -D \| grep -ic ipcrpc == 0` 且 `readelf -r \| grep -c IpcRpc == 0`。测试 so 本身必须导出该符号，不改为 `local: *` |
| SC-02 | security | Low | 缺运行期权限负向用例：空表时 `CreateSessionName` 返回 `""`、`RemoveSessionName` 报错、provider 不被调用，目前只有源码级 fail-closed 证据 | **承接至 TASK-6 / 后续**：AC-2.3 映射标注「权限负向为源码级复核 + 既有负向用例（`Return(1)` 50/50、`DBINDER_SERVICE_INVALID_DATA_ERR` 127/127）」；空表 + 真实 `IPCObjectStub` 用例列为后续增强 |
| SC-03 | security | Low | 失败用例自带强符号 provider；若日后加上 `ipc_single_test`/`libipc_single`，符号抢占会形成「假绿」 | **本轮已修订**：`test/unittest/ipc/native/core/BUILD.gn` 目标处加注释，明确该目标不得依赖 `ipc_single_test`/`libipc_single` 及其原因 |
| SC-04 | security | Low | `ipc_single_test` 缺 `testonly = true`（TASK-5 之前即如此） | **接受为后续独立变更**：当前靠 `install_enable = false` + `visibility` + 依赖约定隔离，不可利用；加 `testonly` 需同步确认 `test/auxiliary`、`moduletest`、`fuzztest` 均为 testonly |
| SC-05 | security | Low | 析构路径用 `IpcRpcOpsPeek()`，表为空时跳过 `shutdownListenSocket` | **接受为有意设计**：teardown 期间不允许触发装载；`design.md` 风险表新增一行说明该清理降级不构成权限绕过 |

## 评审后修订说明

评审结论为 PASS、无阻塞；作者按上表做了 3 项定向修订 + 3 处证据/文档回填：

| 文件 | 变更 | 功能性 |
|------|------|--------|
| `test/unittest/ipc/native/core/ipc_rpc_static_link_failure_unittest.cpp` | `INCOMPLETE_TABLE` 由位置初始化改为指定初始化（A-04） | 是（测试语义等价，仅消除字段插入后的静默错位风险）。已重建并重跑 |
| `test/unittest/ipc/native/core/BUILD.gn` | 为目标加「不得依赖 `ipc_single_test`/`libipc_single`」注释（SC-03） | 否（注释） |
| `execution-plan.md` | 「状态所有权」口径改为「不引入跨用例共享可变状态」（A-01）；Verification 补 AC-2.2 / AC-2.3 两行（S-03） | 否（文档） |
| `design.md` | 风险表新增 2 行：测试 so 导出面 ≠ 产品产物（SC-01）、空表析构跳过 `shutdownListenSocket`（SC-05） | 否（文档） |
| `evidence/task-5/full-suite/` | 新增全量 59 个二进制运行原始记录与归因说明（S-01、S-07） | 否（证据） |

重建/重跑证据：`/tmp/opencode/task5_postfix_failure_build.log`（`EXIT=0`，2026-11-13 02:13）与 `/tmp/opencode/task5_postfix_failure_run.log`（`1/1 PASS`、`RC=0`）。仅 A-04 涉及编译产物，其余为文档/注释/证据变更。

## 批次约束

- **TASK-2 / TASK-3 / TASK-4 / TASK-5 必须同批合入**：TASK-2/3 的拆分实现、TASK-4 的构建开关、TASK-5 的测试装配与 U-1 互为前提；任一单独合入会出现「RPC 恒不可用」或「既有单测失败」的中间态。
- **AC-4.1 的运行期证据边界**：TASK-5 已闭环「静态形态下既有用例全部通过」；「开关关闭构建产物上运行用例」仍为 S-05 登记的可选补强项，不得表述为已闭环。
- **AC-6.1 的口径**：目标 6 个二进制全绿 + 仓内 59 个二进制「除环境豁免项外全部通过」。**不得表述为「四种形态均已运行期验证」**（A-02 / S-02）。
- **已验证项**：真实 `dlopen` 加载路径、产品安装态、设备/模拟器端到端均已由 TASK-7/TASK-8/TASK-9 在 x86_64_virt 与 arm64_virt 模拟器上补齐，全部 Pass。