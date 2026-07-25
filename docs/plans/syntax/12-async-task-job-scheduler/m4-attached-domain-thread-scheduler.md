# Syntax 12 M4: AttachedDomain ThreadScheduler

## 状态与产出记录

- 状态：`completed`（M4 范围；全目标 legacy 基线单列）
- 开始时间：2026-07-25 09:47 +08:00
- 完成时间：2026-07-25 11:52 +08:00
- 完成项目：
  - 分配 `ThreadScheduler`、`Send`、`Sync` 的 canonical protocol id，并以
    resolved Task Scheduler schedule role 驱动 Job argument zero 的唯一 move。
  - 实现 `ThreadScheduler(workerCount)` AttachedDomain provider：worker state
    连接 caller `GcDomain`，prepared Job/Task root 通过 mutex-protected queue
    发布，provider await hook 等待同一个 caller-domain Task completion。
  - 关闭 worker 退出/提交竞争：空队列 worker 在锁内释放 live slot；worker
    创建失败会 fault 并释放未 claim 的 prepared request，不会遗留 pending Task。
  - `Send` / `Sync` generic capability 判定从 imported descriptor protocol mask
    得到 `THREAD_SEND` / `THREAD_SYNC`，不以 short name、qualified text 或
    wrapper name 作为语义后备。
  - 增加 caller-domain attach、单 worker 双 Job queue、non-Send rejection 和
    Job 二次提交 rejection 回归；三工具链均通过 M4 专用断言。
  - 记录 `zr_vm_thread_runtime_test` 的 9 个既有 legacy TaskRunner/isolated
    failures；GCC 的 `zr_vm_task_runtime_test` 54/6 与 M3 snapshot 逐项一致，
    未作为 M4 引入回归。

## 产出位置

- 实施计划：`docs/plans/syntax/12-async-task-job-scheduler/m4-attached-domain-thread-scheduler-implementation-plan.md`
- 验收记录：`tests/acceptance/2026-07-25-syntax-12-m4-attached-domain-thread-scheduler.md`
