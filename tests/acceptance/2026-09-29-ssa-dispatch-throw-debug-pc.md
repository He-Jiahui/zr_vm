---
related_code:
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
tests:
  - tests/core/test_ssa_dispatch_boundaries.c
plan_sources:
  - docs/plans/ssa/03-interpreter-binding/01-dispatch-boundaries.md
doc_type: acceptance-record
---

# SSA 03.01 Debug Trace at a Throwing Instruction

## Scope

This slice verifies the executable 03.01 boundary that debugging observes the
program counter at the instruction that throws. The regression uses
`ZrCore_Execute` with a real `THROW` opcode and a trace observer, then checks the
observed instruction pointer, bytecode offset, source line, call-frame PC, and
`state->previousProgramCounter`. The observer is the only debug facility
registered; no hook trap is pending.

The dispatcher now keeps the fast path disabled while a trace observer is
registered. Its shared fetch path calls the observer independently of pending
hook traps, publishes the stack top, and only then fetches the next instruction.

## Baseline

Before the dispatcher change, the focused Unity executable reported 7 tests and
1 failure: the observer callback count was 0 instead of 2. The existing tests
passed. Disabling only the fast path was insufficient because the shared fetch
macro also gated `ZrCore_Debug_TraceExecution` on a pending trap; the paired
fetch-path change made observer callbacks independent of traps.

## Test Inventory

`test_observer_only_debug_reports_throw_instruction_pc_and_line` creates a
two-instruction function with a `NOP` at offset 0 and `THROW` at offset 1. Its
source map assigns line 47 to the throw. The observer verifies the callback
points at the actual `THROW`, and the test verifies the runtime-error result
and the saved call-frame and state PCs.

## Tooling Evidence

The GCC build used the existing D-drive cache at
`/mnt/d/tmp/zr_vm/close-proxy-core-red`; all compiled artifacts and CTest logs
remained there. The direct callable-metadata and precall-frame-reset suites are
not registered with CTest in this cache, so they were run directly.

```bash
cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red \
  --target zr_vm_ssa_dispatch_boundaries_test -j 4
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_ssa_dispatch_boundaries_test
ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red \
  -R '^ssa_dispatch_boundaries$' --output-on-failure --no-tests=error
cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red \
  --target zr_vm_execution_dispatch_callable_metadata_test \
    zr_vm_precall_frame_slot_reset_test -j 4
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_execution_dispatch_callable_metadata_test
/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_precall_frame_slot_reset_test
```

## Results

- The focused dispatcher Unity executable passed 7/7 tests, and registered CTest
  `ssa_dispatch_boundaries` passed 1/1.
- The adjacent callable-metadata suite passed 18/18; the precall frame-slot
  reset suite passed 18/18.
- An independent MSVC build and direct run of the focused dispatcher suite also
  passed 7/7 tests.
- GCC reported the dispatch file's existing computed-goto pedantic and unused
  label warnings; the focused target built and linked successfully.

## Acceptance Decision

Accepted for the observer-only throwing-instruction PC boundary. The test
proves the trace callback sees offset 1, source line 47, and the actual throwing
instruction before execution, while the VM returns a runtime error. It does not
claim coverage for debug hook signaling, suspension, or the remaining 03.01
call/native boundaries; the plan remains open.
