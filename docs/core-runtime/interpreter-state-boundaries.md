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
tests:
  - tests/core/test_ssa_dispatch_boundaries.c
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

Any operation that can grow or move the VM stack must use the existing
`SZrFunctionStackAnchor` APIs; a cached frame base is never valid across such an
operation. Exception or budget unwinding may replace or remove the call frame,
so a reload must not dereference the old `SZrCallInfo` after unwinding.

This first boundary layer intentionally does not bind ExecIR layouts or active
call-binding generations. Those invariants are introduced by the later binding
guard stages. Bytecode dispatch continues to use the single instruction list
from `zr_instruction_conf.h` for both computed-goto and switch builds.
