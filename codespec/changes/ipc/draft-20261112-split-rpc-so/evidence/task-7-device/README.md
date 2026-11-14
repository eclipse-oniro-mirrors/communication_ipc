# TASK-7 模拟器设备侧验证：复现方法

本目录固化「在 QEMU 模拟器里真机侧运行拆分产物」的全部原始记录，用于把 TASK-6 遗留的
「RSS/PSS 未测」「真实 dlopen 未跑」「端到端只在宿主静态验证」三项未验证内容补齐到运行期口径。

## 1. 环境

| 项 | 值 |
|----|-----|
| 产品 | `x86_64_virt`（`out/x86_64_virt/packages/phone/images`） |
| 宿主 | Linux x86_64，`/dev/kvm` 可用 |
| 模拟器 | `qemu-system-x86_64` 6.2.0（Debian 1:6.2+dfsg-2ubuntu6.31），`-machine q35`，4 vCPU / 4 GiB |
| 客机内核 | `Linux localhost 6.6.101-gc0c10d57367a-dirty #1 SMP PREEMPT_DYNAMIC ... x86_64` |
| 客机版本 | `const.product.software.version = OpenHarmony 7.1.0.25`，`bootevent.boot.completed = true` |
| 连接 | `hdc tconn 127.0.0.1:5555`（x86_64 实例端口为 **5555**，不是 arm64 的 5655） |

启动与连接：

```sh
cd out/x86_64_virt/packages/phone/images
./qemu_run.sh run --instance 00 --display vnc --background
# 等待 bootevent.boot.completed
export PATH=<code>/prebuilts/ohos-sdk/linux/26.1.0/toolchains:$PATH
hdc tconn 127.0.0.1:5555
```

原始信息见 `raw/device_info.txt`。

## 2. 为什么要推送库而不是直接跑镜像内库

`system.img` 内是**拆分前基线件**（`libipc_single.z.so` 492,056 B，md5 `d2ee5266…`，无 `libipc_rpc.z.so`），
因为本轮 `make_packages`/镜像阶段未跑通（见 `evidence/task-6/summary.md` 第 8 节）。
因此采用「推送新构建库 + `LD_LIBRARY_PATH` 优先」的方式让被测二进制加载**本轮拆分产物**：

```sh
R=/data/local/tmp/ipc_rpc_test
hdc shell "mkdir -p $R"
hdc file send out/x86_64_virt/communication/ipc/libipc_single.z.so     $R/
hdc file send out/x86_64_virt/communication/ipc/libipc_rpc.z.so        $R/
hdc file send out/x86_64_virt/communication/ipc/libipc_common.z.so     $R/
hdc file send out/x86_64_virt/communication/ipc/libdbinder.z.so        $R/
hdc file send out/x86_64_virt/communication/ipc/libipc_single_test.z.so $R/
hdc file send out/x86_64_virt/tests/unittest/ipc/ipc/core/<TARGET>     $R/
hdc shell "chmod +x $R/*Test $R/realdl"
```

被测件指纹（md5）见 `manifest.txt`：推送件 md5 与 `out/x86_64_virt` 下构建产物逐条一致，
与系统镜像内基线件 md5 不同，可据此确认「测的是新件」。

设备侧库搜索路径（顺序敏感，先本目录后系统目录）：

```
/data/local/tmp/ipc_rpc_test:/system/lib64/chipset-sdk-sp:/system/lib64/ndk:/system/lib64/platformsdk:/system/lib64
```

- `/system/lib64/chipset-sdk-sp` 提供 `libc++.so`
- `/system/lib64/ndk` 提供 `libffrt.so`
- `/system/lib64/platformsdk` 提供 `libutils.z.so`、`libsec_shared.z.so`、`libhilog.so` 等
- musl loader 固定在 `/lib/ld-musl-x86_64.so.1`（`/system/lib64` 下没有 `ld-musl-*`，不要照搬宿主路径名）

## 3. 运行命令

```sh
cd $R
export LD_LIBRARY_PATH=$R:/system/lib64/chipset-sdk-sp:/system/lib64/ndk:/system/lib64/platformsdk:/system/lib64
timeout 240 ./<TARGET> --gtest_output=xml:$R/<TARGET>.xml
```

`--gtest_output` 必须写可写路径（`/data/local/tmp/...`），不要写系统目录。

## 4. 真实 dlopen 探针

`realdl.c` 是本轮新增的设备侧探针，做三件事：

1. `dlopen(<核心库绝对路径>, RTLD_NOW|RTLD_NODELETE|RTLD_LOCAL)`；
2. `dlsym` 取 `_ZN4OHOS17IPCThreadSkeleton16GetRemoteInvokerEi`；
3. 以 `IF_PROT_DEFAULT(0)` 或 `IF_PROT_DATABUS(1)` 调两次，用 `/proc/self/maps` 判断
   `libipc_rpc.z.so` 是否被映射、用 `/proc/self/smaps_rollup` 的 `Pss` 做同进程前后差。

它走的是**生产代码路径**（`GetRemoteInvoker(IF_PROT_DATABUS)` → `IpcRpcOpsGet()` → `dlopen`），
不是测试桩，因此能验证「首次 RPC 使用才加载、二次使用复用缓存、IPC-only 进程不加载」。

编译（宿主侧，musl 交叉）：

```sh
CC=prebuilts/clang/ohos/linux-x86_64/llvm/bin/clang
M=$PWD/out/x86_64_virt/obj/third_party/musl/usr
$CC --target=x86_64-linux-ohos \
    -isystem $M/include/x86_64-linux-ohos \
    -O2 -fPIC -pie -nostdlib \
    $M/lib/x86_64-linux-ohos/Scrt1.o $M/lib/x86_64-linux-ohos/crti.o \
    realdl.c -L$M/lib/x86_64-linux-ohos -lc $M/lib/x86_64-linux-ohos/crtn.o \
    -Wl,-dynamic-linker,/lib/ld-musl-x86_64.so.1 -o realdl
```

运行：

```sh
./realdl $R/libipc_single.z.so ipc   # 只走 binder，RPC 库必须保持未映射
./realdl $R/libipc_single.z.so rpc   # 触发 DATABUS，RPC 库必须被 dlopen
# 降级场景：目录里只放 libipc_single.z.so，不放 libipc_rpc.z.so
./realdl /data/local/tmp/ipc_only_dir/libipc_single.z.so rpc
```

## 5. 原始文件清单

| 文件 | 内容 |
|------|------|
| `raw/device_info.txt` | 客机内核、产品版本、被测件与基线件 md5 |
| `raw/loader.xml` | `IpcRpcLoaderUnitTest` 16 例 |
| `raw/IpcRpcStaticLinkUnitTest.xml` | 静态链接分支 4 例 |
| `raw/IpcRpcStaticLinkFailureUnitTest.xml` | 静态链接降级 1 例 |
| `raw/IPCProcessSkeletonUnitTest.xml` | 进程骨架 80 例 |
| `raw/IPCObjectProxyUnitTest.xml` | Proxy 64 例 |
| `raw/DbinderDataBusInvokerUnitTest.xml` / `dbinv.log` | DBinder DATABUS invoker 117 例 |
| `raw/stub_no004.xml` / `stub_no004.log` | `IpcObjectStubUnitTest` 排除 `DBinderGetSessionNameTest004` 后 78 例 |
| `raw/IpcObjectStubUnitTest.xml` | 该用例单跑记录（1 例） |
| `raw/stub.log` | 全量 79 例运行到 004 时 abort 的截断日志（块缓冲丢失前段） |
| `raw/cppcrash-IpcObjectStubUnitTest-091255.log` | 上述 abort 的 faultlog（含完整调用栈） |
| `raw/cppcrash-IpcObjectStubUnitTest-091252.log` | 同批次另一份 faultlog |
| `raw/realdl_ipc.txt` | IPC-only 模式输出 |
| `raw/realdl_rpc.txt` | RPC 模式输出 |
| `raw/realdl_rpc_missing_lib.txt` + `raw/realdl_rpc_missing_lib_hilog.txt` | RPC 库缺失降级输出与唯一一条 hilog |
| `realdl.c` | 探针源码 |
| `manifest.txt` | 被测件指纹 |

## 6. 收尾

```sh
cd out/x86_64_virt/packages/phone/images
./qemu_run.sh stop --instance 00 --force
./qemu_run.sh delete --instance 00
```

本 Task 结束时实际只执行了 `stop --force`：实例盘保留在
`~/.local/state/openharmony-qemu/instances/x86_64_linux_full/<id>/00/`，
`/data/local/tmp/ipc_rpc_test/` 内推送件仍在，重新 `run --instance 00` 即可复测而无需重新推送。
如需彻底清理再执行 `delete --instance 00`。

> 注：`out/sdk/clang_x64/developtools/hdc/hdc` 依赖 `libusb_shared.so`，需
> `LD_LIBRARY_PATH=<code>/out/sdk/clang_x64/thirdparty/libusb`；直接用
> `<code>/prebuilts/ohos-sdk/linux/26.1.0/toolchains/hdc` 更省事。