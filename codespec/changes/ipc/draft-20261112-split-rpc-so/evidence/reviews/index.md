# Task Review Evidence Index

本目录按 Task 分目录保存 R2 三席评审与汇总结论；Task 内证据文件命名固定为 `arch.md`、`scope.md`、`security.md`、`verdict.md`。

| Task | 风险 | 三席证据 | 汇总结论 | 阻塞发现 |
|------|------|----------|----------|----------|
| TASK-1 | R2 | `task-1/arch.md`、`task-1/scope.md`、`task-1/security.md` | `task-1/verdict.md`：PASS | 0 |
| TASK-2 | R2 | `task-2/arch.md`、`task-2/scope.md`、`task-2/security.md` | `task-2/verdict.md`：PASS | 0 |
| TASK-3 | R2 | `task-3/arch.md`（3 轮）、`task-3/scope.md`（2 轮）、`task-3/security.md`（2 轮） | `task-3/verdict.md`：PASS | 0（B-1 / S-01 已闭环） |
| TASK-4 | R2 | `task-4/arch.md`、`task-4/scope.md`、`task-4/security.md` | `task-4/verdict.md`：PASS（评审后定向修订 A-01/A-04/A-05/A-08/S-03/SC-02） | 0（A-02/S-02/SC-04 转 TASK-5；A-03/S-01/SC-01 已登记接受） |
| TASK-5 | R2 | `task-5/arch.md`、`task-5/scope.md`、`task-5/security.md` | `task-5/verdict.md`：PASS（评审后定向修订 A-01/A-04/SC-03 + S-01/S-03/S-07 证据回填） | 0（A-02/S-02/SC-01 为真实 `dlopen` 未验证，转 TASK-6 并限制 AC-6.1 口径；A-03/S-05/SC-02 转 TASK-6；A-05/A-06/SC-04/SC-05 接受并登记） |
| TASK-6 | R0 | 未派三席（R0，self-review） | `task-6/verdict.md`：PASS（0 阻塞；登记 T6-01~T6-05 观察项，其中 T6-05 已修复） | 0 |
| TASK-7 | R1 | 未派三席（R1，只做设备侧运行与证据落盘） | `task-7/verdict.md`：PASS（0 阻塞；T7-01~T7-05 为非阻塞观察项，已在文件内逐条登记） | 0（未验证项已单列） |

TASK-7 为 R1 设备侧验证（QEMU `x86_64_virt`），不派三席评审，采用 **self-review** 并在 `task-7/verdict.md` 记录结论；独立性限制与未验证项在 `evidence/task-7-device/summary.md` 第 7 节显式披露。

TASK-6 为 R0 收尾（体积/安装/汇总），不派三席评审，采用 **self-review** 模式并在 `task-6/verdict.md` 显式披露独立性限制；后续 Task 在验证通过后按同一结构追加。

TASK-5 全量运行证据（59 个测试二进制，47 个 rc=0）固化在 `evidence/task-5/full-suite/`。