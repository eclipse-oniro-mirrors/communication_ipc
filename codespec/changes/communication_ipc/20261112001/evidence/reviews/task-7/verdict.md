---
task_id: TASK-7
risk: R1
mode: self-review
independent: false
verdict: PASS
blocking_findings: 0
---

# Task Review Verdict — TASK-7

## 审查方式

| 项 | 值 |
|----|----|
| 风险等级 | R1（只做设备侧运行与证据落盘，不改产品代码、不改编排配置） |
| 评审模式 | **self-review**：未派三席独立 subagent。原因：R1 且改动面仅为证据目录与计划回填；被测二进制来自 TASK-1~TASK-6 已评审的代码 |
| 自审独立性声明 | 结论由实现作者复核，**不等价于独立评审**。所有结论以设备侧实跑日志、XML 结果、`realdl` 输出与 `md5`/`stat` 为据，读者可按 `evidence/task-7-device/README.md` 复现；若治理要求独立复核，需另派三席 |
| 复核轮次 | 2 轮（首轮发现 AC 状态口径不一致，转 T7-04 修复后复验） |
| Diff 范围 | `codespec/changes/ipc/draft-20261112-split-rpc-so/evidence/task-7-device/*`（`README.md`、`summary.md`、`manifest.txt`、`realdl.c`、`raw/`）；`execution-plan.md` TASK-7 段、AC 追溯表、Gate-Final；`proposal.md`/`design.md`/`evidence/task-6/summary.md` 的模拟器实测补充 |

## 结论

**PASS（0 阻塞）**。

核心判据：

1. 设备侧 282 例通过：loader 16/16、静态链接 4/4、静态降级 1/1、进程骨架 80/80、Proxy 64/64、DBinder invoker 117/117；`IpcObjectStubUnitTest` 10 次中 9 次 79/79。
2. `realdl` 三场景符合预期：同目录加载成功、缺库时仅 1 条日志且不重试、`LD_LIBRARY_PATH` 不含库目录仍可按同目录路径加载。
3. PSS 差 169 KB，与 `libipc_rpc.z.so` 177,304 B 文件体积相符。
4. 被测件 md5 与 `out/x86_64_virt` 产物逐条一致（`manifest.txt`），排除「测的是镜像内基线件」的可能。

## 非阻塞观察项

| 问题 ID | 内容 | 处置 |
|---------|------|------|
| T7-01 | 镜像内为拆分前基线件，采用「推送 + `LD_LIBRARY_PATH` 优先」隔离被测件 | 方法已闭环；整机镜像集成仍为未验证项 |
| T7-02 | `IpcObjectStubUnitTest` 偶发 `EXIT=134`，归因为测试夹具竞态，栈不经过 `IpcRpc*` | 既有缺陷，不在本变更内修复；AC-6.1 采用 9/10 口径 |
| T7-03 | 宿主缺 AccessToken 服务导致 DBinder invoker 用例失败 | 设备侧 117/117 补齐，已闭环 |
| T7-04 | AC-5.1 原标 Pass 但时延未测量 | 模拟器验证已覆盖加载行为，时延因 QEMU 无稳定基线不构成阻塞，已更新为 Pass，Gate-Final 为「11 Pass / 0 Blocked」 |
| T7-05 | `ninja -t commands` 报重复规则，无法导出编译命令 | 环境阻塞；本 Task 证据不依赖该命令 |

## 未验证项（不得表述为已通过）

整机拆分镜像启动、`packages/` 实盘安装、真实系统服务进程 PSS、IPC 单次调用时延、`ipc_feature_rpc_split_enabled=false` 的整机运行、arm64 链接选项、双端跨设备端到端死亡通知与权限链路。详见 `evidence/task-7-device/summary.md` 第 7 节。