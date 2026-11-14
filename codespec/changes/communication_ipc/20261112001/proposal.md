---
# target_release: 发布版本号（如 7.0，可选 7.0-Beta）；不填分支名(master/dev)
target_release: "7.1"
# change_type: 变更类型（new-feature/enhancement/optimization/bugfix/refactor/deprecation）
change_type: "optimization"
# lite/standard/critical；旧提案缺省 standard。见有效契约，不以降级规避门禁。
delivery_level: "standard"
proposal_id: "20261112001"
slug: "split-rpc-so"
author: "huawei"
date: "2026-11-12"
status: Draft
# sessions: 从 .sessions.json sidecar 镜像（逗号分隔），由各 skill 调用时自动增量追加（去重）；勿手动编辑
sessions: "ses_e30c4a9c3ffeXW2oydHcNICAXE"
---

## 会话记录

<!-- 从 .sessions.json sidecar 镜像（由 skill 自动填充，勿手动编辑） -->
| Session ID | Skill | 时间(UTC) | 操作 |
|------------|-------|----------|------|
| ses_e30c4a9c3ffeXW2oydHcNICAXE | odk-propose | 2026-11-12T11:52:18Z | 生成 |

# Proposal

## 背景与问题

`foundation/communication/ipc` 当前把设备内 Binder IPC 与跨设备 RPC（DBinder/Databus/SoftBus）编译进同一个共享库 `libipc_single.z.so`。

现状事实（x86_64_virt 基线构建产物）：

- 安装到 `platformsdk` 的 `libipc_single.z.so` 剥离后为 492,056 B，未剥离 `.text` 为 289,951 B，动态导出符号 167 个。
- 6 个 DBinder 编译单元（`databus_socket_listener.cpp`、`dbinder_callback_stub.cpp`、`dbinder_databus_invoker.cpp`、`dbinder_session_object.cpp`、`dbinder_softbus_client.cpp`、`dbinder_softbus_client_death_recipient.cpp`）合计符号量 107,312 B，占该库符号总量 317,889 B 的 33.8%。
- 大量进程（应用进程、系统服务）只使用设备内 IPC，却因为共用同一个 so，必须把 RPC 实现及其对 SoftBus 客户端的引用一起映射进地址空间，即使从未发起跨设备调用。

问题：RPC 能力与 IPC 能力在二进制层面强耦合，导致"只用 IPC"的模块承担 RPC 的镜像体积（ROM）与常驻内存（RAM）成本，并且 RPC 相关缺陷/依赖（如 SoftBus 客户端）扩散到全部 IPC 使用方。

本变更把 RPC 能力从 `libipc_single.z.so` 中拆出为独立共享库，仅在进程真正使用 RPC 时加载；只使用设备内 IPC 的模块不再承担 RPC 的代码体积与常驻页。

<!-- 不涉及：用户场景与业务触发；理由：纯内部二进制拆分与内存优化，对终端用户与上层业务无可感知行为变化，也不新增业务触发入口。 -->

## 初始分级判断

| 判断项 | 结果 | 依据 |
|--------|------|------|
| 变更类型 | optimization | 主导是降低内存/镜像资源开销，不改变功能行为；次要类型 refactor（内部二进制结构与依赖关系重构） |
| 复杂度 | 复杂 | 涉及 6 个编译单元迁出、核心侧约 39 处调用点改为间接调用、约 118 个内部符号反向导出、按需加载与失败降级、构建与测试目标同步拆分 |
| 涉及仓数量 | 1 | `foundation/communication/ipc` 单仓；仓外仅为运行时消费方，无源码改动 |
| 是否涉及 Public/System API | 否 | 公开头文件签名、错误码、同步/异步语义均不变；仅内部实现、so 边界与构建目标变化 |
| 是否涉及安全/性能关键路径 | 是 | IPC/Binder 为系统关键路径，RPC 跨设备调用跨越信任边界；同时本变更直接以体积/内存指标为目标 |
| 是否跨 SIG | 否 | 不跨 SIG，不改变对外接口契约与子系统边界 |

## 目标

- **G-1:** 只使用设备内 IPC 的进程不再加载 RPC 实现，其 IPC 能力与调用行为保持不变。
- **G-2:** 使用 RPC 的进程功能不回归——跨设备调用、服务死亡通知、会话管理、权限校验与错误码语义与当前一致。
- **G-3:** RPC 能力按需可用：进程首次真正发起 RPC 调用时能力即可用；RPC 实现缺失或加载失败时，调用方得到确定且可诊断的结果，不发生崩溃或静默错误。
- **G-4:** 核心 IPC 库的镜像体积与符号负担显著下降，且下降量可被构建产物直接度量。
- **G-5:** 变更可通过构建开关回退到当前单库行为，便于分阶段落版与问题定位。
- **G-6:** 拆分后的两条链路（仅 IPC、IPC+RPC）均有可执行的自动化验证，且现有 IPC 测试全部保持通过。

## 非目标

- 不改变 `IPCObjectProxy`/`IPCObjectStub`/`MessageParcel`/`IRemoteObject` 等公开头文件的签名、语义与对象布局（不做 ABI 不兼容变更）。
- 不优化 Binder 驱动、线程池、序列化等设备内 IPC 本身的热路径性能。
- 不引入新的 IPC/RPC 协议、不改变设备发现、认证、授权与权限校验逻辑。
- 不重构或优化 `DBinderSoftbusClient` 对 SoftBus 的使用方式。
- 不在本次变更中承诺**真机** RSS/PSS 的具体收益数值。本轮在 `x86_64_virt` 模拟器上给出两类口径：① 单映射副本（推送到 `/data/local/tmp`）下 IPC-only 与 RPC 进程 PSS 增量差 169 KB（见 `evidence/task-7-device/summary.md`）；② 整机镜像内库被常驻进程共享，`smaps_rollup` 的 PSS 按映射数摊分，不能作为收益估算，收益判据改为「仅用 IPC 的进程对 `libipc_rpc` 的映射段数为 0」（见 `evidence/task-8-image/summary.md`）。真机 phone/wearable 仍未测量，列为未验证项并给出方法。
- 不拆分或改造 `libdbinder`、NAPI/ANI/Taihe/Rust 绑定层的对外能力（仅在依赖关系上同步）。

## 1+8 设备差异规格

| 设备/差异项 | 是否存在差异 | 差异说明 |
|-------------|--------------|----------|
| phone | 是 | 本地 IPC 调用方（应用进程与系统服务）占比高，RPC 按需加载的常驻内存收益最直接；分布式协同/迁移等场景才会触发 RPC 实现加载，因此同一设备上存在"已加载/未加载"两种进程状态。 |
| tablet | 是 | 与 phone 共用同一构建开关和加载策略，但内存规格更大、并行系统服务实例更多，单进程收益在整机内存中的占比与 phone 不同，验收权重低于 phone。 |
| pc/2in1 | 是 | 多窗口、多进程并行度高，跨设备协同（投屏/接续）使用路径与 phone 不同，RPC 首次加载时机分布不同；需关注首次 RPC 调用的时延可接受性。 |
| wearable | 是 | 内存与镜像空间最紧张，so 体积下降的收益权重最高；RPC 使用场景少，多数进程应长期处于"未加载 RPC"状态。 |
| tv | 是 | 常驻系统服务数量少、RPC 使用集中于投屏与分布式场景，收益主要体现为镜像体积（ROM）而非多进程常驻内存。 |
| car | 是 | 多屏多服务、跨设备（车机-手机/穿戴）RPC 使用频繁，需重点验证按需加载后首次调用时延与 RPC 功能完整性。 |
| default（其他设备） | 是 | 未单列设备沿用同一构建开关与加载策略，按该产品实际 RPC 使用比例获得不同的体积/内存收益；不单独引入设备特有分支。 |
| 功能差异（非品类划分） | 是 | 差异按"是否使用 RPC"划分而非设备品类：仅使用本地 IPC 的模块不加载 RPC 实现，使用 RPC 的模块在首次调用时加载；同一设备内两类模块行为不同。 |

## Agent Scope Guard

| 维度 | 范围/限制 | 需人工确认的触发条件 |
|------|-----------|----------------------|
| 允许仓库 | 仅 `foundation/communication/ipc` | 需要改动任何其他仓库（含 `chromium_arkweb`、SoftBus、SAMgr）时必须先确认 |
| 允许模块/目录 | `ipc/native/src/core/dbinder/**`、`ipc/native/src/core/framework/**`、`ipc/native/src/core/invoker/**`、`interfaces/innerkits/ipc_single/**`、`interfaces/innerkits/libdbinder/**`、`interfaces/innerkits/ipc_core/**`（仅构建与私有头）、`config.gni`、`utils/**`、`test/**`、`codespec/**` | 超出上述目录、或需要修改 `interfaces/innerkits/ipc_core/include/` 下公开头文件签名时 |
| 禁止修改项 | 公开头文件签名与错误码语义；`ipc_object_proxy.h`/`ipc_object_stub.h` 的 ABI 布局（虚函数与成员布局）；权限校验与认证判定逻辑；SoftBus 协议交互；`ipc_feature_*` 既有开关语义 | 任何确需不兼容变更的场景，需先取得确认并给出兼容方案 |
| 外部依赖/网络访问 | 不新增第三方依赖，不访问网络；仅使用仓内既有 `dlopen` 惯例与系统既有 so（`libsoftbus_client.z.so`） | 需要新增外部 so 依赖、新增内核 ioctl、或引入网络访问时 |

## 成功标准

| 标准 | 可观察指标 | 验证方式 |
|------|-----------|---------|
| 仅用 IPC 的进程不再映射 RPC 实现 | 该进程 `/proc/<pid>/maps` 中不出现 RPC 共享库；核心库的动态依赖列表中不包含 RPC 共享库 | 构建产物 `readelf -d` 检查 + 设备侧进程 maps 检查 |
| RPC 使用方能力不回归 | 现有 RPC/分布式相关用例与新增 RPC 用例 100% 通过，无新增失败用例 | `ipc_unittest`/RPC 测试目标执行结果对比基线 |
| 首次 RPC 调用可用 | 进程在未预加载场景下发起首个跨设备调用可正常完成，返回结果与当前版本一致 | 自动化用例覆盖"冷启动后首次 RPC 调用" |
| 失败可诊断且不崩溃 | RPC 实现不可用时，调用返回既有错误码，进程不崩溃，日志可定位原因且不刷屏 | 负向自动化用例 + 日志检查 |
| 核心库体积下降 | 剥离后核心库体积相对基线下降 ≥ 10%，`.text` 下降 ≥ 20% | 构建产物 `ls -l` 与 `readelf -S` 基线对比 |
| 仅用 IPC 的进程获得体积/内存收益（整机镜像总量不作为判据） | 核心库 stripped 体积与 ALLOC 下降，且不再映射 RPC 共享库；整机镜像因新增 RPC 库可能净增，属预期权衡而非失败 | 构建产物 `llvm-size`/ALLOC 对比 + 设备侧进程 maps 检查 |
| 本地 IPC 路径无额外开销 | 非 RPC 路径不引入按次加载/符号解析动作，IPC 用例时延不高于基线噪声范围 | 代码审查 + IPC 用例执行结果 |
| 可回退 | 关闭构建开关后产物结构与当前版本一致，全部用例通过 | 开关关闭态构建 + 用例执行 |

## 影响范围

| 子系统 | 仓库 | 模块/路径 | 影响类型 |
|--------|------|-----------|---------|
| foundation/communication | communication_ipc | `ipc/native/src/core/dbinder/source/*.cpp`（6 个文件） | 迁出到独立 RPC 共享库 |
| foundation/communication | communication_ipc | `ipc/native/src/core/framework/source/ipc_object_stub.cpp`、`ipc_object_proxy.cpp`、`ipc_process_skeleton.cpp`、`message_parcel.cpp` | RPC 调用点改为经能力表间接调用；数据结构以不透明句柄持有 |
| foundation/communication | communication_ipc | `ipc/native/src/core/framework/include/ipc_process_skeleton.h` 等私有头 | 成员类型与声明调整（非公开 ABI） |
| foundation/communication | communication_ipc | `interfaces/innerkits/ipc_single/BUILD.gn`、`libipc_single_map` | 源文件集合、导出符号表、新增 RPC 库目标 |
| foundation/communication | communication_ipc | `interfaces/innerkits/ipc_core/BUILD.gn`、`interfaces/innerkits/libdbinder/BUILD.gn` | 产物装配与依赖关系 |
| foundation/communication | communication_ipc | `config.gni` | 新增拆分构建开关 |
| foundation/communication | communication_ipc | `test/unittest/**`（含 `ipc_single_test` 拆分与新增 RPC 测试目标） | 测试目标与用例同步调整 |
| foundation/communication | communication_ipc | 全部 IPC 使用方（仓外应用/系统服务） | 仅运行时加载关系变化，无源码改动、无重新编译要求 |

## 外部依赖

| 子系统 | 仓库 | 模块/路径 | 依赖类型 |
|--------|------|-----------|---------|
| 不涉及 | 不涉及 | 不涉及 | 无新增跨仓交付依赖；仓外消费方只需使用新版本产物，无需源码改动或重新编译。运行时仍沿用既有 `libsoftbus_client.z.so`（当前 RPC 路径已依赖），不新增外部依赖。 |

## 假设与开放问题

### 假设

- 假设 RPC 的可执行入口全部收敛在本仓 6 个 DBinder 编译单元与已识别的约 39 处核心调用点内，仓外模块不直接调用这些内部实现符号；该假设需在实现前用全仓符号引用扫描复核。
- 假设进程内按需加载共享库在目标产品形态上可用（仓内 `ipc_trace.cpp`、`ipc_thread_pool.cpp` 已有同类先例）。
- 假设在 cross-DSO CFI 构建约束下，跨库能力表使用 C ABI 与 POD 类型即可满足类型安全要求，无需放宽编译选项。
- 假设 RPC 实现加载失败属于可降级场景：调用方已有错误返回路径可承接"能力不可用"，无需新增对外错误码。
- 假设拆分后核心库与 RPC 库的初始化顺序不影响代理/桩对象的既有生命周期语义。

### 开放问题

- 仓外是否存在直接引用 `DBinderSoftbusClient::*` 或 `DBinderDatabusInvoker` 构造等当前导出符号的模块？若有，需要评估这些符号迁出后的兼容策略。
- 7.1 落版时拆分开关的默认值与灰度策略（默认开启、按产品覆盖、还是先默认关闭）需与版本与产品团队确认。
- 真机 RSS/PSS 收益需要在目标产品（优先 phone 与 wearable）上测量；本轮已在 QEMU `x86_64_virt` 模拟器上完成进程级 PSS 验证（单映射副本差值 169 KB ≈ rpc 库体积），并确认整机镜像口径下 PSS 不可作收益估算（共享页摊分），改用「映射段数为 0」判据；模拟器不等价于目标产品的系统服务进程常驻量。
- 首次 RPC 调用引入的一次性加载开销是否需要在特定低时延场景做预热，需结合真机时延数据决定。

## 资源开销审视

| 维度 | 状态 | 信号/依据 | 确认人/理由/范围 |
|------|------|-----------|-----------------|
| 性能 | waived | RPC 首次调用引入一次性的库加载与符号解析开销；本地 IPC 热路径未新增调用点（AC-5.1：核心库 `dlopen`/`dlsym` 未定义引用与基线持平，源码级唯一新增调用点 `ipc_rpc_loader.cpp`，且只在首次 RPC 使用时触发） | 确认人=需求方；理由=一次性首次调用开销已由 AC-5.1 证据界定，热路径无回归，用户接受不再单独做时延基准测量；范围=RPC 首次调用路径与设备内 IPC 热路径 |
| 内存(RAM) | waived | 本变更直接目标是降低仅用 IPC 进程的常驻内存；已验证 ALLOC 下降 24.29%（467,177 → 353,720 B），RPC 进程 +10.66%；模拟器单映射副本 PSS 增量差 169 KB（IPC-only +662 KB / RPC +831 KB），整机镜像内 IPC-only 对 rpc 库映射段数为 0（共享页摊分致 PSS 不作收益估算，见 `evidence/task-8-image/summary.md`）；真机 RSS/PSS 未测量 | 确认人=需求方；理由=经用户确认以体积/符号/加载依赖证据作为内存验收口径，并在 `x86_64_virt` 模拟器补进程级 PSS（见 `evidence/task-7-device/summary.md`）；真机 RSS/PSS 与真实系统服务进程常驻量列未验证项，设备可用时补测；范围=仅用 IPC 的典型应用进程与系统服务进程 |
| 存储(ROM) | waived | 核心库体积下降、新增一个 RPC 共享库；本地实测拆后整机 IPC 相关产物净 +56,832 B（+11.55%），收益对象是「不加载 RPC 的进程」而非镜像总量 | 确认人=需求方；理由=经用户确认接受镜像总量 +11.55% 的权衡（IPC-only 进程受益约 24%，RPC 用户进程约 +11%），收益判据以进程级映射量为准；范围=system/lib64 与 platformsdk 下的 IPC 相关产物 |

## 不涉及项确认

| 维度 | 是否涉及 | 依据 |
|------|---------|------|
| 性能 | 否 | 资源开销审视「性能」为 `waived`：契约语义下 waived 不计入 8 维「是否涉及」；实际影响仅为 RPC 首次调用的一次性加载开销，热路径未新增调用点，证据见 AC-5.1 |
| 安全/权限 | 是 | 变更触及 IPC/Binder 与跨设备通信边界（跨信任边界），RPC 实现变为按需加载；但不改变权限校验、认证与授权判定逻辑，需在设计中确认加载失败与降级不绕过任何校验 |
| 兼容性 | 是 | 共享库依赖关系与导出符号集合发生变化；必须保证公开接口、行为语义、错误码以及现有调用方二进制兼容 |
| API/SDK | 否 | 与开发者确认：不新增、不修改、不废弃任何 `.d.ts`/`.h` 声明；公开头文件签名不变，仅内部实现与构建产物结构变化 |
| IPC/跨进程 | 是 | 变更主体即 IPC 库的二进制结构与 RPC 跨进程/跨设备调用路径 |
| 构建/组件 | 是 | 新增共享库目标、调整源文件集合与导出符号表、新增构建开关，并同步测试目标 |
| 国际化 | 否 | 不涉及任何数据的表达、展示、解析或交互，不产生面向用户的语言/地区相关文案或格式 |
| 数据迁移 | 否 | 不涉及持久化数据、数据库 schema 或落盘格式变更，无升级/回滚数据迁移需求 |

## API 设计属性

### API 范围声明

| 属性 | 值 | 说明 |
|------|-----|------|
| **涉及 Kit** | 不涉及 | 本次为仓内二进制拆分与构建调整，不新增或修改任何 Kit 对外接口 |
| **API 类型** | 不涉及 | 无 public/system API 变更 |
| **编程语言** | 不涉及 | 不新增 ArkTS 或 C 声明 |
| **ArkTS 静态类型 API** | 不涉及 | 不涉及 ArkTS API |

不涉及理由：变更为内部实现与共享库边界重构，公开头文件签名、错误码与调用语义保持不变；跨库协作通过仓内私有接口完成，不进入 SDK。