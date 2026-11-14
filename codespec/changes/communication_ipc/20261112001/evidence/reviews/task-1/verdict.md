---
task_id: TASK-1
risk: R2
independent: true
verdict: PASS
blocking_findings: 0
---

# Task Review Verdict — TASK-1

## 审查方式

| 项 | 值 |
|----|----|
| 宿主与委派 | host = opencode；`can_delegate = true`、`max_parallel = 3`；按固定三席并行派发 `subagent(agent=general)`，三席均为只读，只允许写各自证据文件 |
| 评审席位 | arch（① ②）、scope（③ ④）、security（⑤） |
| Diff 范围 | 未提交工作区：新增 `ipc/native/src/core/dbinder/include/ipc_rpc_ops.h`、`ipc_rpc_loader.h`、`ipc/native/src/core/dbinder/source/ipc_rpc_loader.cpp`、`test/unittest/ipc/native/core/ipc_rpc_loader_unittest.cpp`；修改 `interfaces/innerkits/ipc_single/BUILD.gn`、`test/unittest/ipc/native/core/BUILD.gn` |
| 证据文件 | `evidence/reviews/task-1/arch.md`、`scope.md`、`security.md` |

评审基于作者交付的原始代码与原始命令输出，未采信作者结论；三席各自独立读取文件、构建产物与 `llvm-nm/readelf` 输出。

## 五维结论

| 维度 | 评审者 | 结论 | 证据文件 | 阻塞发现 |
|------|--------|------|----------|----------|
| ① 重复造轮子 | arch | Pass | `arch.md` | 0 |
| ② 性能 | arch | Pass | `arch.md` | 0 |
| ③ 漏改/完整性 | scope | Pass | `scope.md` | 0 |
| ④ 外部接口影响 | scope | Pass | `scope.md` | 0 |
| ⑤ 安全/攻击面 | security | Pass | `security.md` | 0 |

## 阻塞发现

| 编号 | 严重度 | 维度 | File:Line | 问题 | 状态（Open/Resolved） | 复验依据 |
|------|--------|------|-----------|------|------------------------|----------|
| — | — | — | — | 无 Critical/High 发现 | — | — |

## 非阻塞建议

| 编号 | 严重度 | 维度 | 建议 | 处置 |
|------|--------|------|------|------|
| A-01 | Medium | ① | `DBINDER_*` 常量在 `ipc_rpc_ops.h` 与 `databus_socket_listener.h` 双份定义，TASK-2 引入 include 关系后必须同提交删除旧定义 | 转入 TASK-2：实现步骤要求同提交删除 `databus_socket_listener.h` 中三个常量，并在 TASK-2 验证表增加「`DBINDER_PKG_NAME = ` 全仓仅 1 处」守卫 |
| A-02 | Medium | ① | `IpcRpcOps` 的 4 个 `int32_t` 返回字段语义不同，需逐字段注释 | **已修复**：`ipc_rpc_ops.h` 为 9 个字段各加一行语义注释，明确 `startServerListener` 返回 socketId、`<=0` 即失败，其余 `0 == IPC_RPC_SOFTBUS_SUCCESS` |
| A-03 | Medium | ① | `dlopen` 失败日志未带 `dlerror()`，而失败被永久缓存，该日志是唯一现场 | **已修复**：新增 `DescribeLoaderError()`，`dlopen`/`dlsym` 失败时附加 `dlerror()` 文本；单测 hook 分支保持稳定文本，避免读到陈旧 dlerror |
| A-04 | Low | ① | 头文件内 `static const std::string` 造成每 TU 动态初始化与 `atexit` 注册 | **已修复**：改为 `inline const std::string`（C++17 inline 变量），保持调用点 `std::string` 语义不变、消除每 TU 副本；`ipc_single` 与单测目标均重新构建通过 |
| A-05 | Low | ② | 契约未约束 `Get()` 不得用于按次路径 | **已修复**：`ipc_rpc_loader.h` 注释明确「仅在首次 RPC 入口调用 `Get()`，按次路径复用已取得的指针」 |
| A-06 | Low | ① | 独立单测目标逐行复制 `ipc_unittest` 模板配置，存在漂移风险 | 采纳现状并在 `test/unittest/ipc/native/core/BUILD.gn:170-171` 注释说明「必须独立、不得链接 `ipc_single_test`」的理由；TASK-5 装配测试时复核该目标与模板配置一致 |
| A-07 | Low | ① | `IPC_RPC_SOFTBUS_SUCCESS` 复述 softbus `SOFTBUS_OK`，需标注来源 | **已修复**：常量上方注释标注来源 `dl_deps/dsoftbus_interface.h` 与同步要求 |
| A-08 | Low | ① | 校验失败分支既不 `dlclose` 也不记录，代码意图与副作用不一致 | **已修复**：`LoadOnce()` 注释明确「句柄故意不释放：`RTLD_NODELETE` 下 `dlclose` 无效，静态初始化已完成，失败仅表示核心库不使用该表」 |
| SEC-01 | Medium | ⑤ | 裸 soname `dlopen` 依赖全局搜索路径，同名库可被优先命中，而表中含授权函数 | 转入 TASK-3：`design.md` 状态不变量新增「加载路径信任」约束，TASK-3 必须选定并记录路径固定或 `dlinfo` 校验方案，否则作为残余风险登记 |
| SEC-02 | Medium | ⑤ | `dlopen` 在 `call_once` 内执行 rpc so 静态构造，若构造回调 `Get()` 即自重入死锁 | **已修复（约束固化）**：`LoadOnce()` 顶部注释写明重入禁止；`design.md` 状态不变量新增同一约束，TASK-3 的 entry TU 顶部需复述并由评审核对 |
| SEC-03 | Medium | ⑤ | ABI/完整度被拒后 rpc so 静态注册已生效，可能形成「invoker 可用但 ops 为空」半加载状态 | **已修复（约束固化）**：`design.md` 状态不变量新增「RPC 能力可用性唯一判据是 `ops != nullptr`」；TASK-2 的 8 个使用项按「先取 ops、判空、再用 invoker」实现与评审 |
| SEC-04 | Low | ⑤ | 失败日志粒度偏粗 / 或有意不打印路径 | **已修复**：与 A-03 合并处理，失败日志附 `dlerror()`；日志仍为单次输出（`IPC_RPC_LOG_LOAD_FAIL` 调用点 1 处），不存在高频刷日志面 |
| SEC-05 | Low | ⑤ | 测试 seam 仅靠 `IPC_RPC_UNIT_TEST` 隔离，缺少构建期守护 | 转入 TASK-4/TASK-5：拆分/装配构建目标时复核生产目标未定义该宏；若 GN 可表达则加 `assert`，否则在 TASK-5 记录人工核对结论 |
| SEC-06 | Low | ⑤ | 头文件 `static const std::string` 的静态初始化面 | **已修复**：与 A-04 合并为 `inline const std::string` |
| SEC-07 | Low | ⑤ | 能力表跨 DSO 传递 STL 类型，`abiVersion` 无法发现 STL ABI 混用 | **已记录**：`design.md` 状态不变量新增「同一次构建/同一工具链/同一 libc++ 配置」ABI 前提，跨镜像混装不在支持范围 |

## 待确认

| 项 | 缺少的事实 | 是否关联 High 风险点 |
|----|------------|----------------------|
| AC-3.1/3.2/3.3 端到端语义 | 本 Task 仅交付 loader；空表到既有错误码的映射、失败不重试的跨调用点行为需 TASK-2 接入后回归 | 否 |
| TASK-3 加载路径固定方案 | 安装目录与 `target_cpu` 映射在 TASK-3 才确定 | 否（已登记为 TASK-3 强制项） |
| `IPC_RPC_STATIC_LINK` 静态回退归属 | 计划中该分支在 TASK-4 落地，TASK-1 无对应代码 | 否 |

## 复验记录

评审后按非阻塞项修改，并重新执行验证（均为原始输出）：

| 检查项 | 命令 | 结果 |
|--------|------|------|
| host harness | `g++ -std=c++17 -DIPC_RPC_UNIT_TEST ... ipc_rpc_loader.cpp harness.cpp && ./t` | `RESULT PASSED (7 scenarios)`，退出码 0 |
| 设备侧单测（9 用例） | `LD_LIBRARY_PATH=<out>/packages/phone/system/lib64:<...>/chipset-sdk-sp <out>/common/common/libc/ld-musl-x86_64.so.1 <out>/tests/unittest/ipc/ipc/core/IpcRpcLoaderUnitTest` | `[  PASSED  ] 9 tests.`，退出码 0；新增 `GetOpsNullResultIsRejected`、`PeekAfterGetReturnsSameOps` 覆盖 scope 席位指出的两个缺口 |
| 单测目标构建 | `./build.sh --product-name x86_64_virt --ccache --deps-guard=false --build-target foundation/communication/ipc/test/unittest/ipc/native/core:IpcRpcLoaderUnitTest` | 退出码 0，`build successful`，产物 296856 B |
| 核心库构建 | `./build.sh --product-name x86_64_virt --ccache --deps-guard=false --build-target foundation/communication/ipc/interfaces/innerkits/ipc_single:ipc_single` | 退出码 0，`build successful` |
| 单次加载与日志唯一 | `grep -c "dlopen(" / "dlsym(" / 日志宏调用点` | `dlopen` 1 处（L94）、`dlsym` 1 处（L111）、日志调用点 1 处（L132），全部位于 `LoadOnce()`（L81-139）；`IpcRpcOpsPeek()`（L147）0 命中 |

## 门禁结论

- [x] PASS — 无未闭环 Critical/High，`blocking_findings: 0`
- [ ] FAIL — 存在未闭环 Critical/High；Task 标记 `❌ Blocked`
- [ ] BLOCKED — 阻塞原因未解除，Task 标记 `❌ Blocked`

TASK-1 可标记 Done。转入下游的 A-01、SEC-01、SEC-05 已在 TASK-2/TASK-3/TASK-4 的计划与 `design.md` 不变量中固化为强制项，不在本 Task 留 Open 项。