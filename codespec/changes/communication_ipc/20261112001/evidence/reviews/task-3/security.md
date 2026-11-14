---
task_id: TASK-3
seat: security
verdict: PASS
blocking_findings: 0
---

# TASK-3 安全与攻击面评审（⑤）

> **第 2 轮复验（2026-11-13）改判 `PASS` / `blocking_findings: 0`**：S-01 已按第 1 轮给出的「代码侧收口」方案闭环，本轮在**当前工作区代码 + 重新构建的产物 + 本仓 musl 源码/反汇编**上独立复核通过。以下第 1 轮正文作为历史记录保留，最终判定以文末「复验（第 2 轮）」为准。

## 结论（第 1 轮，已由第 2 轮覆盖）

TASK-3 的**权限判定逻辑本身没有退化**：4 个迁移 TU 相对 HEAD 零改动（`git diff --stat` 空），核心侧 7 处能力表调用点全部先判空后解引用且空表分支均为 fail-closed，`grantPermission` 的返回值比较常量与 `ERR_NONE` 一致（均为 0）。这部分通过。

但本任务**未完成 `design.md:245` 给它下的强制义务**：新增的跨 DSO 边界 `dlopen("libipc_rpc.z.so")` 既没有做路径固定/加载后校验，也没有按设计允许的替代方案把该项登记为残余风险（TASK-3 验证表 `execution-plan.md:407-419` 无对应行，`grep -c 残余风险` 对 `proposal.md`/`spec.md`/`execution-plan.md` 均为 0，`design.md` 仅 `:245`/`:272` 两处声明性文字，无任何登记行）。该库随后提供的 `IpcRpcOps` 表直接承载两个权限原语（`grantPermission`/`removePermission`）和身份解析原语（`getPidAndUidFromServiceName`），而 `abiVersion` 由被加载库自述，无法证明来源。按评审规则「削弱权限判定保证」计为阻塞，故 **verdict = FAIL，blocking_findings = 1（S-01，High）**。

阻塞项的解除门槛与设计一致且成本很低：二选一即可——(a) 改为可校验的加载方式（同目录绝对路径 + `dlinfo(RTLD_DI_LINKMAP)` 校验信任目录），或 (b) 在 TASK-3 验证章节显式登记为残余风险并给出前置条件与后续处置任务。仅补文档即可让本席位改判 PASS。

未做运行期/设备侧验证：本机无 `hdc`、无可用模拟器，且本席位为只读评审，未执行 `./build.sh`。下文所有结论均来自源码、构建产物与装载器源码的静态核验。

## 维度结论

| 维度 | 结论 | 证据 |
|---|---|---|
| 权限判定完整性（不因拆分放宽） | 通过 | 4 个迁移 TU 零 diff；`ipc_object_stub.cpp:791,811` 的 `grantPermission`/`removePermission` 返回值与 `ERR_NONE`(=0) 比较，`ipc_rpc_ops.h:38-41` 注释与 `dbinder_softbus_client.h:29` 的 `SOFTBUS_CLIENT_SUCCESS=0` 一致；`AddCommAuth` 的 uid/pid/token 取自 invoker 虚函数（`ipc_object_stub.cpp:250-252`），未改为从能力表取 |
| 空表 fail-closed | 通过 | 7 处 `ops->` 使用点前均有 `ops == nullptr` 分支且返回既有错误值：`ipc_object_stub.cpp:244,283,786,806`、`ipc_object_proxy.cpp:1073,1135,1175`、`ipc_process_skeleton.cpp:545,1322`；`Peek()` 侧 `dbinder_session_object.cpp:44-48`、`ipc_process_skeleton.cpp:115` 不做解引用 |
| 跨 DSO 信任边界（库来源可验证性） | **不通过** | 见 S-01：`ipc_rpc_ops.h:34` 裸库名 + `ipc_rpc_loader.cpp:94` 裸 `dlopen`；`ipc_rpc_loader.cpp:124` 仅比较自述 `abiVersion`；`ipc_rpc_loader.cpp` 全文无 `dlinfo`/绝对路径 |
| 符号可见面/注入面 | 部分通过（带 Medium，见 S-02） | `libipc_single.z.so` 导出 167 → **235**（+78/−10，净 +68）；新增中包含 `IPCProcessSkeleton::AttachOrUpdateAppAuthInfo`/`QueryCommAuthInfo`/`QueryAppInfoToStubIndex`/`DetachAppAuthInfoBySocketId`、`DBinderSessionObject::SetPeerUid/SetPeerPid`、`MessageParcel::GetRawData/RestoreRawData`；两库 `readelf -d` 均无 `-Bsymbolic`（FLAGS 仅 `BIND_NOW`），故新增全局符号可被先序 DSO 抢占 |
| rpc 库导出收敛 | 通过 | `llvm-nm -D --defined-only libipc_rpc.z.so \| wc -l` = **12**（`IpcRpcGetOps@@1.0` + 9 个 `DBinderSoftbusClient` + `DBinderDatabusInvoker` C1/C2），`local: *` 收口生效 |
| 单向依赖 | 通过 | `readelf -d`：rpc 的 NEEDED 含 `libipc_single.z.so`；single 的 NEEDED 中 `libipc_rpc` 命中 0 |
| 静态初始化/重入 | 通过（本任务范围） | `nm libipc_rpc.z.so` 仅 `_GLOBAL__sub_I_dbinder_softbus_client.cpp` 一个 init 数组符号，无 `_GLOBAL__sub_I_ipc_rpc_entry.cpp`；`IpcRpcOps RPC_OPS` 为纯函数指针常量（`ipc_rpc_entry.cpp:117-127`），entry TU 不产生动态初始化，`design.md:243` 的「入口静态初始化期间禁止 `Get()`」在本任务产物上真空成立 |
| 生命周期/并发 | 通过 | `call_once` 包住全部 `dlopen`/`dlsym`（`ipc_rpc_loader.cpp:81-141`），`Peek()` 仅 `load(acquire)`（`:147`）；`RTLD_NODELETE` 保证表存活至进程退出 |
| 日志与信息泄漏 | 通过 | 加载失败日志仅 1 条且只附 `dlerror()` 文本（无路径白名单外的敏感数据）；`dbinder_session_object.cpp:41` 保留 `ConvertToSecureString` |
| 安装面 | 通过（声明级） | `ipc_rpc_core_module_info.json`：`dest=["system/lib64/platformsdk/libipc_rpc.z.so","updater/lib64/platformsdk/libipc_rpc.z.so"]`，与核心库同目录，未新增独立 SELinux 路径类别（实盘未产出，无法验证标签继承） |

## 独立核验记录

| # | 操作 | 输出片段（本方执行） |
|---|---|---|
| 1 | `git diff --stat -- <4 个迁移 TU>` | 输出为空（零改动）；`dbinder_session_object.cpp` 为 `6 insertions(+), 6 deletions(-)` |
| 2 | `llvm-nm -D --defined-only .../libipc_single.z.so \| wc -l` | `235`；基线 `out/x86_64_virt/packages/phone/system/lib64/platformsdk/libipc_single.z.so`（492,056 B）为 `167` |
| 3 | `comm -13 base new` / `comm -23 base new` | `added: 78  removed: 10`；移除项全部是 8 个 `DBinderSoftbusClient` 方法 + `DBinderDatabusInvoker` C1/C2，属搬迁而非裁剪 |
| 4 | `llvm-nm -D --defined-only .../libipc_rpc.z.so \| wc -l` | `12` |
| 5 | `llvm-readelf -d` 两库 | single：`SONAME libipc_single.z.so`、`FLAGS BIND_NOW`、无 RPATH/RUNPATH；rpc：`SONAME libipc_rpc.z.so`、`NEEDED libipc_single.z.so`、无 RPATH/RUNPATH |
| 6 | `git diff -U0 -- libipc_single_map \| grep -c "^+.*;"` | `74`（新增映射条目），`^-` 计数 `0`，`local: *` 仍在文末 |
| 7 | `grep -n "dlopen\|dlinfo\|realpath" ipc_rpc_loader.cpp` | 仅 `:94` 裸名 `dlopen(IPC_RPC_LIBRARY_NAME, ...)`，无 `dlinfo`、无绝对路径 |
| 8 | `sed -n 1860,1870p third_party/musl/ldso/dynlink.c` / `:1139` | `if (!libc.secure) { env_path = getenv("LD_LIBRARY_PATH"); ... }`；`:1139` `if (env_path) fd = path_open(name, env_path, ...)` 先于 sys_path，说明裸名解析受环境变量与先序目录直接影响 |
| 9 | `grep -n "namespace.default.lib.paths" out/.../ld-musl-namespace-x86_64.ini` | `...:/system/lib64:/vendor/lib64:/vendor/lib64/chipsetsdk:/vendor/lib64/chipset-sdk:/vendor/lib64/passthrough:/vendor/lib64/passthrough/indirect:/system/lib64/chipset-pub-sdk:/system/lib64/chipset-sdk:/system/lib64/platformsdk:...`——本组件安装目录 `platformsdk` 排在其后 8 个目录之后 |
| 10 | `llvm-nm libipc_rpc.z.so \| grep _GLOBAL__sub_I` | 仅 `_GLOBAL__sub_I_dbinder_softbus_client.cpp`，无 entry TU 初始化符号；`__cxx_global_var_init` 计 11 |
| 11 | `grep -rn "InvokerDelegator<"` | 生产代码中 `IF_PROT_DATABUS` 仅 `dbinder_databus_invoker.h:123` 一处注册，`binder_invoker.h:294` 为 BINDER；`AsDatabusInvoker` 的 `static_cast` 当前类型唯一 |
| 12 | `python3 -c` 读 `ipc_rpc_core_module_info.json` | `dest=['system/lib64/platformsdk/libipc_rpc.z.so','updater/lib64/platformsdk/libipc_rpc.z.so']`，`innerapi_tags=['platformsdk']`，`install_enable=True` |

## 攻击面清单

1. **新增运行期装载点**：首个 DBinder/RPC 入口触发 `IpcRpcOpsGet()` → `dlopen`（`ipc_rpc_loader.cpp:94`）。可达入口为 `DBINDER_INC_REFS`/`DBINDER_DEC_REFS`/`DBINDER_ADD_COMM_AUTH`/session name 事务（`ipc_object_stub.cpp:244,283,315,786,806`）与代理侧会话建立（`ipc_object_proxy.cpp:1073,1135,1175`）。纯设备内 IPC 不触达（`IPCSkeleton::GetLocalDeviceID` 走 `BinderInvoker`）。
2. **能力表即权限原语的载体**：`grantPermission`（`ipc_object_stub.cpp:791`）、`removePermission`（`:811`）、`getPidAndUidFromServiceName`（`ipc_object_proxy.cpp:1142,1182`，结果写入 `dbinderData.peerUid/peerPid`）、`getClientFd`（`ipc_object_stub.cpp:260,298`，结果作为 `AppAuthInfo` 的 listenFd 落库）——四者都直接参与授权/身份数据流。
3. **库解析顺序**：裸名 + 默认命名空间路径，`platformsdk` 之前存在 8 个候选目录（`/system/lib64`、`/vendor/lib64`、`/vendor/lib64/chipsetsdk`、`/vendor/lib64/chipset-sdk`、两个 `passthrough`、`/system/lib64/chipset-pub-sdk`、`/system/lib64/chipset-sdk`）；`libc.secure` 为假时 `LD_LIBRARY_PATH` 还会整体前置。
4. **符号注入面扩大**：+78 个全局导出（含上述 auth 表改写器、`SetPeerUid/SetPeerPid`、`MessageParcel::GetRawData/RestoreRawData`），两库均未使用 `-Bsymbolic`，先序 DSO 可抢占核心库内部调用。
5. **ABI 校验的语义边界**：`abiVersion` 是结构布局一致性检查，不是来源证明；正确形状的替身库可通过 `IsOpsComplete`（`ipc_rpc_loader.cpp:53-60`）。
6. **类型转换**：`AsDatabusInvoker` 为无校验 `static_cast`（`ipc_rpc_entry.cpp:49-52`），当前 `IF_PROT_DATABUS` 唯一注册故等价于 `dynamic_cast`；若未来注册第二种 invoker，将退化为 UB。
7. **未新增**：无新 fd 传递、无新 ioctl、无新权限位、无新 IPC transaction code；`DBinderRemoveSessionName` 仍受 `IsLocalCalling() && IsSamgrCall()` 门控（`ipc_object_stub.cpp:386`）。

## 发现

| ID | 严重度 | 位置 | 问题 | 影响 | 建议 |
|---|---|---|---|---|---|
| S-01 | **High（阻塞）** | `ipc_rpc_ops.h:34`、`ipc_rpc_loader.cpp:94,124`；对照 `design.md:245`、`execution-plan.md:218,407-419` | 跨 DSO 装载既未固定/校验路径，也未按设计允许的替代分支登记残余风险。`abiVersion` 由被加载对象自述，无法证明库来源 | 若攻击者能在解析顺序更靠前的目录放置同名库（或控制 `LD_LIBRARY_PATH`），核心库将接受其 `IpcRpcOps`，从而：`grantPermission` 无条件返回 0（跨设备会话授权被跳过）、`getPidAndUidFromServiceName` 返回伪造 pid/uid（对端身份被冒充）、`getClientFd` 注入任意 listenFd 污染 `AppAuthInfo`。前置条件是「对 `/system/lib64`、`/vendor/lib64`、`chipset*` 或进程环境有写/控制权」，普通 HAP 无法直接满足，故不是低权限应用可直利用漏洞；但它使设计明确要求收口的权限判定保证失去可验证性 | 二选一：(a) 按 `target_cpu` 拼绝对路径加载，并用 `dlinfo(handle, RTLD_DI_LINKMAP, &map)` 校验 `map->l_name` 位于信任目录（建议同时校验 `dli_fname` 的 realpath）；(b) 在 TASK-3 验证章节登记残余风险（前置条件、影响、由哪个任务收口），并在 `design.md` 风险表补行 |
| S-02 | Medium | `libipc_single_map`（新增 `/* used by libipc_rpc */` 74 条）；`libipc_single.z.so` 导出 167→235 | 为满足链接而导出的符号集合过宽，包含授权表与身份字段的改写入口：`AttachOrUpdateAppAuthInfo`、`QueryCommAuthInfo`、`QueryAppInfoToStubIndex`、`DetachAppAuthInfoBySocketId`、`AddStubByIndex`、`QueryStubByIndex`、`GetCurrent`、`DBinderSessionObject::SetPeerUid/SetPeerPid`、`MessageParcel::GetRawData/RestoreRawData` | 与 D-04「仓内私有、避免内部实现变成对外可见面」的意图存在张力；无 `-Bsymbolic` 时这些符号可被先序 DSO 抢占，进程内攻击面扩大（需能注入同进程 DSO，属高前置） | 只导出 rpc 侧 bitcode 真正未定义的最小集合（`-Wl,--no-undefined` 已能给出精确清单，可按符号逐个复核是否可改为在 rpc 侧内联/传参消除依赖）；至少为 auth 表与 `peerUid/peerPid` 改写接口评估 `-Bsymbolic-functions` 或改为 friend/显式注册，并在 `design.md` 记录导出面收敛结论 |
| S-03 | Low | `ipc_rpc_entry.cpp:49-52` | `AsDatabusInvoker` 无校验 `static_cast`，以「`IF_PROT_DATABUS` 只注册一种类型」为隐含前提 | 当前安全（已核唯一注册）；将来若新增 databus invoker 子类，将静默 UB，可能被用来做类型混淆 | 加 `assert`/`DCHECK`（release 下保留一个 `invoker == nullptr` 判空即可），或在 `invoker_factory` 侧禁止同协议二次注册 |
| S-04 | Low（既有/设计已登记） | `dbinder_session_object.cpp:44-48`；`design.md` D-09 | `Peek()==nullptr` 时仍无条件把 `socket_` 置 `SOCKET_ID_INVALID`，与「rpc 未加载 = 无 socket 需清理」的前提耦合；`libdbinder` 以 DT_NEEDED 载入 rpc 后该前提可能不成立 | 最坏为 fd 泄漏至进程退出（拒绝服务面很小） | 沿用 TASK-2 NB-04 建议：未 shutdown 时保留原值或加一次性 `ZLOGW` |
| S-05 | Low（既有） | `libipc_single.z.so`/`libipc_rpc.z.so` 动态段 | 两库均无 `-Bsymbolic`，全局符号语义可被抢占；属仓内既有构建风格（`dbinder_softbus_client.cpp:152` 也使用裸名 `dlopen`） | 需同进程注入能力，单独不构成提权 | 与 S-02 合并评估；本任务不要求修改 |
| S-06 | Low | `ipc_rpc_loader.cpp:132` + 9 处调用点日志 | 加载失败日志单条且无敏感数据（通过）；但事务型入口在 rpc 缺失镜像上可被对端反复触发产生同类 `ZLOGE`，属设计已登记的日志放大面（`design.md:272`） | 日志噪声/磁盘占用，非信息泄漏 | 转 TASK-5/TASK-6 按设计评估限频或降级 |

## 阻塞判定

- 计入阻塞的仅 **S-01（High）**：它削弱的是 `design.md:245` 明文要求收口的权限判定保证，且本任务两个允许的收口动作（固定/校验路径、登记残余风险）一个都没做。其余发现最高为 Medium（S-02），按规则不阻塞。
- 前置条件必须如实记录：利用 S-01 需要写 `/system/lib64`、`/vendor/lib64`、`chipset*` 等先序目录或控制目标进程的 `LD_LIBRARY_PATH`，普通三方应用不具备该能力。因此本项不是「低权限应用可直接利用」的漏洞，而是**信任边界未被验证 + 强制验收项缺失**；这也是判 High 而非 Critical 的原因。
- 解除方式（任一即可，改判 PASS、`blocking_findings: 0`）：
  1. 代码侧：`dlopen` 改为按 `target_cpu` 组装的绝对路径，并在成功后用 `dlinfo(RTLD_DI_LINKMAP)` 校验实际 `l_name`/`l_addr` 落在 `/system/lib64/platformsdk`（或 updater 对应目录）；若校验失败按加载失败处理（`ops = nullptr`，保持现有 fail-closed 语义）。
  2. 文档侧：在 TASK-3 验证章节新增一行「加载路径信任」并登记为残余风险，写明前置条件、影响面、收口任务（例如 TASK-4 开关化时一并处理），同步在 `design.md:245` 标注决议。
- 已验证不受本阻塞项影响的结论（即使 S-01 未修）：权限判定代码零改动、空表分支全部 fail-closed、rpc 库导出收敛为 12、依赖单向、无 entry TU 动态初始化。**未验证项**：设备/模拟器上的实际装载解析结果、SELinux 对新增库文件的标签、`packages/` 实盘安装（环境阻塞，`make_packages` 不可用）。

---

## 复验（第 2 轮）

**复验时间**：2026-11-13。**对象**：当前工作区（含未提交改动与未跟踪新文件）中的 `ipc_rpc_loader.cpp/.h`、`ipc_rpc_ops.h`、`ipc_rpc_entry.cpp`、12 个能力表调用点、loader 单测、`out/x86_64_virt` 重新构建的产物，以及本仓 musl 装载器源码与 `libc.so` 反汇编。**方法**：只读静态核验 + 宿主侧实跑单测。musl 行为不采信设计/实现注释的叙述，全部回到源码与反汇编独立确认。

### 1. 复验结论

**S-01 关闭，判定 `verdict = PASS`、`blocking_findings = 0`。**

实现方选择了第 1 轮给出的方案 (a)（代码侧收口），并且没有停留在「换个写法」：加载路径、加载后来源校验、失败语义、单次日志四段都做了，且四段在本轮都能被独立证据支撑。

需要更正第 1 轮建议中的一处技术错误：我当时建议用 `dlinfo(handle, RTLD_DI_LINKMAP)` 校验。该方案在本仓 musl 上**恒失败**（`HANDLE_RANDOMIZATION` 下 `dlopen` 返回随机 opaque handle，而 `dlinfo` 把入参直接当 `struct dso *` 用 `head` 链表比对）。实现方改为对 `dlsym` 得到的**入口符号地址**做 `dladdr()`，本轮独立验证后认定这不是降级替代，而是本平台唯一可行且更强的方案：校验对象从「handle 自称的身份」变成「即将被调用的那段代码所在映像」。

### 2. S-01 信任链逐段核验

| 段 | 实现位置 | 本轮核验事实 | 判定 |
|---|---|---|---|
| ① 定位自身文件 | `ipc_rpc_loader.cpp:85-92` | 对同 DSO 内的 `IpcRpcOwnLibraryMarker()` 取地址做 `dladdr`，失败即返回空串 | 通过 |
| ② 取自身目录 | `:94-105` | 以最后一个 `/` 截断；无 `/` 返回空串；`/x.so` 返回 `/` | 通过 |
| ③ 拼绝对路径 | `:107-116`、`:125-134` | 空目录 → 空路径，不拼裸名；无任何「拼失败回退裸名」分支 | 通过 |
| ④ 按绝对路径打开 | `:194-214`（`:207`），flags `:48` | `dlopen("<own-dir>/libipc_rpc.z.so", RTLD_NOW\|RTLD_NODELETE\|RTLD_LOCAL)`；含 `/` 的路径在 musl 中走直接 `open()` 分支，不经过搜索路径 | 通过 |
| ⑤ 取入口符号 | `:250` | 只用**该 handle** 做 `dlsym`；`RTLD_LOCAL` 且 `find_sym_by_deps` 只查本 DSO + 依赖，不可能被无关先序 DSO 满足 | 通过 |
| ⑥ 校验提供者 | `:171-182`、`:140-148` | 对**入口符号地址** `dladdr`；`dli_fname` 与 expectedPath 串相等通过，否则 `stat` 比 `st_dev`/`st_ino`；同名异文件仍拒绝 | 通过 |
| ⑦ 失败语义 | `:258-260`、`:274-279`、`:280`、`:287`、`:291-294` | 校验失败不发布能力表；单条日志（`:276`）后缓存 `g_ops = nullptr`；`call_once` 保证不重试；`Peek()` 只做 atomic load | 通过 |
| ⑧ 判定顺序 | `:250` → `:258` → `:264` | 先确认入口符号的来源映像，**之后**才调用 `IpcRpcGetOps()` 并检查 `abiVersion`/完整度；`abiVersion` 仍只作布局一致性检查，不再承担来源证明 | 通过 |

### 3. musl 事实独立核验（不采信作者叙述）

| # | 待证事实 | 本方证据 | 结论 |
|---|---|---|---|
| M-1 | `HANDLE_RANDOMIZATION` 仅在非 asan/tsan 构建开启 | `third_party/musl/musl_template.gni:543-548`：`} else { defines = [ "HANDLE_RANDOMIZATION", "LOAD_ORDER_RANDOMIZATION" ] }`（`if (is_asan \|\| is_tsan)` 的 else 分支） | 成立 |
| M-2 | `dlopen` 返回随机 opaque handle | `third_party/musl/ldso/linux/dynlink_rand.c:96-105` `gen_handle()` 用 `getrandom(GRND_RANDOM\|GRND_NONBLOCK)` 生成并按 handle 链表去重；`:107-115` `assign_valid_handle()` 返回该 handle | 成立 |
| M-3 | `dlinfo` 把入参当 `struct dso *` 直接回填 | `third_party/musl/src/ldso/dlinfo.c:6-13`：`if (__dl_invalid_handle(dso)) return -1; ... *(struct link_map **)res = dso;` | 成立 |
| M-4 | `__dl_invalid_handle` 遍历 `head` 比对 dso 指针 | `third_party/musl/ldso/linux/dynlink.c:6443-6449`：`for (p=head; p; p=p->next) if (h==p) return 0;` | 成立 |
| M-5 | 上述结论在**实际构建的 libc.so** 中生效（而非仅源码） | `llvm-objdump -d --disassemble-symbols=dlinfo .../libc.so`：`c6070 <dlinfo>` → `callq 0x8e280`（`__dl_invalid_handle` 内联体，load `0x15b768` 即 `head`，`cmpq %rsi,%rax`、`movq 24(%rax)` 遍历）；通过后 `c6099: movq %rbx,(%r15)`，`%rbx` 即原始 handle | 成立，`dlinfo` 在本平台对随机 handle 恒返回 -1 |
| M-6 | `dladdr` 是真实 ldso 实现，返回 `p->name`（ADLT 时返回名字组合） | `dynlink.c:6976-6994`：`addr2dso(addr)` → `info->dli_fname = p->name`（或 `p->adlt_ndso_name`） | 成立 |
| M-7 | 含 `/` 的路径不走 `LD_LIBRARY_PATH`/搜索路径 | `dynlink.c:3077-3088`：`if (strchr(name, '/')) { pathname = name; ... fd = open(name, O_RDONLY\|O_CLOEXEC); }`；环境变量与 sys_path 只在无 `/` 的后半段使用 | 成立 |
| M-8 | 同一文件按不同拼写预加载时，DSO 保留首次路径串（故串比较不足，需要 inode 兜底） | `dynlink.c:3145-3156`：`find_library_by_fstat` 命中后只补 `p->shortname`、**不重写** `p->name`；`:3201-3204` `p->name` 来自首次的 `pathname` | 成立 |
| M-9 | 带 handle 的 `dlsym` 只查该 DSO 及其依赖 | `dynlink.c:7060-7078`（`__dlsym`：`find_dso_by_handle` → `do_dlsym`）；`:6494-6527`（handle 分支 `use_deps = 1`）；`:1664` `find_sym_by_deps` | 成立，跨库抢占入口符号不可行 |

### 4. 攻击路径逐条复验

| 攻击路径 | 本轮机制核对 | 结果 |
|---|---|---|
| `LD_LIBRARY_PATH` 前置同名库 | 加载路径含 `/`，musl 走 `open(name)` 直接打开（M-7）；环境变量分支只作用于裸名 | **关闭** |
| 搜索顺序更靠前的目录（`/system/lib64`、`/vendor/lib64`、`chipset*` 等） | 同上；且 `ns.default.lib.paths` 不再参与 | **关闭** |
| 预先用搜索路径拼写装载的**同名异 inode** 恶意副本 | 绝对路径按 inode 去重（M-8）会映射真实文件；入口符号取自真实文件的依赖闭包（M-9），`dladdr` 指向真实文件 → 校验通过并调用真实实现；恶意副本不会提供入口符号 | **对 S-01 关闭**。恶意副本若已被预加载，其静态构造已执行，属「同进程 DSO 注入」类（S-02/S-05 面，需写系统目录或控制进程环境），不因本次修改新增 |
| 替换 `expectedPath` 指向的 trusted 目录文件 | 需要写 `/system/lib64/platformsdk`，与替换 `libipc_single.z.so` 同级权限；威胁模型外 | 不新增风险 |
| 用另一 DSO 抢占 `IpcRpcGetOps` 符号 | `dlsym(handle)` 只查本 DSO + 依赖（M-9），且 `RTLD_LOCAL` | **关闭** |
| 由 rpc 库的依赖提供同名入口符号 | `dladdr` 会指向依赖的映像，与 expectedPath 不同文件 → 拒绝（fail-closed） | **关闭** |
| 通过 CWD 影响解析结果 | 仅当核心库自身以相对路径被装载时，own-dir 为相对目录，expectedPath 变相对路径 → **残余 R2-01**（生产路径不可达，加固项） | 残余（Low） |
| ADLT 形态打包 | `dli_fname` 为 `adlt名:库名` 组合（M-6、`dynlink.c:8397-8441`），`stat` 失败 → 恒拒 → **残余 R2-02**（可用性，非越权） | 残余（Low） |
| 校验失败后镜像仍驻留 | `dlopen` 成功即已跑静态构造，校验在其后（`:207` vs `:258`），`RTLD_NODELETE` 不卸载（`:225-226`）→ **残余 R2-03**（需能写入 trusted 路径才成立） | 残余（Low，接受） |

### 5. 本轮新发现与残余

| ID | 严重度 | 位置 | 问题 | 影响 | 处置建议 |
|---|---|---|---|---|---|
| R2-01 | Low（加固） | `ipc_rpc_loader.cpp:85-116` | 未断言 `GetOwnLibraryPath()` 返回绝对路径。若核心库自身以相对路径被 `dlopen`，own-dir 为相对目录，expectedPath 相对 CWD，校验锚点随 CWD 漂移 | 生产形态不可达：核心库由链接期/`DT_NEEDED` 经绝对搜索目录装载，`p->name` 为绝对串；仅在非常规宿主（测试/自定义装载器）下才可能 | 在 `ResolveRpcLibraryPath()` 增加 `ownPath[0] == '/'` 断言，不满足即返回空路径（fail-closed，零成本） |
| R2-02 | Low（可用性） | `ipc_rpc_loader.cpp:171-182`；`dynlink.c:6990-6994,8397-8441` | 若 `libipc_rpc.z.so` 将来被打进 ADLT 包，`dli_fname` 为 `adlt名:库名`，`stat` 必失败，`dladdr`/路径串比较均不通过 | 该打包形态下 RPC 恒不可用（既有错误码，不崩溃）；当前安装形态为普通 `.so`，不适用 | 在 `ipc_rpc_ops.h` 的私有 ABI 登记中补一句「不支持 ADLT 打包；若引入需同步调整校验」 |
| R2-03 | Low（接受） | `ipc_rpc_loader.cpp:207,225-226,258` | 校验发生在 `dlopen` 之后：被拒镜像的静态构造已执行，且 `RTLD_NODELETE` 下不会卸载 | 仅在「攻击者可写 trusted 目录/替换 rpc 库」时成立，此时核心库同样可被替换；是 dlopen-then-verify 的固有边界，非本次实现缺陷 | 保持现状，在设计中如实记录该边界（当前 `:150-170` 注释已说明「被拒镜像保持映射但不被调用」） |
| R2-04 | Low（测试覆盖） | `ipc_rpc_loader_unittest.cpp:334-335`、`:213-222` | 「同名异文件」负例使用**不存在**的文件 `libipc_rpc_absent_probe.z.so`，实际只覆盖 `stat()` 失败分支，未覆盖「同名同目录不同 inode」；`PathVerificationFailureIsRejected` 用 `verifyHook` 短路，未走真实拒绝路径 | 生产逻辑本身正确（`NamesSameFile` 的 dev/ino 比较是直白的），但回归网对「同名异 inode」这一**最关键负例**无真实覆盖 | 在临时目录造两个同名文件（或 hardlink + 独立副本）覆盖 dev/ino 不同分支；真实拒绝路径可由 TASK-5 的端到端用例补齐 |
| R2-05 | 观察（通过） | `ipc_rpc_loader.cpp:41-42,66-74,274-279` | 加载失败仅 1 条 `ZLOGE`，字段为 expectedPath 与固定原因串（`dlerror()` 文本最多含库路径），无 uid/pid/token/会话名 | 无信息泄漏；符合第 1 轮要求 | 无需改动 |
| R2-06 | 观察 | `ipc_rpc_loader.cpp:207` 调用的 `dladdr`/`stat`/`dlopen`/`dlsym` | 校验原语自身来自 libc；理论上的抢占需有 DSO 排在 libc 之前进入全局作用域，而 libc 随主程序最先装载，运行期 `dlopen` 的 DSO 只能排在其后 | 实际不可利用；与 S-05 同属「全局符号无 `-Bsymbolic`」的既有面 | 与 S-02/S-05 合并跟踪 |

### 6. 既有发现状态（第 2 轮）

| ID | 第 1 轮 | 第 2 轮状态 | 本轮依据 |
|---|---|---|---|
| S-02 | Medium | **维持 Medium，不阻塞** | 导出仍为 235（`llvm-nm -D --defined-only libipc_single.z.so \| wc -l`），`libipc_single_map:205-281` 仍是 74 条 `/* used by libipc_rpc */` 映射；`AttachOrUpdateAppAuthInfo`/`QueryCommAuthInfo`/`QueryAppInfoToStubIndex`/`DetachAppAuthInfoBySocketId`、`DBinderSessionObject::SetPeerUid/SetPeerPid`、`MessageParcel::GetRawData/RestoreRawData` 仍在导出集合中；两库 `FLAGS` 均只有 `BIND_NOW`，无 `SYMBOLIC`。`ipc_rpc_ops.h:33-50` 的「Registered private ABI」**只能补齐 A1 的登记缺口**（私有 ABI 属性、必须同构建同目录、不混镜像、fail-closed、为何不做版本节点隔离），无法缩小技术面 |
| S-03 | Low | **维持 Low** | `ipc_rpc_entry.cpp:49-52` 仍为无校验 `static_cast`；`IF_PROT_DATABUS` 仍只有 `dbinder_databus_invoker.h` 一处注册 |
| S-04 | Low | **维持 Low** | `dbinder_session_object.cpp:44-48`：`Peek()` 为空时仍执行 `socket_ = SOCKET_ID_INVALID`，未改为保留原值 |
| S-05 | Low | **维持 Low** | 两库动态段无 `-Bsymbolic`（本轮 `llvm-readelf -d` 复核：`FLAGS BIND_NOW`，无 `SYMBOLIC`） |
| S-06 | Low | **维持 Low** | loader 侧单条日志；调用点 `ZLOGE("rpc capability is unavailable")` 仍可在 rpc 缺失镜像上被重复触发，属 `design.md:272` 已登记日志放大面 |

### 7. 调用点 fail-closed 复查（12 处）

| 文件:行 | 取值 | 空表行为 | 判定 |
|---|---|---|---|
| `ipc_object_stub.cpp:244` | `Get` | `return IPC_STUB_INVALID_DATA_ERR` | fail-closed |
| `ipc_object_stub.cpp:283` | `Get` | `return IPC_STUB_INVALID_DATA_ERR` | fail-closed |
| `ipc_object_stub.cpp:786` | `Get` | `return ""`（不返回会话名，授权链不继续） | fail-closed |
| `ipc_object_stub.cpp:806` | `Get` | `return IPC_STUB_REMOVE_SESSION_NAME_ERR` | fail-closed |
| `ipc_object_proxy.cpp:1073` | `Get` | `return false` | fail-closed |
| `ipc_object_proxy.cpp:1135` | `Get` | `return ERR_INVALID_DATA` | fail-closed |
| `ipc_object_proxy.cpp:1175` | `Get` | `return ERR_INVALID_DATA` | fail-closed |
| `ipc_process_skeleton.cpp:115` | `Peek` | `ops != nullptr` 才调用 `shutdownListenSocket`，随后清理本地状态 | fail-closed |
| `ipc_process_skeleton.cpp:545` | `Get` | `return networkId`（空串），且取表在 `databusProcMutex_` **之前**（`:544-546`），`dlopen` 不持框架锁 | fail-closed |
| `ipc_process_skeleton.cpp:1322` | `Get` | `return false` | fail-closed |
| `ipc_thread_skeleton.cpp:243` | `Get` | 取表为空则不重建 invoker，`invoker == nullptr` 走 `return nullptr` 既有分支 | fail-closed |
| `dbinder_session_object.cpp:44` | `Peek` | 不解引用；`socket_` 清理属 S-04 | fail-closed |

12 处全部先判空后解引用，返回的都是拆分前该函数已在用的错误码；没有新增「默认成功」「降级放行」分支。

### 8. 阻塞判定（第 2 轮）

- **`blocking_findings = 0`，`verdict = PASS`。** S-01 的两个收口动作中，代码侧已完整落地：绝对路径 + 入口符号来源校验 + fail-closed + 单次日志 + 不重试；`design.md:245-247` 的决议与 `execution-plan.md:745` 的闭环记录同当前实现一致，不存在「文档说做了、代码没做」的偏差。
- 本轮新发现 R2-01～R2-04 均为 Low：两条是可用性/加固（相对路径锚点、ADLT 打包），一条是 dlopen-then-verify 的固有边界，一条是测试负例覆盖不足。它们都**不改变**「谁能提供 `IpcRpcOps`」这个判定，也就不削弱 `grantPermission`/`removePermission`/`getPidAndUidFromServiceName`/`getClientFd` 的完整性。
- S-02（Medium，235 导出、auth 表改写入口可被先序 DSO 抢占）按规则不阻塞，但它仍是本任务**技术面最宽**的遗留项；`ipc_rpc_ops.h` 的登记只解决治理/可追溯性，建议转 TASK-6 体积与导出面收敛时给出量化结论（逐符号说明为何必须导出）。
- 复核证据：本机实跑 `IpcRpcLoaderUnitTest` → `[==========] 14 tests from 1 test suite ran` / `[  PASSED  ] 14 tests`（含真实 `dladdr`+`stat` 的 `PreloadedImageWithEquivalentPathIsAccepted`，二进制 318,656 B，00:35 重建，晚于 loader 源码 00:31）。构建产物 371,584 B（single）/173,168 B（rpc），`libipc_single.z.so` 的未定义符号含 `dladdr dlopen dlsym dlerror stat`，反汇编 `LoadOnce`（`out/x86_64_virt/lib.unstripped/communication/ipc/libipc_single.z.so`）的 `callq` 序列为 `dladdr → dlopen@plt → dlsym@plt → dladdr@plt → bcmp@plt → stat@plt ×2 → callq *%r12`（间接调用 `IpcRpcGetOps`）`→ HiLogPrint`，与源码 `:85-92 → :207 → :250 → :258 → :181 → :144-147 → :264 → :276` 一一对应。

### 9. 未验证项（显式声明）

以下项本轮**没有**验证，不作为通过依据：

1. **设备/模拟器实跑**：本机无 `hdc`、无可用模拟器；未在真实 target 上执行首次 RPC。
2. **`packages/` 实盘安装**：`packages/phone/{system,updater}/lib64/platformsdk/libipc_rpc.z.so` 未产出（`make_packages` 仍被 preheat 遗留 CMakeCache 路径不一致阻塞），同目录安装仅有 module_info 声明级证据。
3. **SELinux 标签继承**：新增库文件的 `u:object_r:*` 标签未实测，仅确认未新增独立路径类别。
4. **ADLT 打包形态**：未验证任何 ADLT 容器场景（R2-02 为源码推演）。
5. **真实「`libdbinder` 先按搜索路径拼写装载 rpc」进程**：单测以「同文件不同拼写」覆盖 `stat` 分支，未在真实 DBinder 进程里观察 `dli_fname` 的实际拼写。
6. **`ld-musl-namespace` 对 `RTLD_LOCAL` 加载的可见性影响**：未在命名空间隔离场景（nDSO/继承 ns）下实跑。
7. **arm64/其它 `target_cpu`**：本轮 musl 行为证据来自源码与 `x86_64_virt` 的 `libc.so`/`libipc_single.z.so`，逻辑与架构无关，但未逐个 target 复核产物。

### 10. 本轮核验记录

| # | 命令/操作 | 输出片段 |
|---|---|---|
| 1 | `grep -rn "IpcRpcOpsGet\|IpcRpcOpsPeek" --include=*.cpp --include=*.h ipc/` | 12 个调用点，行号与第 7 节一致 |
| 2 | `grep -n "HANDLE_RANDOMIZATION" -A 3 third_party/musl/musl_template.gni` | `} else { defines = [ "HANDLE_RANDOMIZATION", "LOAD_ORDER_RANDOMIZATION" ] }`（`:545` 附近） |
| 3 | `sed -n '96,115p' third_party/musl/ldso/linux/dynlink_rand.c` | `gen_handle()` 用 `getrandom`；`assign_valid_handle()` 返回该 handle |
| 4 | `sed -n '6,13p' third_party/musl/src/ldso/dlinfo.c` | `if (__dl_invalid_handle(dso)) return -1; ... *(struct link_map **)res = dso;` |
| 5 | `llvm-objdump -d --disassemble-symbols=dlinfo .../obj/.../libc.so` | `c6070 <dlinfo>` → `callq 0x8e280`；成功分支 `movq %rbx,(%r15)`（handle 直传）；`0x8e280` 读 `0x15b768`（`head`）并 `movq 24(%rax)` 遍历 |
| 6 | `sed -n '6976,6994p' third_party/musl/ldso/linux/dynlink.c` | `addr2dso(addr)`；`dli_fname = p->name`（`adlt` 时为 `p->adlt_ndso_name`） |
| 7 | `sed -n '3077,3088p;3145,3156p;3201,3204p' dynlink.c` | `strchr(name,'/')` → `open(name,...)`；`find_library_by_fstat` 命中只补 `shortname`；`p->name` 来自 `pathname` |
| 8 | `sed -n '7060,7078p;6494,6527p' dynlink.c` | `__dlsym` 经 `find_dso_by_handle`；handle 分支 `use_deps = 1` → `find_sym_by_deps` |
| 9 | `llvm-nm -D --defined-only .../libipc_single.z.so \| wc -l` | `235` |
| 10 | `llvm-nm -D --defined-only .../libipc_rpc.z.so` | `11` 行：`IpcRpcGetOps@@1.0` + 8 个 `DBinderSoftbusClient` + `DBinderDatabusInvoker` C1/C2 |
| 11 | `llvm-readelf -d` 两库 | single：`SONAME libipc_single.z.so`、`FLAGS BIND_NOW`、NEEDED 无 `libipc_rpc`；rpc：`NEEDED libipc_single.z.so`；两者均无 `SYMBOLIC` |
| 12 | `llvm-nm -D -u .../libipc_single.z.so \| grep -E "dladdr\|dlopen\|dlsym\|dlerror\|stat"` | `U dladdr`、`U dlerror`、`U dlopen`、`U dlsym`、`U stat`；且 `grep IpcRpcGetOps` 无命中（核心库不导出入口符号） |
| 13 | `IpcRpcLoaderUnitTest` 宿主实跑（musl loader + `chipset-sdk-sp` 库路径） | `[==========] 14 tests from 1 test suite ran` / `[  PASSED  ] 14 tests` |
| 14 | `awk '/used by libipc_rpc/{f=1} f&&/^[[:space:]]+"/{n++} END{print n}' interfaces/innerkits/ipc_single/libipc_single_map` | `74` |
| 15 | `git diff --stat -- dbinder_softbus_client.cpp databus_socket_listener.cpp dbinder_databus_invoker.cpp` | 空（零改动，权限判定实现未被改写） |
| 16 | `llvm-objdump -d --disassemble-symbols=_ZN4OHOS12_GLOBAL__N_18LoadOnceEv out/x86_64_virt/lib.unstripped/.../libipc_single.z.so \| grep callq` | `dladdr@plt` → `dlopen@plt` → `dlsym@plt` → `dladdr@plt` → `bcmp@plt` → `stat@plt` ×2 → `callq *%r12` → `HiLogPrint@plt`；即「先取自身路径、再打开、再取符号、再校验、最后调用」，且成功路径只打印 1 条日志 |