---
related_code:
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership_moves.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - tests/task/test_task_job_scheduler.c
doc_type: module-detail
---

# `zr.task` Job and Scheduler Contract

## Scope

Syntax 12 M3 extends the existing `zr.task` descriptor with the cold
`Job<T>` handoff and cooperative `Scheduler` surface. The descriptor module
version is `2.1.0`. This document describes only the M3 runtime contract; it
does not make `TaskRunner`, coroutine scheduler names, worker threads, or a
numeric duration type part of the new contract.

## Public Contract

- `zr.task.Job<T>` implements the Task Job protocol and is a non-Copy,
  single-consumption value.
- `init Job<T>(callable)` stores a cold callable. It does not queue or run
  work by itself.
- `zr.task.Scheduler` implements the Task Scheduler protocol.
- `Scheduler.schedule(Job<T>)` consumes its first argument and returns a
  `zr.task.Task<T>` completion handle. Reusing the submitted Job is a source
  ownership error.
- `zr.task.currentScheduler()` returns the local cooperative scheduler.
- `zr.task.yieldNow()` and `zr.task.delay(...)` return completion Tasks via
  the same Task ABI. M3 establishes the call contract only; a public Duration
  value provider belongs to a later scheduler/provider milestone.

At runtime, consuming a Job clears its callable even when task-handle
allocation fails. A failed queue handoff therefore cannot restore the source
owner or execute the callable twice.

## Canonical Metadata and Facts

The native descriptor publishes protocol masks and stable member contract
roles for the Job constructor, `Scheduler.schedule`, `currentScheduler`,
`yieldNow`, and `delay`. Parser semantic reference facts carry the resolved
role and ownership qualifier. A native member with a non-zero contract role
is an exact resolved fact even when it has no source declaration `SymbolId`.

The ownership move pass consumes an argument only when the resolved call fact
identifies the Scheduler schedule role at argument zero. It does not inspect
the member spelling, legacy `start`/`pump` methods, or a coroutine owner.

The compiler also rejects a standalone Task expression through the canonical
Task Handle protocol. A Task must be awaited, returned, or stored rather than
silently discarded.

## Compatibility Boundary

The existing Task/frame runtime and legacy `TaskRunner` surface remain in
place for later migration work. M3 neither introduces a thread scheduler nor
changes cross-domain transfer, artifact/AOT metadata, debugger/LSP behavior,
or the legacy public surface deletion planned after the scheduler stack is
complete.
