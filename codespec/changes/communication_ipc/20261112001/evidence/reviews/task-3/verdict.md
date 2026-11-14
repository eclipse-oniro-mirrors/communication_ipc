---
task_id: TASK-3
risk: R2
independent: true
verdict: PASS
blocking_findings: 0
---

# Task Review Verdict — TASK-3

## 审查方式

| 项 | 值 |
|----|----|
| 宿主与委派 | host = opencode；`can_delegate = true`、`max_parallel = 3`；按固定三席并行派发 `subagent(agent=general)`，三席均为只读，只允许写各自证据文件 |
| 评审席位 | arch（① ②）、scope（③ ④）、security（⑤） |
| 评审轮次 | scope 2 轮、security 2 轮、arch 3 轮 |
| Diff 范围 | 未提交工作区：`ipc/native/src/core/dbinder/{include,source}/ipc_rpc_ops.h`、`ipc_rpc_loader.*`、`ipc_rpc_entry.cpp`、`ipc_single/libipc_{rpc,single}_map`、4 个构建侧 `BUILD.gn` + 3 个测试/fuzz `BUILD.gn`（详见 `execution-plan.md` 代码范围映射 TASK-3 段） |
| 证据文件 | `evidence/reviews/task-3/arch.md`、`scope.md`、`security.md` |

评审基于作者交付的原始代码与原始命令输出，未采信作者结论；三席各自独立读取源码、`git show HEAD:` 基线、`out/x86_64_virt` 产物与 `llvm-nm/readelf/size` 输出，并各自实跑 `IpcRpcLoaderUnitTest`。TASK-1/2 产物只作上下文。

## 五维结论

| 维度 | 评审者 | 结论 | 证据文件 | 阻塞发现 |
|------|--------|------|----------|----------|
| ① 重复造轮子 | arch | Pass | `arch.md` | 0 |
| ② 性能/资源 | arch | Pass（资源净增按 Medium 记录，非阻塞） | `arch.md` | 0 |
| ③ 漏改/完整性 | scope | Pass（第 2 轮） | `scope.md` | 0 |
| ④ 外部接口影响 | scope | Pass（第 2 轮） | `scope.md` | 0 |
| ⑤ 安全/攻击面 | security | Pass（第 2 轮） | `security.md` | 0 |

## 阻塞发现

| 编号 | 严重度 | 维度 | File:Line | 问题 | 状态 | 复验依据 |
|------|--------|------|-----------|------|------|----------|
| B-1 | High（已解除） | ③ 漏改 | `test/fuzztest/ipc/native/core/{dbindergrantpermission,getlocalnodedeviceid}_fuzzer/BUILD.gn:31` | 两个 fuzz 目标仍依赖 `:ipc_single` 并直接调用已迁出的 `DBinderSoftbusClient::GetInstance()`，链接必然失败；计划中「其依赖 `ipc_single_test`，无需改动」的前提与事实相反 | **Resolved** | 两目标 deps 改为 `$SUBSYSTEM_DIR/test:ipc_single_test_static`；链接行输入无 `libipc_single.z.so`；静态库含 4 个迁移 TU + loader；两个 `FuzzTest` 二进制 `llvm-nm -u -C` 的 `DBinderSoftbusClient` 计数为 **0**，构建两次 `EXIT=0` |
| S-01 | High（已解除） | ⑤ 加载路径 | `ipc_rpc_loader.cpp` | 初版用裸名 `dlopen("libipc_rpc.z.so")`，可被 `LD_LIBRARY_PATH`/搜索顺序中的同名库满足 | **Resolved** | 改为 `dladdr(自身 marker)` → own-dir 绝对路径 → `dlopen` → `dlsym` 入口 → 对入口符号 `dladdr` 校验同一文件（串相等或 `st_dev/st_ino` 相等）；own-path 必须绝对，否则 fail-closed；security 第 2 轮在源码 + 反汇编 + 实跑上独立复核通过 |

无未闭环 Critical/High，`blocking_findings: 0`。

## 非阻塞发现与处置

| 编号 | 严重度 | 维度 | 内容 | 处置 |
|------|--------|------|------|------|
| M-1 | Medium | ④ | `ipc_rpc_core` 的 `innerapi_tags` 用直连档 `platformsdk`，把内部伴随库登记为平台 SDK 直连接口 | **已修复**：改 `platformsdk_indirect`，注释与源目标对齐；`ipc_rpc_core_module_info.json` 实测 |
| M-2 | Medium | ③ | `libipc_rpc_map` 与 `libipc_single_map` 的 `DBinderSoftbusClient` 条数不等（9 vs 8），且 single 表 8 条已悬空 | **已闭环**：实际导出按 `libdbinder` 强未定义符号收敛为 11 条（入口 + 8 方法 + 构造 C1/C2，删 `DBinderRemovePermission`）；`libipc_single_map` 的 8 条悬空条目留 TASK-4 关闭态处置 |
| M-3 | Medium | ④ | 裸名 `dlopen` 依赖不在 x86_64_virt 生效的 `ld-musl-*.path`，本产品族缺直接证据 | **已闭环**：加载改为 own-dir 绝对路径，不再依赖 loader 搜索路径；运行期仍归 TASK-6 |
| A2 | Medium | ② | 交付件按「体积净收益」表述，实际整机镜像净增；收益只在 IPC-only 进程 | **已修复**：Actual Result 与 `proposal.md` 改为「净增 + IPC-only 收益」双口径，并补「整机镜像总量不作为判据」的成功标准 |
| S-02 | Medium（维持） | ⑤ | 核心库导出面 167 → 235、74 条私有映射；auth 表改写入口、`SetPeerUid/Pid`、`MessageParcel::GetRawData/RestoreRawData` 仍在导出面；两库无 `-Bsymbolic` | **登记 + 转 TASK-6**：`ipc_rpc_ops.h` 私有 ABI 登记承接治理缺口，逐符号收敛与 `-Bsymbolic` 评估转 TASK-6，并列入 TASK-6「已知变量」 |
| R2 | Low→已闭环 | ⑤ | 「同一文件两种拼写」需通过，否则 `DT_NEEDED` 先装载的进程永久判失败 | **已修复**：串相等直接通过，否则 `stat()` 比较 `st_dev/st_ino`；同名异文件仍拒绝 |
| R5 | Low→已闭环 | ⑤ | 初版建议用 `dlinfo(RTLD_DI_LINKMAP)`，但本仓 musl 定义 `HANDLE_RANDOMIZATION`，`dlinfo` 恒失败 | **已修复**：改用 `dladdr(入口符号)`；arch 用 `musl_template.gni:545` + `dynlink_rand.c` + `dlinfo.c` + 探针 3/3 次实测确认方向正确，`<link.h>` 依赖移除 |
| R4 | Low→已闭环 | ⑤ | own-path 解析与校验需可在不 `dlopen` 的情况下被测试 | **已修复**：新增 5 个仅测试可见 API；3 个用例不装 hook 直调生产实现 |
| R1 | Low→已闭环 | ④ | 两库是否装到同一目录 | **已闭环（声明级）**：`dest` 完全一致，`install_enable=true`；实盘安装受环境阻塞，见未验证项 |
| A1 | Low→已闭环 | ③ | 新增私有导出缺治理登记 | **已修复**：`ipc_rpc_ops.h` 新增「Registered private ABI」段（跨 DSO 符号组、非受支持 API、必须同构建同目录、混镜像属打包错误、`1.0` 单节点、拒绝节点隔离理由、不支持 ADLT） |
| R2-01 | Low（加固） | ⑤ | own-path 未断言绝对性，相对路径会让「绝对」`dlopen` 路径随 CWD 漂移 | **已修复**：`DirectoryOfLibraryPath()` 只接受 `/` 开头；新增 `RelativeOwnLibraryPathIsRejected`；`OwnLibraryDirectoryIsResolvedWithoutHooks` 按 own-path 形态断言两分支 |
| R2-02 | Low | ⑤ | ADLT 形态下 `dli_fname` 为伪名，`stat` 必失败 → RPC 恒不可用 | **登记**：私有 ABI 段写明「不支持 ADLT」，表现为既有错误码 + 单条日志 |
| R2-03 | Low | ⑤ | 校验在 `dlopen` 之后，被拒映像静态构造已执行且 `RTLD_NODELETE` 不卸载 | **接受**：dlopen-then-verify 固有边界，需能写 trusted 目录才成立；TASK-6 记入已知变量 |
| R2-04 / R7 | Low | ⑤③ | 「同名异文件」负例只用不存在文件，仅覆盖 `stat` 失败分支 | **已修复**：新增 `ExistingDifferentFileIsRejected`，用真实 `/proc/self/maps` 覆盖「两侧 `stat` 成功、inode 不同」；真实 `dlopen` 临时目录副本转 TASK-5 |
| S-03 | Low | ⑤ | `ipc_rpc_entry.cpp` 无校验 `static_cast` | **接受**：当前唯一注册实现，等价 `dynamic_cast`；转 TASK-6 记录 |
| S-04 | Low | ⑤ | `dbinder_session_object.cpp` 在 `Peek()` 为空时仍置 `SOCKET_ID_INVALID` | **转 TASK-4**：与 `Peek()` fd 泄漏一并处置 |
| S-05 | Low | ⑤ | 两库无 `-Bsymbolic` | **转 TASK-6**：与 S-02 一并评估 |
| S-06 | Low | ⑤ | 空表分支日志可被对端反复触发 | **已登记**：仅首次加载失败记一条，调用方失败日志由既有路径承担；TASK-6 复核 |
| R6 | Low | ③ | 私有 ABI 段「neither an innerapi nor an SDK surface」与 tags/节点事实不符 | **已修复**：改为「exported but not a supported API」 |
| R8 | Low | ② | TASK-3 验证表与 Actual Result 仍用早期构建数字；「ALLOC」实为 `Total − .gnu_debugdata` | **已修复**：全部换为 round8/9 快照，写明 ALLOC 定义并附 LOAD `MemSiz`；四口径完整表统一归 TASK-6 |
| R9 | Low | ③ | 存在「源码 16 例 / 二进制 15 例」的时间窗 | **已修复**：采集前断言 mtime（loader 源码 < 单测源码 < 测试二进制） |
| A3 / A4 / A7 | Low | ②③ | 74 条导出致核心库 29 处 PLT 化；`ipc_rpc` 未继承 arm64 专用链接选项；`common_source` 命名与「测试全量集合」语义不符 | **转 TASK-6**：已写入 TASK-6 任务书「已知变量」段（只测量/注明，不在本变更内优化） |
| A6 | Low | ③ | `ipc_rpc_entry.cpp` 注释称 function-local static，实为 namespace 作用域 `const` | **已修复**：注释改为「namespace-scope `const` + 常量初始化」 |
| A8 | Low | ③ | `design.md` 类图签名与 `ipc_rpc_ops.h` 漂移 | **已修复**：逐项对齐（`int32_t`/引用出参/`IRemoteInvoker*`），loader 改为自由函数 `IpcRpcOpsGet/Peek` |
| B9 | Low | ② | `proposal.md` ROM 行残留 +52,656 / +10.70% | **已修复**：改为 +52,696 / +10.71% |
| L-1 / L-3 | Low | ③ | 代码范围映射漏 `libdbinder/BUILD.gn`；「32 个 TU」「修改 4 个 BUILD.gn」计数不符 | **已修复**：补齐映射并标注偏离；计数改为「基线 30 项 → `core_source` 27 项」「共 7 个 `BUILD.gn`（构建侧 4 + 测试/fuzz 侧 3）」 |
| L-4 | Low | ③ | `packages/`、`system.zip`、`images/system.img` 为拆分前陈旧副本 | **已修复**：验证表与证据缺口明确标注「不作验收依据」 |
| L-5 | Low | ④ | `interfaces/innerkits/ipc_core/libipc_core_map` 为无构建引用的遗留死文件 | **接受**：不参与构建，不在本变更扩大改动面 |
| N-1 | Low | ④ | `libipc_rpc_map` 导出 `DBinderDatabusInvoker` C1/C2，但无 DSO 引用，导出面由 9 增至 11 | **维持**：`InvokerDelegator` 在 TU 内实例化需要该定义；与 TASK-4 关闭态清单一起复核 |
| L-2 | Low | ③ | `.gitignore` 新增 `codespec/` 等 4 行，评审证据不会随变更提交 | **转交付说明**：属流程工具产物，由 TASK-6/PR 描述单列，不随功能提交 |

## 复验记录

评审后按非阻塞项修改并重新构建/实跑（均为原始输出）：

| 检查项 | 命令 | 结果 |
|--------|------|------|
| 组件 + 测试目标构建 | `./build.sh --product-name x86_64_virt --ccache --deps-guard=false --build-target ...:IpcRpcLoaderUnitTest foundation/communication/ipc:ipc_components` | `EXIT=0`（`/tmp/opencode/task3_fix_round9.log`） |
| loader 单测（相对 argv） | `ld-musl-x86_64.so.1 + LD_LIBRARY_PATH`，相对路径启动 | `[  PASSED  ] 16 tests` |
| loader 单测（绝对 argv） | 同上，绝对路径启动 | `[  PASSED  ] 16 tests` |
| 产物时效 | `ls -l` 比较源码与二进制 mtime | loader 源码 < 单测源码 < 测试二进制（R9 关闭） |
| rpc 导出 | `llvm-nm -D --defined-only libipc_rpc.z.so` | 11 条：`IpcRpcGetOps@@1.0` + 8 `DBinderSoftbusClient` + C1/C2 |
| 单向依赖 | `llvm-readelf -d` 双库 NEEDED | rpc 含 `libipc_single.z.so`；single 含 `libipc_rpc` 计数 0 |
| 未定义符号 | `check_rpc_symbols_resolved.sh` | `RESULT: PASS (211 undefined, all covered by NEEDED closure)` |
| 安装声明 | `ipc_rpc_core_module_info.json` | `dest = system/updater platformsdk`、`innerapi_tags = ["platformsdk_indirect"]`、`install_enable=true` |
| 体积 | `ls -l` / `llvm-size -A` | single 371,584 B（-120,472 / -24.48%）、`.text` 206,579（-83,372 / -28.75%）、ALLOC 353,720（-113,457 / -24.29%）；rpc 173,168、`.text` 90,717、ALLOC 161,464；合计 file +52,696（+10.71%）、`.text` +7,345（+2.53%）、ALLOC +48,007（+10.28%）、LOAD `MemSiz` +48,740（+10.42%） |

## 未验证项（不作为通过依据）

- `packages/phone/{system,updater}/lib64/platformsdk/libipc_rpc.z.so` 实盘安装：`make_packages` 被 preheat 遗留 CMakeCache 路径不一致阻塞；`packages/`/`system.zip`/`images/system.img` 为拆分前陈旧副本。
- 真机/模拟器首次 RPC 链路（无 `hdc`、无 `qemu-system-x86_64`）：AC-1.1/2.1/2.2/2.3/3.1/5.1 的执行面。
- arm64 链接选项继承（A4）与 arm64 目标产物复核。
- SELinux 标签继承、ADLT 容器场景。

## 门禁结论

- [x] PASS — 无未闭环 Critical/High，`blocking_findings: 0`
- [ ] FAIL — 存在未闭环 Critical/High
- [ ] BLOCKED — 阻塞原因未解除

TASK-3 可标记 Done。**合入约束不变**：TASK-2 与 TASK-3 必须同批合入（中间态 RPC 能力恒不可用）；TASK-3 与 TASK-4 亦须同批（开关关闭态未就绪前，关闭开关不可用）。转出项已写入 TASK-4/TASK-5/TASK-6 任务书与 `design.md` 风险表，本 Task 不留 Open 项。