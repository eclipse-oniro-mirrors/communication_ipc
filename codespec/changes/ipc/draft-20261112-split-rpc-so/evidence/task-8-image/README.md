# TASK-8 整机镜像集成验证（x86_64_virt）：复现方法

本目录补上 TASK-7 遗留的唯一硬缺口：**被测库不是推送件，而是真正从本轮构建出的 `system.img` 里加载**。
验证目标是把「源码/单测/静态口径通过」提升到「整机镜像内 `libipc_single` 不再包含 RPC 符号、RPC 能力由同目录
`libipc_rpc.z.so` 在首次使用时就地 `dlopen`」这一运行期事实。

## 1. 为什么必须重开实例

镜像里的 `system.img` 变了，而实例盘是 qcow2 overlay（backing file 指向上一次构建的 `system.img`）。
`qemu_run.sh` 的 `validate_instance` 会比对 base image 的 `(size, mtime/inode…)` 签名，签名不一致会直接拒绝启动并提示
`base image changed … reset is required`。因此本次用：

```sh
./qemu_run.sh run --instance 00 --display vnc --background --reset-before-run
```

`--reset-before-run` 会丢弃该实例的历史写入，**同时也清掉了 TASK-7 遗留在 `/data/local/tmp/ipc_rpc_test/` 的推送库**，
这正是本次要的干净状态：设备上再也找不到能「顶替」镜像库的副本。

## 2. 构建与镜像产物

```sh
cd <code>
./build.sh --product-name x86_64_virt --ccache --deps-guard=false
# → x86_64_virt build success，Cost Time: 1:40:07
# → out/x86_64_virt/packages/phone/images/*.img 全部重建
```

镜像时间戳（`system.img` 2,147,483,648 B）与库指纹见 `manifest.txt`、`raw/elf.txt`。

## 3. 设备侧零推送库运行

```sh
export PATH=<code>/prebuilts/ohos-sdk/linux/26.1.0/toolchains:$PATH
hdc tconn 127.0.0.1:5555           # x86_64 实例端口 5555
R=/data/local/tmp/imgverify
hdc shell "mkdir -p $R"
hdc file send realdl $R/realdl
# 7 个测试可执行 + libipc_single_test.z.so 也只需推送到 $R；核心库一律不推
hdc shell "chmod +x $R/*UnitTest $R/realdl"
```

库搜索路径（注意**没有** `$R` 里的核心库，只有一个测试桩库）：

```sh
export LD_LIBRARY_PATH=$R:/system/lib64/chipset-sdk-sp:/system/lib64/ndk:/system/lib64/platformsdk:/system/lib64
```

### 真实 dlopen 探针

```sh
./realdl /system/lib64/platformsdk/libipc_single.z.so ipc   # 只走 binder，RPC 库必须保持未映射
./realdl /system/lib64/platformsdk/libipc_single.z.so rpc   # 触发 DATABUS，RPC 库应被 dlopen
```

探针源码与原理见 `../task-7-device/realdl.c`：`dlopen` 核心库 → `dlsym`
`_ZN4OHOS17IPCThreadSkeleton16GetRemoteInvokerEi` → 以 `IF_PROT_DEFAULT/DATABUS` 调用 → 用 `/proc/self/maps`
判定 `libipc_rpc.z.so` 是否映射。它走的是**生产加载路径**，不是测试桩。

### 测试目标

```sh
cd $R
export LD_LIBRARY_PATH=$R:/system/lib64/chipset-sdk-sp:/system/lib64/ndk:/system/lib64/platformsdk:/system/lib64
timeout 600 ./<TARGET> --gtest_output=xml:$R/<TARGET>.xml
```

## 4. 原始文件清单

| 文件 | 内容 |
|------|------|
| `raw/device_info.txt` | 客机内核/版本、镜像内四个库的 `ls -l` + `md5sum` |
| `raw/elf.txt` | 从 `system.img` dump 出的四库 ELF 事实：size/md5/架构/SONAME/NEEDED/导出数，以及 `debugfs` inode 映射 |
| `raw/realdl_ipc.txt` | IPC-only 模式：`RPC_MAPPED_AFTER=0` |
| `raw/realdl_rpc.txt` | RPC 模式：4 段 `libipc_rpc.z.so` 映射，路径为镜像内 `/system/lib64/platformsdk/` |
| `raw/realdl_rpc_missing_lib.txt` + `raw/realdl_rpc_missing_lib_hilog.txt` | 同目录只放核心库时降级为 `nullptr` + 单条日志 |
| `raw/IpcRpcLoaderUnitTest.xml` | 加载器 16 例 |
| `raw/IpcRpcStaticLinkUnitTest.xml` | 静态链接分支 4 例 |
| `raw/IpcRpcStaticLinkFailureUnitTest.xml` | 静态链接降级 1 例 |
| `raw/ps_1.xml` / `raw/ps_1.log` | 进程骨架 80 例（通过轮） |
| `raw/IPCProcessSkeletonUnitTest_first_abort.log` | 进程骨架首轮既有偶发 abort（EXIT=134），记录用 |
| `raw/IPCObjectProxyUnitTest.xml` | Proxy 64 例 |
| `raw/DbinderDataBusInvokerUnitTest.xml` / `raw/dbinv.log` | DBinder DATABUS invoker 117 例 |
| `raw/stub_1.xml` / `raw/stub_1.log` | `IpcObjectStubUnitTest` 79 例（通过轮） |
| `raw/IpcObjectStubUnitTest_flaky_abort.log` | 同目标既有偶发 abort（EXIT=134），记录用 |
| `manifest.txt` | 镜像内库 vs 构建产物 md5 对照 + 推送件指纹 |

## 5. 收尾

```sh
cd out/x86_64_virt/packages/phone/images
./qemu_run.sh stop --instance 00 --force
```

实例盘保留在 `~/.local/state/openharmony-qemu/instances/x86_64_linux_full/<image-key>/00/`，
`/data/local/tmp/imgverify/` 内的推送件仍在，重启同一实例可复测而无需重新推送。