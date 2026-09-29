---
related_code:
  - zr_vm_core/include/zr_vm_core/execution_context.h
  - zr_vm_core/include/zr_vm_core/call_info.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/src/zr_vm_core/execution/execution_safepoint.c
  - zr_vm_core/src/zr_vm_core/execution/execution_cold.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/execution_context.h
  - zr_vm_core/src/zr_vm_core/execution/execution_safepoint.c
  - zr_vm_core/src/zr_vm_core/execution/execution_cold.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
tests:
  - tests/core/test_ssa_dispatch_boundaries.c
  - tests/acceptance/2026-09-28-ssa-dispatch-safepoint-poll.md
plan_sources:
  - docs/plans/ssa/03-interpreter-binding/01-dispatch-boundaries.md
doc_type: module-detail
---

# Interpreter state boundaries

The interpreter keeps hot dispatch values in registers, but garbage collection,
native callbacks, debugging, exceptions, and suspension can invalidate those
registers. `SZrCallInfo` and `SZrState` remain the GC-visible authority:

- `callInfo->context.context.programCounter` is the resumable program counter.
- `callInfo->functionBase` and `functionTop` describe the active frame.
- `state->stackTop` describes the current state stack top and may temporarily be
  higher than the frame top for meta-call scratch storage.

`ZrCore_Execution_PublishBoundary` writes the continuation PC before entering a
boundary. It validates that the PC belongs to the current function (the
one-past-end address is accepted for terminal control flow) and snapshots the
three stack roles separately. `ZrCore_Execution_ReloadBoundary` then resolves
the current call frame and rebuilds all derived values from the GC-visible
state. Failed validation returns an explicit status and does not partially
replace the context.

`ZrCore_Execution_SafepointPoll` combines publish, mutator polling, and reload.
It returns `ZR_EXECUTION_BOUNDARY_OK` when the mutator did not park,
`ZR_EXECUTION_BOUNDARY_RELOADED` after a completed pause and successful reload,
and propagates validation errors. The interpreter calls it at the existing
256-fetch poll: before polling, dispatch stores the resume PC and frame top;
when no pause occurs, the hot local state continues unchanged. After a pause,
dispatch takes the current `callInfo` from the reloaded context and resumes
through `LZrReturning`, which rebuilds the instruction and frame locals from the
root-visible frame. A failed boundary is raised through the VM runtime-error
path with the boundary status name. Execution-budget polling remains a separate
operation after the mutator poll.

Any operation that can grow or move the VM stack must use the existing
`SZrFunctionStackAnchor` APIs; a cached frame base is never valid across such an
operation. Exception or budget unwinding may replace or remove the call frame,
so a reload must not dereference the old `SZrCallInfo` after unwinding.

This first boundary layer intentionally does not bind ExecIR layouts or active
call-binding generations. Those invariants are introduced by the later binding
guard stages. Bytecode dispatch continues to use the single instruction list
from `zr_instruction_conf.h` for both computed-goto and switch builds.

The focused dispatch test runs a 257-NOP function and checks that the saved
resume PC matches `state->previousProgramCounter` at the 256-fetch poll. Direct
reload coverage also simulates replaced call-frame, instruction, stack, domain,
and profile roots. The current integration coverage does not run a concurrent
collector through a parked dispatcher; 03.01 remains open for that boundary and
the other call, native, debug, and suspend paths.
