---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_core/include/zr_vm_core/execution_context.h
  - zr_vm_core/include/zr_vm_core/call_info.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/include/zr_vm_core/debug.h
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/src/zr_vm_core/execution/execution_safepoint.c
  - zr_vm_core/src/zr_vm_core/execution/execution_cold.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h
  - zr_vm_core/src/zr_vm_core/object/object_super_array.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c
  - zr_vm_core/src/zr_vm_core/debug.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/execution_context.h
  - zr_vm_core/src/zr_vm_core/execution/execution_safepoint.c
  - zr_vm_core/src/zr_vm_core/execution/execution_cold.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h
  - zr_vm_core/src/zr_vm_core/object/object_super_array.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c
tests:
  - tests/core/test_ssa_dispatch_boundaries.c
  - tests/core/test_ssa_dispatch_native_callback.inc
  - tests/core/test_ssa_dispatch_vm_call_minor_gc.inc
  - tests/acceptance/2026-09-28-ssa-dispatch-safepoint-poll.md
  - tests/acceptance/2026-09-29-ssa-dispatch-throw-debug-pc.md
  - tests/acceptance/2026-09-29-ssa-dispatch-native-stack-growth-minor-gc.md
  - tests/acceptance/ssa-dispatch-vm-call-minor-gc.md
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

The native-call boundary also has to survive stack relocation inside the
callback. `KNOWN_NATIVE_CALL` publishes its continuation at the instruction
after the call before preparing the native frame. The native call-info links
back to the VM caller, and function-call setup anchors its stack window and
return destination. A callback may grow the stack and run a collection; after
each operation it must resolve the caller frame from the current stack base and
the saved offset, then return to dispatch with the published PC intact.

The native callback fixture exercises this path with a real `KNOWN_NATIVE_CALL`,
`GET_STACK`, and `FUNCTION_RETURN`. Its callback doubles the logical stack
capacity, records the caller frame and PC, and runs a generational minor
collection. The test checks the frame base against its reloaded stack offset,
checks that the caller remains at PC 1 through the collection, and verifies the
next load and return still produce the rooted object. The observed GCC run
relocated the stack allocation from 64 to 128 slots. If an allocator grows in
place, the test still checks the capacity and frame-offset invariants without
requiring a pointer change.

The collector currently promotes ordinary individually allocated objects from
EDEN to SURVIVOR in place during minor collection. This boundary therefore
asserts the collection kind, generation/storage transition, and continued
frame-root value; it does not claim that the object address moves. Physical
object relocation/compaction is a separate GC contract.

The VM-to-VM fixture covers a real `FUNCTION_CALL` into a callee with a larger
frame, followed by `GET_STACK` and `FUNCTION_RETURN` in the caller. A separate
thread attaches a collector state to the same GC domain and requests a minor
collection while the callee is active. The interpreter poll publishes the
callee PC, call chain, and stack top, parks the mutator, then reloads the frame
after the collector resumes it. The collector is initiated externally through
`ZrCore_GarbageCollector_GcStep`; the interpreter safepoint poll does not start
a collection from allocation debt on its own.

The worker inspects the call chain only while stop-the-world is active. If an
early pause lands before `FUNCTION_CALL` installs the callee frame, it releases
the pause and retries. The test verifies that the parked callee PC is inside
the callee, the caller PC is at the instruction after `FUNCTION_CALL`, stack
growth preserves the caller frame's saved offset, and the minor collection
promotes the rooted object from EDEN to SURVIVOR. It then verifies the caller's
next `GET_STACK` and return still produce that object. As with the native
callback fixture, this checks generational collection and root survival without
claiming physical object movement.

This first boundary layer intentionally does not bind ExecIR layouts or active
call-binding generations. Those invariants are introduced by the later binding
guard stages. Bytecode dispatch continues to use the single instruction list
from `zr_instruction_conf.h` for both computed-goto and switch builds.

The computed-goto fast table explicitly initializes every current opcode:
144 existing fast-handler entries and 102 ordinary fallback entries, including
`FUNCTION_CALL_SPREAD`. This removes overlapping default initializers without
changing handler targets or the indexed dispatch expression. The reviewed
246-opcode count is checked at compile time; changes to the instruction schema
require a complete table review. Label addresses remain in `ZrCore_Execute`,
because computed-goto labels belong to that function. The switch build keeps
the same opcode numbers and default handler. This finite table maintenance does
not close the remaining runtime or performance gates in 03.01.

The shared fetch uses a conditional expression for the one-instruction advance.
It preserves the comparison and selected operand evaluation: the one-instruction
path increments the PC before dereferencing it, and the other path adds the
requested count before dereferencing it. The indexed dispatch expression and
the fast-only fetch macro remain unchanged. The two inline-array fast label names
are emitted only in computed-goto builds; their ordinary switch handlers remain
unchanged.

The invalid spread-operand path and the by-value super-array fast setter raise
errors through the declared non-returning ZrCore_Debug_RunError. Their following
dead returns are removed; the error calls and reachable success returns retain
their behavior. The raw-int materializer also removes dead returns after its
three errors: unexpected node entries, pair-capacity overflow and failed
materialization. The optional by-value setter in object_super_array.c removes
dead returns after the bounds errors in both its primitive and copy branches.
All conditions, error calls, materialization order and reachable returns remain
unchanged.

The retained r2 compile-only run completed four actual compiler processes with
exit 0. Clang dispatch, Clang super-array and MSVC dispatch passed the zero
diagnostic and actual-header gates. MSVC super-array emitted five C4702 warnings;
the r2 receipt is validated=false, with four actual compilations and three
verified compilations. Its stdout/stderr logs were decoded with a UTF-8
StreamReader and may contain replacement characters; they are decoded evidence,
not byte-exact native output.

The five additional dead-return removals passed a new four-phase compile-only
verification with Clang 19 UBSan instrumentation and MSVC 19.44, using the existing
C11, MD, W4 and UNDEBUG settings and the MSVC C11 atomics option. Both compilers
compiled dispatch and super-array with zero diagnostics and natural exit 0.
Their actual header dependencies numbered 243 and 213 for Clang, and 132 and 102
for MSVC. All four native processes completed and both output streams drained;
the new receipt records four actual and four verified compilations. The helper
saved raw BaseStream bytes, then decoded only in memory for diagnostic analysis.
The internal super-array header is also consumed by object.c and
execution_member_access.c; their affected objects require header-dependency
validation. Compilation does not establish interpreter execution, Source
retention, runtime performance or completion of the remaining SSA gates.

`ZR_INSTRUCTION_USE_RET_FLAG` in `operandExtra` selects the interpreter's
local `ret` temporary instead of a stack destination. Opcode handlers interpret
that field: loads can write this temporary, while meta-set handlers can read
it as a receiver. The sentinel does not itself mean that the result is
discarded or that ownership has been released.

The focused dispatch test runs a 257-NOP function and checks that the saved
resume PC matches `state->previousProgramCounter` at the 256-fetch poll. Direct
reload coverage also simulates replaced call-frame, instruction, stack, domain,
and profile roots. A separate observer-only trace test runs a real `THROW` and
checks that the callback sees that opcode's PC and mapped source line before it
executes, even when no debug hook trap is pending. Registering a trace observer
keeps dispatch on the traced path, and shared fetch invokes the observer
independently of hook traps. A separate integration test covers full GC while
the dispatcher is parked. The native-callback test covers stack growth and a
minor collection between a call instruction and its next frame load. The trace
test does not cover debugger hook signaling or suspension at the throwing
instruction. This VM-to-VM call fixture covers one ordinary call frame; other
call-boundary variants and suspension remain open in 03.01.
