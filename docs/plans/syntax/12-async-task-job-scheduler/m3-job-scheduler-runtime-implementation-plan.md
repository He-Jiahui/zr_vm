# Syntax 12 M3: Job/Scheduler Runtime Contract

## 目标

在唯一的 `zr.task` native descriptor 中建立 cold `Job<T>`、`Scheduler` 与
`schedule/currentScheduler/yieldNow/delay` 的 canonical contract。新路径只消费
protocol id 和 member contract role，不以 `TaskRunner`、`zr.coroutine` 或成员名称
作为语义判断；旧运行时表面仅在 M6 迁移前保持兼容。

## Exact Write Set

| 层 | 路径 | 责任 |
|---|---|---|
| core contract | `zr_vm_core/include/zr_vm_core/object.h`、`zr_vm_common/include/zr_vm_common/zr_contract_conf.h` | 追加 Job/Scheduler protocol id 与 task contract role。 |
| native runtime | `zr_vm_library/src/zr_vm_library/task_runtime.c` | Job constructor、一次性 consume、Scheduler schedule、current scheduler、yield/delay 的 native descriptor/runtime adapter。 |
| tests | `tests/task/test_task_job_scheduler.c`、`tests/CMakeLists.txt` | descriptor ABI、constructor、single consume、Task completion 和 scheduler function contract。 |
| docs | `docs/library-and-builtins/zr-task-job-scheduler.md`、本计划、M3 record、acceptance record | public ledger、legacy boundary、状态和验证证据。 |

## 实施步骤

1. **RED: canonical descriptor**
   - 以独立 task target 固定 `zr.task.Job<T>`、`zr.task.Scheduler`、Job constructor、
     `Scheduler.schedule`、`currentScheduler`、`yieldNow` 和 `delay` 的 generic,
     protocol 与 role metadata。
   - 禁止把 `TaskRunner`、`start`、`pump` 或 `zr.coroutine.Scheduler` 作为新的
     public contract evidence。

2. **Green: cold Job handoff**
   - Job 只由 `init` meta constructor接收 callable 创建，并在 `schedule` 参数
     接收点标为 consumed；第二次提交直接失败，不重新取得 callable。
   - Scheduler 复用现有 local cooperative queue 创建 Task completion handle；Job
     已被提交后 queue 失败也不能恢复其 source owner。`yieldNow` 和 `delay` 只通过
     同一 Task completion ABI 表达协作式完成。

3. **Static and acceptance closure**
   - 在 compiler 的 canonical ownership/type facts 上补 Job non-Copy、schedule
     consume 和 Task must-use 诊断；不以 type-name 或 diagnostic text 匹配。
   - 用 isolated snapshot 完成 GCC/Clang/MSVC focused targets、stdio/CLI smoke，
     更新本记录与 acceptance 后，以 isolated Git index exact-stage 该表路径。

## Completion Checklist

- [x] Job/Scheduler protocol ids and stable native member contract roles are
  published by the single `zr.task` descriptor.
- [x] Job construction, one-time scheduler handoff, and cooperative Task
  completion adapters are implemented without a legacy-name fallback.
- [x] Canonical semantic facts carry native contract roles and ownership
  qualifiers; Scheduler schedule consumes only its exact Job argument.
- [x] Task must-use checking is driven by the Task Handle protocol.
- [x] The exact staged M3 overlay completed its isolated GCC, Clang, and
  MSVC acceptance replay; the resulting exact-path commit records the phase.

## 不在本里程碑内

- ThreadScheduler、Send/Sync、domain attach 和 transfer envelope 属于 M4-M5。
- scheduler close/worker failure 的跨 domain 状态机、artifact/debug/LSP 与删除
  legacy `TaskRunner`/`autoCoroutine` public surface 属于 M4-M6。
- `Duration` 没有既有 canonical owner；M3 只建立 delay 的 task contract，时间
  value provider 在 scheduler/provider milestone中收口，不能用裸数值替代公开 ABI。
