---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_frame_layout.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_call_transfer.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_call_transfer.c
  - zr_vm_core/include/zr_vm_core/execution_call_transfer.h
  - zr_vm_core/src/zr_vm_core/execution/execution_call_transfer.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_tail_call.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dynamic_call_guard.h
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_call_transfer.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_call_transfer.c
  - zr_vm_core/include/zr_vm_core/execution_call_transfer.h
  - zr_vm_core/src/zr_vm_core/execution/execution_call_transfer.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dynamic_call_guard.h
tests:
  - tests/core/test_ssa_call_return_tail.c
  - tests/parser/test_call_binding_pipeline.c
  - tests/parser/test_tail_dispatch_runtime.c
plan_sources:
  - docs/plans/ssa/04-frame-native/02-call-return-tail.md
doc_type: module-detail
status: draft
---

# Call, return, and tail transfer

04.02 separates transfer classification from execution. The parser receives the
packed frame layouts produced by 04.01 and records one of five explicit classes:
scalar copy, inline-span copy, move, borrow, or boxed bridge. Core execution
consumes that class; it must not infer boxing or ownership from a runtime value.

Return forwarding is allowed only when source and target layouts are compatible,
there is no alias conflict, commit order is preserved, and no receiver/writeback
step is required. Otherwise the callee writes an independent return buffer and
the caller commits it after successful completion. A throw or native failure
therefore cannot expose a half-written aggregate.

Tail-frame reuse is an optimization. The eligibility predicate requires no
pending cleanup, no escaping alias into the old frame, a compatible
continuation, and a debug policy that permits frame identity reuse. If any
condition is false, callers use the normal call-frame path. Existing
`ZrCore_Function_TryReuseTailVmCall` remains the runtime frame operation; this
contract only centralizes the safety decision.

Transfer plans are transactional: validation and allocation occur in a
candidate plan, and the destination plan is replaced only after every value is
validated. Failure leaves the caller's previous plan intact. Ownership cleanup
is consequently performed by the selected runtime path exactly once.

## Tail-call fallback window

The dispatcher marks the live outgoing call window by setting `stackTop` to the
slot after the callable and its staged arguments. A dynamic callable can be
rewritten to its `@call` function before frame reuse is attempted. That rewrite
inserts the original object as `self` and advances `stackTop`, so the active
window can be longer than the argument count encoded by the original tail-call
instruction.

If frame reuse is declined, the normal-call fallback copies the live window
from the callable slot up to `stackTop`. It derives the copy length from those
two pointers after the reuse attempt, rather than from the stale instruction or
call-site-cache argument count. The fallback helper keeps stack anchors around
stack growth and restores both the frame base and source window before copying.
This preserves the receiver and every original argument, including the final
argument after an inserted receiver.

`test_dynamic_tail_meta_call_preserves_last_argument_after_reuse_declines`
compares ordinary and tail dynamic calls to the same `@call` contract. Its
inline-struct parameter forces frame reuse to decline, while a separate integer
sentinel contributes to the expected result `347`. The test also checks the
compiled `@call` parameter layout and the expected ordinary/tail call opcodes.
This regression covers fallback-window sizing only; the broader 04.02 transfer
plan and return-forwarding work remains open.

The size derivation stays in the existing dispatcher fallback helper. That
helper owns the staging copy and stack-anchor restoration; extracting only its
live-span calculation would add a cross-file seam without separating a complete
responsibility. A future split should move the coherent tail-call preparation
and fallback family together if that boundary grows.

## Validation evidence for this slice

The `D:/tmp/zr_vm/ssa-artifact-v6-msvc` MSVC build completed 708/708 edges.
The call-binding pipeline direct test passed 18/18, the adjacent tail-reuse
call-info test passed 4/4, and registered CTest `call_binding_pipeline` plus
`ssa_call_return_tail` passed 2/2. These results cover the fallback regression
and nearby tail-call behavior; they do not accept all of 04.02.

## Legacy dynamic call errors

The generic precall helper reports a missing `@call` with `Debug_CallError`,
which performs a non-local Throw. Calling that helper directly from a dynamic
VM instruction used to bypass `execution_unwind_exception_to_handler`, so a
source-language catch was skipped and the caller's Unique owner remained
registered after the host captured the error.

Dynamic ordinary, tail, no-argument and cached instruction bodies now check
the target before generic precall. The small private inline guard matches the
precall admission categories: functions, closures, native pointers and the
existing native-data case continue directly; other targets require an `@call`
meta entry. The guard does not stage or invoke the receiver. Rejection uses the
dispatcher's existing runtime-error path to save the PC, create the same
non-callable Error message, clear pending control and unwind to the VM handler.
An unhandled exception propagates after VM frame cleanup.

The source runtime gate covers 8,192 eligible tail calls with a stable frame,
unhandled ordinary and tail scalar-target errors, and a language catch that
returns normally. Owned Tracker destruction must run exactly once, ownership
roots must return to baseline, and the error retains its message and traceback.
At the catch entry the outer owner is still rooted; after successful execution
the harness keeps one result at `stackBase + 1` and top at `stackBase + 2`.
Captured failures use the harness's separate one-slot reset boundary.

These are legacy-bytecode checks. They do not connect ExecIR transfer plans to
execution or establish null-callable, suspend, malformed native-pointer or
all ownership/GC failure behavior. The admission and error-recovery choice is
consistent with the reference interpreter call boundaries in `lua/src/ldo.c`
and the exception return from a missing call target in
`lua/QuickJS-master/quickjs.c`; ZR retains its own Error object and VM unwind
mechanism. Detailed platform results are recorded in
[the runtime acceptance](../../tests/acceptance/ssa-tail-dispatch-runtime.md).
