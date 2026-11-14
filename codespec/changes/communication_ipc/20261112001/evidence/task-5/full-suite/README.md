# TASK-5 全量用例运行原始证据

本目录是 TASK-5 scope 席独立复跑「`out/x86_64_virt/tests/unittest/ipc` 下全部 59 个测试二进制」的原始记录，用于回应 scope 评审发现 S-01（作者只跑了 6 个二进制，AC-6.1「全部通过」口径不完整）。

## 运行方法（原样固化，供复现）

```bash
OUT=/srv/workspace/openharmony_master_default_x86_64_virt_ai_20261111185909_huawei_c9e7dfc75/code/out/x86_64_virt
SEARCH=$(sed "s|^\.|$OUT|" /tmp/opencode/sodirs2.txt | tr '\n' ':')   # 317 个部件产出目录
LD=$OUT/common/common/libc/ld-musl-x86_64.so.1
cd <测试二进制目录>
LD_LIBRARY_PATH="<dir>/resource/communication/ipc:$SEARCH$OUT/packages/phone/system/lib64:$OUT/packages/phone/system/lib64/chipset-sdk-sp:$OUT/common/common" \
  timeout 90 "$LD" ./<测试二进制>
```

- `LD_LIBRARY_PATH` 顺序是**本仓部件产出优先、`packages/phone/system/lib64` 在后**。原因是 `packages/phone/system/lib64` 中的安装件为陈旧副本（`libdbinder.z.so` 2026-11-12 01:52，`DT_NEEDED` 无 `libipc_rpc`；`libipc_single.z.so` / `libipc_rpc.z.so` 缺失），若把它放在前面，依赖新 `libdbinder` 的用例会 rc=127。该顺序本身也是 S-07 要求固化的前提。
- `/tmp/opencode/sodirs2.txt` 是 317 个部件产出目录清单，生成命令：从 `out/x86_64_virt` 下排除 `lib.unstripped/packages/obj/clang_x64/innerkits/tests/common/ndk` 后收集全部 `.so` 所在目录。

## 结果分布

- 59 个二进制：**47 个 rc=0**，12 个非零（明细见 `summary.txt`）。
- 非零项已逐项归因环境，**无一归因本次拆分**：
  - 无 `/dev/binder` 驱动：`BinderInvokerUnitTest`（core/common）、`BinderConnectorUnitTest`、`IPCNativeUnitTest`。
  - 无 access token service / SAMgr：`IpcCApiUnitTest`、`DbinderDataBusInvokerUnitTest`、`IPCNapiUnitTest`。
  - fd / ashmem 限制：`MessageParcelAddUnitTest`、`IPCFileDescOpsUnitTest`。
  - 线程池上限：`IPCMockUnitTest`。
  - gmock 自身 leak 报告：`IPCCskeletonUnitTest`（17/17 用例 PASS）。
  - binder 驱动缺失导致 parcel 回读为空：`DBinderServiceDeathRecipientUnitTest`（706/701 命中 `dbinder_service_stub.cpp` 的 `object == nullptr` 分支；该实现与用例文件在本变更集内 `git diff` 为空，路径不经过 `IpcRpcOps`）。
- 12 个非零项各自的完整输出日志一并保存于本目录。

## 与 AC-6.1 的口径关系

AC-6.1 的「全部通过」在本变更中的准确含义是：

1. **TASK-5 目标二进制全部通过**：`IpcRpcStaticLinkUnitTest` 4/4、`IpcRpcStaticLinkFailureUnitTest` 1/1、`IpcRpcLoaderUnitTest` 16/16、`IPCProcessSkeletonUnitTest` 80/80、`IPCObjectProxyUnitTest` 64/64、`IpcObjectStubUnitTest` 79/79（TASK-2 登记的 13 个既有失败用例已转绿）。
2. **仓内 IPC 全量 59 个二进制**：除上表环境豁免项外全部通过；豁免项在拆分前同样依赖真机驱动/服务，本机无法提供。

因此批次结论应表述为「除环境豁免项外全部通过」，不得表述为「59/59 全部通过」。