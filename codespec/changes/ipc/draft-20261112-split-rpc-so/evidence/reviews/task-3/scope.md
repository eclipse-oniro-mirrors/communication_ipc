---
task_id: TASK-3
risk: R2
seat: scope
verdict: PASS
blocking_findings: 0
---

# Task Review — 漏改/完整性（③）与外部接口影响（④）

## 结论（第 1 轮，保留原记录）

> 第 2 轮复验结论见文末 `## 复验（第 2 轮）`：**verdict PASS，blocking_findings 0**（B-1 已闭环）。

- **verdict: FAIL**，**blocking_findings: 1**（B-1，High）。
- 拆分主体是成立的：4 个 TU 已从 `libipc_single.z.so` 移出并编入 `libipc_rpc.z.so`，单向依赖（`libipc_rpc → libipc_single`，核心库 NEEDED 中 `libipc_rpc` 计数为 0）在产物上成立，公开头文件零改动，仓内 `DBinderSoftbusClient` 的唯一动态消费方 `libdbinder` 已闭环。
- 阻塞原因是**漏改**：2 个 fuzz 目标仍以 `:ipc_single` 共享库为依赖并直接调用 `DBinderSoftbusClient::GetInstance()`，而该符号已不在 `libipc_single.z.so` 的动态导出中，链接必然失败；该面在计划里无人认领（TASK-3 只改 3 个 BUILD.gn、TASK-5 明令禁改 `test/fuzztest/`、TASK-4 不含 `test/`）。
- 另发现 1 项外部接口面的定位偏差（`ipc_rpc_core` 的 `innerapi_tags` 用了 `platformsdk` 直连档，把内部库登记成平台 SDK 接口面）、1 项导出清单不变量与实际不符、以及 5 项低风险证据/登记问题，均不阻塞。

## 维度结论

| 维度 | 结论 | 摘要 |
|------|------|------|
| ③ 漏改/完整性 | **不通过（1 项阻塞）** | 生产侧（`libdbinder`、组件构建、安装声明）无漏改；测试/模糊侧漏改 2 个目标（B-1）。源码集合、导出清单、`local: *` 收口、`--no-undefined` 均与计划一致；导出清单的「同一清单」不变量未满足（M-2）。 |
| ④ 外部接口影响 | 通过（含 1 项 Medium 偏差） | 公开头文件/NAPI/C API/ArkTS 声明零改动；`ioctl` 与 transaction code 零新增；错误码与成功判定未变；`libipc_single` 减少 10 个动态导出、新增 68 个净导出，仓内消费方闭环。偏差在新库的 `innerapi_tags` 定位（M-1）与 x86_64 装载路径证据缺失（M-3）。 |
| 漏改计数 | **1 类（2 个构建目标）** | 生产模块 0 项；测试/模糊模块 1 项。 |

## 独立核验记录

### 命令基线

仓根 = `code/foundation/communication/ipc`；产物目录 = `code/out/x86_64_virt`；工具链 = `code/prebuilts/clang/ohos/linux-x86_64/llvm/bin/{llvm-nm,llvm-readelf}`。基线一律用 `git show HEAD:` 或拆分前产物 `out/x86_64_virt/innerkits/ohos-x86_64/ipc/ipc_core/libipc_single.z.so`（492,056 B @04:30），**未执行任何改写工作区的 git 命令**，未执行 `./build.sh`。

### R-1 源码集合与目标装配

| 检查 | 结论 | 证据 |
|------|------|------|
| 4 个 TU 是否真的离开核心库 | 是 | `interfaces/innerkits/ipc_single/BUILD.gn:49-55`（`rpc_source` 4 项）与 `:56-83`（`core_source` 27 项）；`:122` `ipc_single.sources = core_source` |
| 是否只迁出 4 个 TU | 是，且唯一新增 loader | 脚本比对 `git show HEAD:...BUILD.gn`：基线 `common_source` 30 项；`core_source` = 基线 − {`databus_socket_listener.cpp`,`dbinder_databus_invoker.cpp`,`dbinder_softbus_client.cpp`,`dbinder_softbus_client_death_recipient.cpp`} + `ipc_rpc_loader.cpp`（27）；`rpc_source` = 4；`common_source` = 31（`:88`） |
| rpc 库是否吃 entry + 4 个 TU | 是 | `:186` `sources = [ "$IPC_CORE_ROOT/src/core/dbinder/source/ipc_rpc_entry.cpp" ] + rpc_source`；`:227` `ldflags = [ "-Wl,--no-undefined" ]` |
| 测试变体是否仍保全量 | 是（TASK-5 前态） | `:249` `ipc_single_test.sources = common_source`；`git status --short` 显示 `test/BUILD.gn` 未改，`test/BUILD.gn:104-109` 的 `ipc_single_test_static` 仍显式列出 4 个 TU |
| `ENABLE_IPC_TRACE` 是否遗漏到 rpc 库 | 否（非缺陷） | `config.gni:23` 默认 `ipc_feature_trace_enabled = false`；4 个迁移 TU 内 `grep -c ENABLE_IPC_TRACE` = 0，故 `BUILD.gn:206-210` 只为宏一致性镜像，不影响行为 |

### R-2 导出面与依赖闭包（产物实测）

| 检查 | 结论 | 证据 |
|------|------|------|
| rpc 库导出集合 | 12 个 | `llvm-nm -D --defined-only lib.unstripped/communication/ipc/libipc_rpc.z.so` → `IpcRpcGetOps@@1.0` + 9 个 `DBinderSoftbusClient::*` + `DBinderDatabusInvoker::C1/C2` |
| 核心库反向依赖 | 0 | `llvm-readelf -d libipc_single.z.so \| grep -c ipc_rpc` → **0** |
| rpc 库正向依赖 | 成立 | `llvm-readelf -d libipc_rpc.z.so \| grep NEEDED` → `libipc_single.z.so`、`libipc_common.z.so` |
| `libdbinder` 依赖链 | 成立 | `llvm-readelf -d libdbinder.z.so \| grep NEEDED` → `libipc_common.z.so`、`libipc_rpc.z.so`、`libipc_single.z.so`；`interfaces/innerkits/libdbinder/BUILD.gn:83-90` 的 deps 已加 `ipc_single:ipc_rpc` |
| `DBinderSoftbusClient` 在核心库是否已彻底不可见 | 是 | `llvm-nm -D --defined-only libipc_single.z.so \| grep -ci softbus` → 1，唯一命中 `_ZN4OHOS18IPCProcessSkeleton19RemoveSoftbusServerEv`（无关）；`*DBinderSoftbusClient*` 计数 **0**（基线 8） |
| 核心库导出净变化 | 235 = 167 − 10 + 78 | 基线 167（`innerkits/.../libipc_single.z.so`），现状 235；移除集恰为 8 个 `DBinderSoftbusClient::*` + 2 个 `DBinderDatabusInvoker::C1/C2` |
| `libipc_single_map` 变更 | +74 / −0，`local: *` 保留 | `git diff --numstat` → `76 0`；`git diff` 新增 `"OHOS::` 行 74、删除 0；`local: *` 仍存在 |

### R-3 消费者闭环审计（③ 核心）

审计口径：源码级 `grep -rln` 找到全部引用 4 个迁出类型的 TU（`test/`、`example/`、`dbinder/`、`ipc/` 全域，文件类型不限），再逐个回查其 BUILD.gn 依赖目标；产物级用 `grep -rla --include=*.so` 扫 `out/x86_64_virt`（排除 `obj/`）。

| 消费方 | 依赖目标 | 结论 |
|--------|----------|------|
| `dbinder/dbinder_service/src/dbinder_service.cpp`、`.../socket/dbinder_remote_listener.cpp` | `libdbinder`（已加 `ipc_rpc`） | 安全 |
| 6 个 `*_fuzzer`（`bind`/`dbinderremovepermission`/`dbindersoftbusclient`/`listen`/`sendbytes`/`socket`） | `test:ipc_single_test_static` | 安全（全量 TU 静态库） |
| 10 个 mock/其他 fuzz（`databussocketlistenermock001/002`、`dbinderdatabusinvokermock003/004`、`ipcprocessskeletonmock004`、`ipcobjectproxymock002`、`ipcskeletonmock002`、`dbindersoftbusclientmock001/002` 等） | `ipc_single:ipc_single_test` 或 `ipc_core:ipc_core`+`libdbinder` | 安全（`ipc_single_test` 导出 `DBinderSoftbusClient` 17 处实测；`libdbinder` 经 NEEDED 传递） |
| `test/unittest/{common,dbinder_service,ipc/native/core}`、`test/moduletest/rpc/cppapi`、`test/auxiliary/dl_softbus` | `ipc_single_test` / `ipc_single_test_static` / `libdbinder` | 安全 |
| `test/unittest/common/BUILD.gn:266` 的 `IpcCApiUnitTest` | `ipc_single`（共享） | 安全（源码仅 C API 用例，不引用迁出类型） |
| **`dbindergrantpermission_fuzzer`、`getlocalnodedeviceid_fuzzer`** | **`ipc_single`（共享）** | **断链（B-1）** |

### R-4 ④ 公开接口面

| 检查 | 结论 | 证据 |
|------|------|------|
| 公开头文件 | 零改动 | `git diff --stat -- interfaces/innerkits/ipc_core/include interfaces/innerkits/c_api/include interfaces/innerkits/libdbinder/include interfaces/kits` → 空 |
| `interfaces/` 全部变更 | 4 个文件，190 增 6 删 | `git diff --stat -- interfaces/` → `ipc_core/BUILD.gn`、`ipc_single/BUILD.gn`、`libipc_single_map`、`libdbinder/BUILD.gn`（另有未跟踪的 `libipc_rpc_map`） |
| `ioctl` / transaction code | 0 新增 | `git diff \| grep -c ioctl` → 0；`git diff \| grep -ic "transaction code"` → 0（AGENTS.md 的 ArkWeb seccomp 同步条款未触发） |
| 安装声明 | 同目录、system+updater | `obj/.../ipc_core/ipc_rpc_core_module_info.json` → `dest=["system/lib64/platformsdk/libipc_rpc.z.so","updater/lib64/platformsdk/libipc_rpc.z.so"]`、`install_enable=true`、`innerapi_tags=["platformsdk"]`、`symlink_ext=None` |
| 实盘安装 | 未产出 | `grep -rl DBinderSoftbusClient out/x86_64_virt/packages` → 只有 492,056 B @01:29 的旧 `libipc_single.z.so`（baseline 体积），`packages/.../libipc_rpc.z.so` 不存在（L-4） |
| 明确不构成缺陷 | 未上板 | 按评审约定，「无设备验证」不计缺陷；dlopen 运行期验证归 TASK-6 |

## 代码范围映射核对

`execution-plan.md:687-731` 的 TASK-3 行与工作区实际对比：

| 映射声明（`execution-plan.md`） | 实际状态 | 结论 |
|-------------------------------|----------|------|
| `ipc/native/src/core/dbinder/source/ipc_rpc_entry.cpp` 新增 | `??` 未跟踪，存在 | 一致 |
| `interfaces/innerkits/ipc_single/libipc_rpc_map` 新增 | `??` 未跟踪，12 条导出 | 一致 |
| `interfaces/innerkits/ipc_single/libipc_single_map` 修改 | `M`，+74 符号行 | 一致 |
| `interfaces/innerkits/ipc_single/BUILD.gn` 修改 | `M`，+84/−6 | 一致 |
| `interfaces/innerkits/ipc_core/BUILD.gn` 修改 | `M`，+26 | 一致 |
| `BUILD.gn` 修改 | `M`，+1（`ipc_components` 追加 `ipc_rpc_core`） | 一致 |
| **未登记：`interfaces/innerkits/libdbinder/BUILD.gn`** | `M`，+4（deps 加 `ipc_rpc`） | **不一致（L-1）**：`execution-plan.md:121` 概览行已写「偏离提前」，但 `:687-731` 的映射表 TASK-3 无此行 |
| **未登记：`.gitignore`** | `M`，+5/−1 | **不一致（L-2）**：4 个流程工具目录（含 `codespec/`，即本评审证据所在）被忽略 |
| 映射表其他行（TASK-1/2/4/5/6） | 与工作区一致 | 未发现额外越界改动 |

`git status --short` 全量 19 个 `M` + 6 个 `??`，除上表外均可归属 TASK-1/TASK-2 或 `test/unittest` 的既有工作，无其他计划外文件。

## 发现

### 阻塞发现

| ID | 严重度 | 类别 | 位置 | 问题 | 证据 | 建议 |
|----|--------|------|------|------|------|------|
| B-1 | **High** | ③ 漏改 | `test/fuzztest/ipc/native/core/dbindergrantpermission_fuzzer/BUILD.gn:31`、`test/fuzztest/ipc/native/core/getlocalnodedeviceid_fuzzer/BUILD.gn:31` | 两个模糊测试目标仍以 `:ipc_single`（共享库）为依赖，并在源码中直接调用 `DBinderSoftbusClient::GetInstance()`。TASK-3 已把 `DBinderSoftbusClient` 全部 9 个方法迁到 `libipc_rpc.z.so`，`libipc_single.z.so` 的动态导出中该符号计数为 0，且 `libipc_rpc` 并非这两个目标的依赖（链接行只有 `communication/ipc/libipc_single.z.so`），因此链接必然报 `undefined reference to OHOS::DBinderSoftbusClient::GetInstance()`。该面没有任何 Task 认领：TASK-3 的 Files 表不含 fuzz 文件、TASK-5 明令「`test/fuzztest/` 下全部文件」禁改、TASK-4 的映射不含 `test/`。计划 `:564` 声称「其依赖 `ipc_single_test`，无需改动」与事实相反 | (1) 链接行：`out/x86_64_virt/obj/foundation/communication/ipc/test/fuzztest/ipc/native/core/{getlocalnodedeviceid,dbindergrantpermission}_fuzzer/*.ninja:14` 只含 `communication/ipc/libipc_single.z.so`，无 `libipc_rpc.z.so`，且 `build/` 内 `grep -rn "allow-shlib-undefined\|unresolved-symbols"` 无命中（无放行开关）。(2) 调用点：`dbindergrantpermission_fuzzer.cpp:44`、`getlocalnodedeviceid_fuzzer.cpp:42`；两者均 `#include "dbinder_softbus_client.h"`（`:17`），编译期 include 路径存在（ninja 内 `-I../../foundation/communication/ipc/ipc/native/src/core/dbinder/include`），故失败点确定在链接。(3) 导出：`llvm-nm -D --defined-only libipc_single.z.so \| grep -c DBinderSoftbusClient` → 0；`libipc_rpc.z.so` → 9。(4) 图可达：两目标在 `test/fuzztest/ipc/native/core/BUILD.gn:32,37` 的 group `ipc_nativecore_fuzztest` 内 → `test/fuzztest/ipc/native:ipc_native_fuzztest` → `test:ipc_fuzztest` → `test:ipc_test`（`test/BUILD.gn:177-186`），并已登记为 part test（`out/x86_64_virt/build_configs/communication/ipc/BUILD.gn:159-163` 的 `test_packages = ["//foundation/communication/ipc/test:ipc_test"]`，`parts_test_list.gni:162`）。故 `ipc_components` 绿不代表测试/模糊构建绿 | 二选一，任选其一即可闭环：**(a)** 把这两个 `BUILD.gn` 列入 TASK-3（或单列 TASK-3b）Files 表，deps 增 `interfaces/innerkits/ipc_single:ipc_rpc`；**(b)** 与同目录 6 个已安全目标对齐，改 dep 为 `$SUBSYSTEM_DIR/test:ipc_single_test_static`（该静态库 `test/BUILD.gn:104-109` 仍含 4 个 TU）。同时修正 `execution-plan.md:564` 的错误前提，并在 TASK-6 增加「构建 `ipc_test`/`ipc_fuzztest`」一项证据 |

**为何 High 而不是 Medium**：这是新引入的、确定的构建期断链（不是潜在风险），落在标准 part-test/模糊构建路径上，且现网无任何 Task 承接；按本仓评审约定 Critical/High 计为阻塞。

### 非阻塞发现

| ID | 严重度 | 类别 | 位置 | 问题与影响 | 证据 | 建议 |
|----|--------|------|------|-----------|------|------|
| M-1 | Medium | ④ 外部接口 | `interfaces/innerkits/ipc_core/BUILD.gn:78` | 新库的安装目标用了 `innerapi_tags = [ "platformsdk" ]`（直连档），而其源目标 `ipc_single/BUILD.gn:229` 用的是 `platformsdk_indirect`。`platformsdk` 会把 `libipc_rpc.z.so` 登记为平台 SDK 直连 innerapi，对外形成 12 个 `@1.0` 符号的 ABI 承诺，与 D-04「避免把内部实现意外变成对外可见面」相反；同仓同类私有库（dsoftbus 各 `nstackx`/`sdk` 目标）统一用 `platformsdk_indirect` | `ipc_rpc_core_module_info.json` → `innerapi_tags: ["platformsdk"]`；源目标 `ipc_single/BUILD.gn:229` → `["platformsdk_indirect"]`；`grep -rn platformsdk_indirect --include=BUILD.gn foundation/communication` 命中 13 处私有库先例 | 安装目标改为 `platformsdk_indirect`（与源目标一致），或在此处写明为何需要直连并补 innerapi 版本/ABI 维护说明 |
| M-2 | Medium | ③ 完整性 | `interfaces/innerkits/ipc_single/libipc_rpc_map:36-45` vs `libipc_single_map:19-26,157` | `execution-plan.md:352` 写明「同一清单必须同时存在于 `libipc_single_map`」，实际两表不等：rpc 表有 9 个 `DBinderSoftbusClient` 方法（含 `DBinderRemovePermission`），single 表只有 8 个（无 `DBinderRemovePermission`）。语义上无功能影响（该符号仅由 `ipc_rpc_entry.cpp:88` 经能力表调用，`libdbinder` 的 8 个未定义引用不含它；基线 single 也从未导出它），但 TASK-4 若按「两表相同」的假设做关闭态回退会误判。另：single 表保留的 8 条 `DBinderSoftbusClient` 条目在 TASK-3 后已无定义（悬空条目） | 行数实测：`grep -c DBinderSoftbusClient libipc_single_map` → 8、`libipc_rpc_map` → 10（含注释行）/ 9 条符号；`llvm-nm -D -u libdbinder.z.so \| grep DBinderSoftbusClient` → 8 条且无 `RemovePermission`；基线 single 导出 8 条、无 `RemovePermission` | 把 `execution-plan.md:352` 的不变量改写为「rpc 表为准，single 表按实际需要」，并在 TASK-4 的关闭态验收里显式校验 `libdbinder` 的 8 个未定义符号（而不是逐条比对两表）；single 表的 8 条悬空条目建议加注释说明「仅为关闭态预留」 |
| M-3 | Medium | ④ 运行期解析 | `ipc/native/src/core/dbinder/source/ipc_rpc_loader.cpp:94`、`build/common/musl/BUILD.gn:141-150` | 加载器用裸名 `dlopen("libipc_rpc.z.so", …)`（`ipc_rpc_ops.h:34`），依赖镜像内的 loader 搜索路径；仓内只提供 `ld-musl-{aarch64,arm,riscv64}.path`，且 `BUILD.gn:141-150` 对其它架构也只会装 `ld-musl-aarch64.path` 这个文件名。rk3568（arm64）路径含 `/system/lib64/platformsdk`，生产态成立；但本轮验收用的 x86_64_virt 不读该文件，故「同目录安装即可被 dlopen 找到」在本产品族上没有直接证据（不是本次改动引入的缺陷，但属于本 Task 验收口径的缺口） | `build/common/musl/` 下只有 3 个 `.path`；`out/x86_64_virt/packages/phone/system/etc/` 只有 `ld-musl-aarch64.path`；`ld-musl-aarch64.path:1` 含 `/system/lib64/platformsdk` | 在 TASK-6 增加一项：在 arm64 目标（或镜像侧 `readelf -d` + loader 路径核对）确认 `libipc_rpc.z.so` 可被裸名 dlopen；或在加载器里给出相对 `libipc_single.z.so` 所在目录的兜底路径 |
| L-1 | Low | ③ 登记 | `execution-plan.md:687-731` | TASK-3 的代码范围映射缺 `interfaces/innerkits/libdbinder/BUILD.gn`（实际 `M`，+4 行）。该偏离在 `:121` 概览与 Actual Result 有记录，但映射表缺行会让 Gate-Final 的「实际文件与映射一致」检查判不一致 | `git status --short` 有该文件；`execution-plan.md:687-731` 的 TASK-3 仅 6 行 | 在映射表 TASK-3 补一行并标注「偏离提前（原 TASK-4）」 |
| L-2 | Low | ③ 登记 | `.gitignore:22-25` | 新增 `codespec/`、`docs/superpowers/`、`matspec/`、`.matspec-cli/` 四行，不属于任何 Task；其中 `codespec/` 正是本次变更的证据与评审产物目录，被 ignore 后证据不会随变更提交 | `git diff --stat .gitignore` → `6 +++++-`；`git status --short` 中 `codespec/` 未出现在未跟踪列表 | 拆成独立 chore 提交，或至少不要把 `codespec/` 排除在版本控制外 |
| L-3 | Low | ③ 证据准确性 | `execution-plan.md:400-441`（Actual Result） | 两处计数与事实不符：「原有 32 个 TU」（基线实际 30 项 = 28 `.cpp` + 2 `.c`）；「修改 3 个 BUILD.gn」（实际 4 个：根 `BUILD.gn`、`ipc_core/BUILD.gn`、`ipc_single/BUILD.gn`、`libdbinder/BUILD.gn`）。不影响实现，但会误导后续 TASK-6 的基线核对 | 脚本比对 `git show HEAD:...BUILD.gn` → 30；`git diff --numstat` 四个 BUILD.gn 均有变更 | 校正 Actual Result 的两处数字 |
| L-4 | Low | ③ 证据一致性 | `out/x86_64_virt/packages/phone/{system,updater}/lib64/platformsdk/libipc_single.z.so`、`out/x86_64_virt/innerkits/ohos-x86_64/ipc/ipc_single/libipc_single.z.so` | 镜像/中间仓仍是拆分前产物（492,056 B @01:29/@04:30，与基线同尺寸），且 `packages/` 下无 `libipc_rpc.z.so`。谁若直接用这些目录做上板/联调，看到的是旧布局，会掩盖 B-1 与安装缺口 | `ls -l` 时间戳与体积；`grep -rl DBinderSoftbusClient out/x86_64_virt/packages` 只命中旧 `libipc_single.z.so` 与旧 `libdbinder.z.so` | 在 TASK-3 证据里标注「`packages/` 为 pre-change 残留，不作为验收依据」，并在执行 `make_packages` 后重新取证 |
| L-5 | Low | ④ 仓内卫生 | `interfaces/innerkits/ipc_core/libipc_core_map:28,48` | 该版本脚本仍列出 `OHOS::DBinderSoftbusClient*`、`OHOS::DBinderDatabusInvoker*`，但已无任何 BUILD.gn 引用它（`grep -rn libipc_core_map --include=*.gn` 无命中），属拆分前遗留死文件。不参与构建，无功能影响 | `grep -n "DBinder" interfaces/innerkits/ipc_core/libipc_core_map` → `:28`,`:48`；`git status --short` 未修改 | 可在后续清理任务中删除或注明为遗留，不建议在本 Task 扩大改动面 |

### 已排除的疑似问题（避免误导）

| 疑点 | 排除依据 |
|------|----------|
| `libipc_rpc` 与 `libipc_single` 各有一份 `InvokerDelegator<DBinderDatabusInvoker>` 静态注册（rpc entry TU 复用同一头） | `invoker_factory` 的注册容器以类型为键，重复 `insert` 返回 false；`RTLD_NODELETE` 使 rpc 库不析构，故不存在「一侧 Unregister 打掉另一侧」的配对风险 |
| rpc 库少定义 `ENABLE_IPC_TRACE` 导致行为差异 | `config.gni:23` 该特性默认关闭；4 个迁移 TU 内无该宏引用 |
| `DBINDER_PKG_NAME` 等三常量从 `static const` 改为 `inline const` 后取值变化 | 与 HEAD 逐字相同（`"DBinderBus"`、`"DBinder"`、`"_"`），且产物侧为 `LOCAL` 单份 |
| 仓外还有其它 `libipc_single` 消费方被断链 | 产物级扫描（`out/x86_64_virt`，排除 `obj/`）中 `DBinderSoftbusClient` 只出现在 `libdbinder`、`libipc_rpc`、`ipc_single_test` 及其资源副本；源码级消费方见 R-3 全表 |

## 阻塞判定

| 判定项 | 结果 |
|--------|------|
| 是否存在 Critical | 否 |
| 是否存在 High | **是：B-1**（2 个 fuzz 目标确定性断链，无 Task 承接） |
| 按本仓约定（Critical/High 计阻塞）的结论 | **FAIL，blocking_findings = 1** |
| 漏改计数 | **1**（测试/模糊模块的 2 个构建目标）；生产模块 0 |
| 解除阻塞的最小条件 | B-1 二选一修复（加 `ipc_rpc` 依赖或改 `ipc_single_test_static`）+ 修正 `execution-plan.md:564` 的错误前提 + 补一条「构建 `ipc_test`/`ipc_fuzztest` 通过」的证据；M-1/M-2/M-3 建议同批处理，但不阻塞 |

## 复验（第 2 轮）

复验基线：仓根 = `code/foundation/communication/ipc`，产物 = `code/out/x86_64_virt`，工具 = `code/prebuilts/clang/ohos/linux-x86_64/llvm/bin/{llvm-nm,llvm-readelf,llvm-ar}`。本轮只读工作区与已产出产物，未执行 `./build.sh`，未执行改写工作区的 git 命令，仅修改本文件。

### 结论

- **verdict: PASS**，**blocking_findings: 0**。
- 上轮唯一阻塞 **B-1 已闭环**，判定取自产物本身（静态测试库成员、链接行输入、fuzz 可执行文件符号表），不以作者自述为证据。
- M-1、M-2、M-3、L-1 已闭环；**L-3、L-4 仅部分修正**，L-2、L-5 维持原状，四项均为 Low 非阻塞。
- 新发现 1 项 Low：**N-1**，`libipc_rpc_map` 多导出了无人引用的 `DBinderDatabusInvoker` 构造函数。
- **本轮未在设备/模拟器上运行任何用例**：环境无 `hdc`、无 `qemu-system-x86_64`，`make_packages` 仍被环境阻塞（`execution-plan.md:420`）。因此 x86_64_virt 上「绝对路径 dlopen 成功」只有静态证据，运行期验证仍按计划留在 TASK-6。

### 逐项复验

| ID | 上轮结论 | 本轮证据（命令 → 输出摘要；`file:line`） | 本轮判定 |
|----|----------|------------------------------------------|----------|
| B-1 | 阻塞（High） | (1) `grep -n deps test/fuzztest/ipc/native/core/{dbindergrantpermission,getlocalnodedeviceid}_fuzzer/BUILD.gn` → 两者均为 `:33 deps = [ "$SUBSYSTEM_DIR/test:ipc_single_test_static" ]`，已无 `:ipc_single`。(2) 链接行输入：`.../DBinderGrantPermissionFuzzTest.ninja` 中 IPC 侧输入只有 `obj/.../test/libipc_single_test_static.a` 与 fuzzer `.o`，无 `libipc_single.z.so`（`grep -o` 实测）。(3) `llvm-ar t obj/.../test/libipc_single_test_static.a` → 含 `databus_socket_listener.o`、`dbinder_databus_invoker.o`、`dbinder_softbus_client.o`、`dbinder_softbus_client_death_recipient.o`、`ipc_rpc_loader.o`。(4) `llvm-nm -C exe.unstripped/tests/fuzztest/ipc/core/DBinderGrantPermissionFuzzTest \| grep 'DBinderSoftbusClient::GetInstance'` → `000000000005b750 T …`；`llvm-nm -u -C … \| grep -c DBinderSoftbusClient` → **0**；`GetLocalNodeDeviceIdFuzzTest` 同为 **0**，两二进制与静态库 mtime 一致（11-13 00:33）。 | **已闭环** |
| M-1 | Medium（不阻塞） | `cat obj/.../ipc_core/ipc_rpc_core_module_info.json` → `"innerapi_tags":["platformsdk_indirect"]`、`dest` 为 `system/lib64/platformsdk` + `updater/lib64/platformsdk`、`install_enable:true`；`interfaces/innerkits/ipc_core/BUILD.gn:86` 与 `:65` 注释（「tagged like the source target's platformsdk_indirect」）一致。 | **已闭环** |
| M-2 | Medium（不阻塞） | `llvm-nm -D --defined-only communication/ipc/libipc_rpc.z.so \| grep -c ' T '` → **11**，逐行为 `IpcRpcGetOps@@1.0` + 8 个 `DBinderSoftbusClient::*` + `DBinderDatabusInvoker::C1/C2`；`llvm-nm -D -u libdbinder.z.so \| grep -c DBinderSoftbusClient` → **8** 且 `RemovePermission` 计数 **0**；`libipc_single_map:205` 有 `/* used by libipc_rpc ... */` 分块，`tail -6` 确认文末 `local: *` 仍在。`DBinderRemovePermission` 虽不在导出表，仍在 `lib.unstripped/.../libipc_rpc.z.so` 内以 `t` 定义（`ipc_rpc_entry.cpp:86-88,123` 经能力表调用），能力表语义未破坏。 | **已闭环**（single 表 8 条悬空 DBinder 条目按建议留 TASK-4 关闭态，非缺陷） |
| M-3 | Medium（不阻塞） | `ipc_rpc_loader.cpp:88` 用 `dladdr(IpcRpcOwnLibraryMarker)` 取自身映像目录；`:133` `JoinLibraryPath(GetOwnLibraryDirectory(), IPC_RPC_LIBRARY_NAME)` 拼绝对路径；`:207` 唯一 `dlopen(expectedPath.c_str(), …)`，无裸名兜底；`:258` 校验入口符号映像（`:147` 用 `st_dev`/`st_ino`）；`ipc_rpc_ops.h:52` 仅提供文件名常量。 | **已闭环**（不再依赖 `ld-musl-*.path`；运行期仍归 TASK-6） |
| L-1 | Low | `grep -n '^\| TASK-3 \|' execution-plan.md` → 10 行，含 `:720 libdbinder/BUILD.gn`（偏离提前）、`:721 test/BUILD.gn`、`:722-723` 两个 fuzz `BUILD.gn`；`git status --short \| grep -c BUILD.gn` → 8，扣除属 TASK-1 的 `test/unittest/ipc/native/core/BUILD.gn` 后 7 个，与映射一致。 | **已闭环** |
| L-3 | Low | 正文已修正：`:432` 为「基线 30 项…26 项 + `ipc_rpc_loader.cpp` 共 27 项」，`:425` 为「修改 4 个 BUILD.gn（根/ipc_core/ipc_single/libdbinder）」。残留：闭环行 `:754` 写「TASK-3 修改 4 个 BUILD.gn」，与其自身 `:714-723` 的 7 个 `BUILD.gn` 不符（`grep -n "32 个 TU"` 仅命中 `:754` 的缺陷描述，正文已无 32）。 | **部分闭环**（Low，残留） |
| L-4 | Low | 文档已有 `make_packages` 环境阻塞记录（`execution-plan.md:420`），但未明确「`packages/` 为 pre-change、不作为验收依据」。实测 `packages/phone/system/lib64/platformsdk/libipc_single.z.so` 仍为 492,056 B @11-12 01:29，`libipc_rpc.z.so` 不存在；`grep -rla DBinderSoftbusClient out/x86_64_virt/packages` 命中 `system.zip`、`images/system.img`（均 11-12 04:35）与 system/updater 两份旧库。 | **部分闭环**（Low，残留） |
| L-2 | Low | `git diff .gitignore` 仍为新增 4 行；`git check-ignore -v codespec/.../task-3/scope.md` → `.gitignore:22:codespec/`；`git status --short` 无 `codespec/` 条目，本次评审证据不会被提交。 | **维持**（Low） |
| L-5 | Low | `grep -n DBinder interfaces/innerkits/ipc_core/libipc_core_map` → `:28`、`:48`；`grep -rn libipc_core_map --include=*.gn --include=*.gni .` 退出码 1（无构建引用）。 | **维持**（Low，可接受残留） |

> B-1 证据方法说明：本产品 `out/x86_64_virt` 的 ninja 图存在与本任务无关的重复规则错误（`multiple rules generate obj/applications/standard/contacts_data/Contacts_DataAbility.stamp`），`ninja -n` 无法加载全图，故本轮不引用 dry-run 结果，改用「产物符号表 + 链接行输入 + 静态库成员 + 时间戳」四证。

### 新发现

| ID | 严重度 | 类别 | 位置 | 问题与影响 | 证据 | 建议 |
|----|--------|------|------|-----------|------|------|
| N-1 | Low | ④ 导出面精度 | `interfaces/innerkits/ipc_single/libipc_rpc_map:47` | 导出了 `DBinderDatabusInvoker::DBinderDatabusInvoker()`（C1/C2 两符号），但无任何 DSO 未定义引用它；与映射表头部自述「limited to what libdbinder actually references」（`:26-29`）不符，导出面由 9 变 11。无功能影响，仅扩大私有 ABI 面。 | 遍历 `out/x86_64_virt/communication/ipc/*.so`（13 个）执行 `llvm-nm -D -u \| grep -c 'DBinderDatabusInvokerC[12]Ev'` → 全部为 **0**；`llvm-nm -D -u libdbinder.z.so \| grep -c DBinderSoftbusClient` → 仅 8 个 softbus 方法；构造函数只经 `InvokerDelegator` 在 TU 内实例化。 | 删除 `:47`（导出回到 9），或在注释中写明保留理由；与 TASK-4 关闭态清单一起核对。 |

### 阻塞判定（第 2 轮）

| 判定项 | 结果 |
|--------|------|
| 是否存在 Critical | 否 |
| 是否存在 High | **否**（B-1 已由产物实测闭环） |
| 阻塞发现合计 | **0** |
| 本轮结论 | **PASS** |
| 未验证项与原因 | 设备/模拟器运行期：无 `hdc`、无 `qemu-system-x86_64`；`make_packages`/镜像安装：preheat CMakeCache 路径不一致（环境）；x86_64_virt 实机 dlopen 与功能回归按计划留 TASK-6 |
| 结论证据边界 | 所有判定均引用本轮本地命令输出或 `file:line`；作者构建日志仅用于交叉核对目标名与时间戳，不作为结论依据 |

### 非阻塞遗留清单（供后续 Task 收口）

- L-3 残留：`execution-plan.md:754` 的「TASK-3 修改 4 个 BUILD.gn」应改为 7。
- L-4 残留：在 TASK-3/TASK-6 证据中显式标注 `packages/`、`system.zip`、`images/system.img` 为 pre-change 残留，不作为验收依据。
- L-2：`.gitignore:22` 忽略 `codespec/`，评审证据不会随变更提交。
- L-5：`interfaces/innerkits/ipc_core/libipc_core_map` 为无引用死文件。
- N-1：`libipc_rpc_map:47` 的构造函数导出可删除（Low）。
- （观察，不计缺陷）`libipc_single_map` 仍有 8 条 `DBinderSoftbusClient` 悬空条目，按 M-2 建议留作 TASK-4 关闭态预留。