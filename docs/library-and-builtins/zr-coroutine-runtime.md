---
related_code:
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_reserved_task.c
  - tests/task/test_task_runtime.c
doc_type: module-detail
---

# zr.coroutine Retirement

`zr.coroutine` is no longer a registered builtin module. Syntax 12 M6.2
removed its public `Scheduler`, `coroutineScheduler`, `start`, `step`, `pump`,
and `autoCoroutine` surface.

Use `zr.task.currentScheduler.schedule(Job<T>)` for scheduling and direct
`await` or `Task.result()` for completion. The local cooperative scheduler is
an implementation detail of `zr.task`; it has no separately importable module
identity and no script-level pump control.
