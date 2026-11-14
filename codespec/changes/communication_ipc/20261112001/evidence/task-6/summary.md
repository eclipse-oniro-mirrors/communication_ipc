# TASK-6 体积、依赖与导出证据汇总

**结论：** AC-1.2 两项体积阈值均 **PASS**（文件降幅 24.48% ≥ 10%、`.text` 降幅 28.75% ≥ 20%）；AC-1.1 依赖方向单向成立（`libipc_single` 的 NEEDED 无 `libipc_rpc`，反向依赖成立）；AC-5.1 核心库 `dlopen`/`dlsym` 引用点数与基线持平，未新增加载点。运行期 RSS/PSS 与真实 `dlopen` 场景已由 TASK-7 在 QEMU `x86_64_virt` 模拟器上补齐，见 `../task-7-device/summary.md`；仍未闭环项见第 11 节。

## 1. 口径与可复现性

全部数值来自 `evidence/task-6/summarize.py` 的实测输出，脚本退出码 `EXIT=0`（日志 `/tmp/opencode/task6_summarize_run.log`）。

| 口径 | 定义 | 工具 |
|------|------|------|
| 文件体积 | 安装形态（stripped）`stat -c '%s'` | `stat` |
| `.text` | 代码段大小 | `llvm-size -A` |
| ALLOC | `llvm-size -A` 的 `Total` 减 `.gnu_debugdata`（不参与内存映射） | `llvm-size -A` |
| LOAD MemSiz | `llvm-readelf -l -W` 全部 `LOAD` 段 `MemSiz` 之和（含页对齐） | `llvm-readelf` |
| 导出面 | 动态定义符号条数 | `llvm-nm -D --defined-only \| wc -l` |

> 4 个迁出 TU 为 LLVM bitcode，`llvm-nm -S` 对 `.o` 返回 0，**符号归属统计一律使用 `lib.unstripped/` 下的未剥离 so**。

复现命令（从仓库根）：

```sh
python3 codespec/changes/ipc/draft-20261112-split-rpc-so/evidence/task-6/summarize.py
```

脚本动作：解压 `baseline_libipc_single.z.so.gz` 与 `split_off_libipc_single.z.so.gz` 到 `/tmp/opencode/task6_work/`，重新采集全部原始输出并覆盖写 `baseline.txt` / `after.txt` / `deps.txt` / `symbols.txt`，打印四口径表并做阈值判定；达标退出 0。

冻结件校验：

| 件 | md5 | 来源 |
|----|-----|------|
| `baseline_libipc_single.z.so.gz` | `d2ee52661cd246e2a986643de9ba0426` | `out/x86_64_virt/packages/phone/system/lib64/platformsdk/libipc_single.z.so`（mtime 2026-11-12 01:29，pre-change） |
| `split_off_libipc_single.z.so.gz` | `f4f1df609aa7dea6643500fb6abf8aea` | `ipc_feature_rpc_split_enabled=false` 构建件（TASK-4 关闭态，`/tmp/opencode/split_off_libipc_single.z.so`） |

## 2. 四口径体积表

| 对象 | 文件 (B) | `.text` (B) | ALLOC (B) | LOAD MemSiz (B) |
|------|---------:|------------:|----------:|----------------:|
| baseline（拆分前 `libipc_single`） | 492,056 | 289,951 | 467,177 | 467,892 |
| on-single（拆分后 `libipc_single`） | 371,584 | 206,579 | 353,720 | 354,444 |
| on-rpc（新增 `libipc_rpc`） | 177,304 | 92,045 | 163,256 | 163,980 |
| off-single（开关关闭态 `libipc_single`） | 488,464 | 282,181 | 468,467 | 469,180 |
| on-test-so（`libipc_single_test`，仅构建期参考） | 606,944 | 318,613 | 593,663 | 594,380 |
| on-dbinder（`libdbinder`，参考） | 166,624 | 76,642 | 158,543 | 159,260 |

> on-rpc 数值为 U-1 修正后产物；U-1 前旧记 `.text` 90,717 / ALLOC 161,464 已作废。

## 3. AC-1.2 阈值判定

| 指标 | 阈值 | 实测 | 结论 |
|------|------|------|------|
| 核心库文件体积降幅 | ≥ 10% | `492,056 → 371,584`，**−120,472 B / −24.48%** | **PASS** |
| 核心库 `.text` 降幅 | ≥ 20% | `289,951 → 206,579`，**−83,372 B / −28.75%** | **PASS** |

补充口径（不替代阈值判定，仅供评审判断）：

| 场景 | ALLOC | 相对基线 |
|------|-------|----------|
| 仅 IPC 进程（`libipc_single`） | 467,177 → 353,720 | **−24.29%** |
| RPC 进程（`libipc_single` + `libipc_rpc`） | 467,177 → 516,976 | **+10.66%** |
| 关闭开关态 vs 基线 | 文件 −3,592 / `.text` −7,770 / ALLOC +1,290 | 略高于基线（entry TU + 导出变化） |

**取舍说明：** 只使用 IPC 的进程节省约 1/4 的 IPC so 映射量；使用 RPC 的进程净增约 10.66%（RPC 能力从原核心库拆到独立库后，两库合计仍略大于原单库，因跨库调用与导出表开销）。这与需求「让只使用 ipc 的模块内存占用更低」一致。

## 4. AC-1.1 依赖方向

`deps.txt` 实测：

| 检查 | 结果 |
|------|------|
| `libipc_single` NEEDED 含 `libipc_rpc.z.so` | **NO** |
| `libipc_rpc` NEEDED 含 `libipc_single.z.so` | **YES** |
| `libdbinder` NEEDED 含 `libipc_rpc.z.so` | **YES** |

依赖链单向：`libdbinder → libipc_rpc → libipc_single`（`libipc_rpc` 另需 `libipc_common`）。核心库对 RPC 库**无编译期依赖**，仅由 `dlopen` 在首次 RPC 使用时按需加载。

核心库 ABI 层面复核（`symbols.txt`）：

| 对象 | `nm -D` 命中 `IpcRpc` | `readelf -r` 命中 `IpcRpc` |
|------|----------------------|---------------------------|
| on-single | 0 | 0 |
| off-single（关闭态） | 0 | 0 |
| baseline | 0 | 0 |

开启态与关闭态均无 `IpcRpc*` 符号/重定位残留，说明编译期不存在对 RPC 库的引用。

## 5. 入口可见性与导出面

| 对象 | 导出条数 | `IpcRpcGetOps` |
|------|---------:|----------------|
| baseline(single) | 167 | 0 |
| on-single | 235 | 0 |
| on-rpc | 11 | **1（`IpcRpcGetOps@@1.0`）** |
| off-single | 245 | 0 |

- `libipc_rpc.z.so` 按 `libipc_rpc_map` 逐条枚举导出 **11 条**，其中 `IpcRpcGetOps` 1 条，其余 10 条为迁出 TU 对外可见的类方法。
- 核心库导出面 `167 → 235`（**+68**）；关闭态 `167 → 245`（**+78**）。增量来源：核心库需向 `libipc_rpc` 提供被调用的符号，以及关闭态多出 entry TU 的 `IpcRpcGetOps`。此代价已在 `design.md` 风险表、`config.gni` 注释与 `execution-plan.md` TASK-6 体积口径中登记为**已接受取舍**。
- 关闭态 `IpcRpcGetOps` 不导出（0 条）：静态链接模式下入口 TU 走 `IPC_RPC_STATIC_LINK` 分支，符号被隐藏。
- 测试 so（`libipc_single_test`）导出面 `839 → 839`：`+IpcRpcGetOps`、`−_ZGVN4OHOS21DBinderDatabusInvoker24DBinderDatabusDelegator_E`（U-1 把 delegator 守护变量从 header 内联改为唯一定义，guard 变量不再导出）。

## 6. AC-5.1 热路径加载点

`symbols.txt` 实测核心库动态未定义符号中 `dlopen`/`dlsym` 条数：

| 对象 | 条数 | 明细 |
|------|-----:|------|
| baseline | 2 | `U dlopen`、`U dlsym` |
| on-single | 2 | `U dlopen`、`U dlsym` |
| off-single | 2 | `U dlopen`、`U dlsym` |

**与基线持平，未新增加载点。** 符号层面无法区分调用点数量（同名符号合并为 1 个未定义引用），因此补充源码级核查：

| 来源文件 | 用途 | 是否新增 |
|----------|------|----------|
| `ipc/native/src/core/framework/source/ipc_trace.cpp` | hitrace 可选动态加载 | 既有 |
| `ipc/native/src/core/framework/source/ipc_thread_pool.cpp` | 取自身 so 句柄 | 既有 |
| `ipc/native/src/core/dbinder/source/ipc_rpc_loader.cpp` | 首次 RPC 使用加载 `libipc_rpc` | **新增（唯一新增调用点）** |

加载点为惰性单次触发：`IpcRpcOpsGet()` 用 `std::call_once` 保证只尝试一次，失败后缓存空表、不再重试；未进入 RPC 路径的进程不会触碰该调用点。`ipc_process_skeleton.cpp:543` 附近仅为注释说明，不构成调用点。

## 7. 迁出 TU 符号归属

`symbols.txt`（未剥离件 `--defined-only`，含 internal）：

| 符号族 | `libipc_single` | `libipc_rpc` |
|--------|----------------:|-------------:|
| `DBinderDatabusInvoker` | 0 | 78 |
| `DBinderSoftbusClient` | 0 | 17 |
| `DatabusSocketListener` | 0 | 33 |
| `DBinderSessionObject` | 31 | 71 |
| `DBinderCallbackStub` | 27 | 0 |
| `IpcRpcGetOps` | 0 | 1 |

- 4 个迁出 TU 的符号已全部离开核心库。
- `DBinderSessionObject` 在 rpc 库出现 internal（本地）副本，是 rpc TU 内部引用其头文件所致，不构成核心库反向依赖。
- `DBinderCallbackStub`（27 条）与 `DBinderSessionObject`（31 条）在核心库中保留，符合 AC-2.2「会话对象与死亡通知桩留在核心库」的静态半边要求。

## 8. 安装声明

产物安装声明 `out/x86_64_virt/obj/foundation/communication/ipc/interfaces/innerkits/ipc_core/ipc_rpc_core_module_info.json`：

```json
{
  "collect": false,
  "dest": [
    "system/lib64/platformsdk/libipc_rpc.z.so",
    "updater/lib64/platformsdk/libipc_rpc.z.so"
  ],
  "install_enable": true,
  "source": "obj/foundation/communication/ipc/interfaces/innerkits/ipc_core/out/x86_64_virt/communication/ipc/libipc_rpc.z.so"
}
```

`libipc_rpc.z.so` 与 `libipc_single.z.so` 同目录（`platformsdk`），system 与 updater 两处安装，`install_enable = true`。

**未验证：** `packages/phone/{system,updater}/lib64/platformsdk/libipc_rpc.z.so` 实盘文件不存在。`packages/` 与镜像只在 `make_packages`/镜像阶段写入，本轮 `make_packages` 被 preheat 遗留的 `/mnt/disk/workspace/preheat_code_prod/...` CMakeCache 路径不一致打断。`packages/` 内仍是拆分前陈旧副本（`libipc_single.z.so` 492,056 B @11-12 01:29），**一律不作验收依据**；验收以 `out/x86_64_virt/communication/ipc/` 与 `lib.unstripped/` 产物 + `module_info.json` 安装声明为准。

## 9. Fuzz 目标影响

`DBinderGrantPermissionFuzzTest` 与 `GetLocalNodeDeviceIdFuzzTest` 原先 `deps` 指向共享库 `ipc_single`，拆分后 `DBinderSoftbusClient` 不再由 `libipc_single` 动态导出，故改为静态聚合目标 `test:ipc_single_test_static`（与同目录其它 fuzzer 一致），构建配置见 `test/fuzztest/ipc/native/core/{dbindergrantpermission,getlocalnodedeviceid}_fuzzer/BUILD.gn`。

| 目标 | 拆分前体积 | 拆分后体积 | 变化 | 构建 |
|------|-----------:|-----------:|-----:|------|
| `DBinderGrantPermissionFuzzTest` | 596,216 B（11-13 00:33，TASK-5 装配前） | **725,456 B**（11-13 02:17） | +21.68% | `EXIT=0`，`/tmp/opencode/task6_fuzz_rebuild.log` |
| `GetLocalNodeDeviceIdFuzzTest` | 596,240 B（11-13 00:33） | **725,536 B**（11-13 02:18） | +21.69% | `EXIT=0`，`/tmp/opencode/task6_fuzz2_rebuild.log` |

`DBinderGrantPermissionFuzzTest` 未剥离件 `.text` = 486,054 B、`Total` = 8,012,039 B；二进制内含 `IpcRpc*` 符号 3 条（`IpcRpcOpsGet`/`IpcRpcOpsPeek`/`IpcRpcOwnLibraryMarker`），证实 RPC 实现已静态并入测试件。

**体积上升属预期且可接受：** fuzzer 此前依赖共享库、只链接所需符号；改为静态聚合后整份 RPC 实现进入测试二进制。fuzzer 是测试件，不参与设备内存占用核算；此处结论仅为「拆分未破坏 fuzz 目标可构建性，且 RPC 代码路径在测试件内可达」。

## 10. 已知变量承接（TASK-3 评审转入）

| 编号 | 内容 | 本 Task 处置 |
|------|------|--------------|
| A3 | 核心库新增私有导出使其内部调用退化为 PLT 调用 | 导出面 `+68`（开启）/`+78`（关闭）已量化；未做 PLT 计数优化，接受为「导出面换拆分」代价 |
| A4 | `ipc_rpc` 未继承 arm64 专用链接选项（`--emit-relocs`、`--no-relax`、`-mno-fix-cortex-a53-843419`） | 本机 `x86_64_virt` 无法实证，记为 **arm64 待验证项** |
| A7 | `ipc_single_test` 的 `common_source` 命名与语义（测试全量集合）不符 | 只在依赖说明中注明，重命名留后续清理 |
| S-02/S-05 | 核心库导出面 167→235；两库均无 `-Bsymbolic` | 已作为结论背景记录（见第 5 节） |
| R2-03 | 加载校验在 `dlopen` 之后，被拒映像仍驻留 | LOAD MemSiz 结论按「正常路径」口径给出，不覆盖恶意同名库场景 |

## 11. 未验证项

> 本表为 TASK-6 结项时的状态。TASK-7 已在 QEMU `x86_64_virt` 模拟器上补齐运行期验证，
> 相关结论见 `../task-7-device/summary.md`；仍未闭环的项在「现状」列标注。

| 项 | 原因 | 现状 | 后续动作 |
|----|------|------|----------|
| RSS/PSS 真机测量 | TASK-6 时本机无 `hdc` 连接设备、无 `qemu-system-x86_64` 可用 | **已在模拟器完成**：IPC-only 进程 ΔPSS +662 KB、RPC 进程 +831 KB、差值 169 KB ≈ rpc 库体积（`../task-7-device/summary.md` 第 3 节） | 真实系统服务进程（`samgr`/`softbus_server`）PSS 仍待真机测量 |
| 真实 `dlopen`/`dlsym` 加载 `libipc_rpc.z.so` 的场景 | 无运行期用例；只有产物级证据（NEEDED、符号、`module_info.json`） | **已在模拟器完成**：`realdl` 三场景（IPC-only 不加载 / 首次 DATABUS 加载并缓存 / 缺库降级）均符合预期（`../task-7-device/summary.md` 第 3、4 节） | — |
| 关闭开关态「字面运行」 | 需切开关全量重建；本轮只冻结产物做段级测量 | 仍未验证；仅关闭态构建 + `IPC_RPC_STATIC_LINK` 等价分支设备侧 4/4 + 1/1 通过 | 可选补强：切开关全量重建后跑全量用例 |
| `packages/` 实盘安装 | `make_packages` 被 preheat CMakeCache 路径不一致打断 | 仍未验证：模拟器 `system.img` 内仍是基线件，设备侧用推送件替代 | 清理 out 后重建镜像阶段校验 |
| arm64 链接选项一致性（A4） | 本机仅 `x86_64_virt` | 仍未验证 | arm64 构建时核对 |
| 端到端死亡通知/权限链路运行期 | TASK-6 时需设备 | 部分覆盖：`DbinderDataBusInvokerUnitTest` 117/117 设备侧通过；双端跨设备（SoftBus 组网）远端退出/权限判定未执行 | 需双端设备执行 |