# Syntax 12 M3: Job/Scheduler Runtime Contract

## 状态与产出记录

- 状态：`completed`
- 开始时间：2026-07-25 06:55 +08:00
- 完成时间：2026-07-25 09:42 +08:00
- 完成项目：
  - 建立 `zr.task.Job<T>` / `Scheduler` 的 protocol 和 native contract-role
    ABI；Job constructor、schedule、currentScheduler、yieldNow 与 delay 都不从
    legacy TaskRunner/coroutine 名称推导。
  - Job 由 canonical ownership facts 标为 Unique，schedule 的 argument zero
    仅在 resolved Scheduler schedule role 下消费；Task Handle protocol 同时
    关闭 discarded Task source path。
  - 在 `c497860 + M3 exact staged overlay + initialized submodules` 隔离快照上
    完成 GCC 11.4、Clang 14、MSVC 19.44 各 5/5 Job/Scheduler tests，实际进程
    均 exit 0；GCC `zr_vm_cli --help` smoke 也 exit 0。
  - 最终 replay 未产生 M3 新 warning。快照仍保留 legacy TaskRunner/await
    descriptor initializer、parser helper 和 zrm const 限定的既有编译 warning，
    不作为本里程碑通过证据。

## 产出位置

- 实施计划：`docs/plans/syntax/12-async-task-job-scheduler/m3-job-scheduler-runtime-implementation-plan.md`
- 验收记录：`tests/acceptance/2026-07-25-syntax-12-m3-job-scheduler-runtime.md`
- module 文档：`docs/library-and-builtins/zr-task-job-scheduler.md`
