---
related_code:
  - zr_vm_core/include/zr_vm_core/call_info.h
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_safepoint.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/stack.c
tests:
  - tests/core/test_ssa_dispatch_boundaries.c
  - tests/core/test_ssa_dispatch_vm_call_minor_gc.inc
plan_sources:
  - docs/plans/ssa/03-interpreter-binding/01-dispatch-boundaries.md
doc_type: acceptance-record
---

# SSA 03.01 VM-to-VM stack growth and minor GC

## Scope

This slice characterizes one real interpreter `FUNCTION_CALL` with stack growth
and a same-domain generational minor collection while the callee is active. The
caller resumes at its next `GET_STACK`, then returns the rooted object. The
fixture uses no native function call to initiate collection.

The interpreter safepoint poll publishes state, parks the mutator, and reloads
the current frame after resume. The collector is initiated by a separate test
thread attached to the same GC domain. That thread acquires a stop-the-world
pause and calls `ZrCore_GarbageCollector_GcStep`; the interpreter does not
automatically start collection from GC allocation debt.

## Fixture and synchronization

`test_vm_call_stack_growth_and_minor_gc_reload_caller_frame` builds a caller
with three instructions:

1. `FUNCTION_CALL` passes the rooted object to a VM callee.
2. `GET_STACK` reads the result from the caller frame.
3. `FUNCTION_RETURN` returns that object.

The callee has a larger frame and a finite 4,194,304-instruction NOP body, then
returns its object parameter. The longer window gives the collector thread time
to observe the running mutator on slower schedulers. Before retrying pauses, the
worker waits for up to 3,000 iterations, sleeping 1 ms between checks, for a
nonzero running-mutator count. The earlier 1,000 `Sleep(0)`-only polls could
finish before the worker observed the VM execution on Windows.

The first timing adjustment fixed that observation, but repeated MSVC runs then
returned false from `StopTheWorldBegin` after an early pause saw only the
caller: the diagnostic stage was STOP_THE_WORLD on attempt 2, after one missed
call-chain check. On Windows the retry now sleeps 1 ms after releasing an early
pause, so the mutator can advance before the next bounded STW attempt. The
worker retains timeout stage and attempt counters and prints domain snapshot
details only on a timeout; it never reads the call chain outside STW.

The test collector inspects `callInfoList` only while STW is active. If it
pauses before the VM call has installed the callee frame, it releases that
pause and retries. It runs `GcStep` only after the paused chain confirms the
callee and caller frames. This avoids an unsynchronized cross-thread read of the
interpreter's call chain.

## Evidence

The first single-pause draft failed its call-chain assertion. GDB showed that
the collector had paused with only the caller frame active, before stack growth.
The fixture was changed to inspect under STW and retry early pauses; no
dispatcher or GC production change was needed.

GDB 12.1 at the completed minor-collection pause observed:

- `callChainWasVmToVm = 1`, with callee PC offset 254 and caller continuation
  offset 1 (the instruction after `FUNCTION_CALL`).
- The VM stack grew, its base moved, and the caller frame pointer moved. The
  frame resolved from the saved stack offset matched the active caller frame.
- The rooted object was present in both the caller and callee before and after
  collection. Its region changed from EDEN to SURVIVOR, while its storage kind
  remained young-movable.
- The minor collection count changed from 0 to 1, the reported collection kind
  was minor, and the outer STW pause remained active while the test inspected
  the post-collection roots.

The current minor collector promotes this ordinary object in place. This test
proves generational collection and root survival; it does not claim physical
object relocation or major compaction.

## Tooling and results

All compiled outputs remained in the existing D-drive GCC cache
`/mnt/d/tmp/zr_vm/close-proxy-core-red`.
The independent Windows build used the existing D-drive MSVC cache
`D:/tmp/zr_vm/ssa-artifact-v6-msvc`.

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

- Before the Windows retry-yield adjustment, a diagnostic MSVC run failed once
  in ten direct runs at STOP_THE_WORLD (attempt 2, one missed caller-only
  call-chain check). After the bounded `Sleep(1)` retry yield, the focused MSVC
  executable passed 10/10 consecutive local direct runs and an independent
  20/20 direct runs; registered CTest passed 1/1.
- The final GCC source built successfully, the direct Unity executable passed
  9/9, and registered CTest `ssa_dispatch_boundaries` passed 1/1. An earlier
  version of the timing fix also passed five consecutive GCC direct runs.
- The MSVC target build completed successfully with the existing warnings in
  the older dispatch-boundary tests; the new fixture emitted no conversion or
  maybe-uninitialized warnings.
- Registered CTest `ssa_dispatch_boundaries` passed 1/1.
- Callable-metadata and precall-frame-slot-reset direct suites each passed
  18/18 tests.
- The focused and adjacent GCC targets built successfully. The core build emitted
  existing warnings in unchanged sources, including computed-goto pedantic and
  signedness/type-limit warnings.

## Acceptance decision

Accepted as a characterization of stack relocation, call-chain publication,
and root survival across an externally initiated minor collection during a
VM-to-VM call. The final caller `GET_STACK` and return preserve object identity;
final PC offset 2 is the location of `FUNCTION_RETURN` while it executes, not a
failed continuation restore. Suspension and physical object relocation remain
outside this slice and the 03.01 milestone remains open.
