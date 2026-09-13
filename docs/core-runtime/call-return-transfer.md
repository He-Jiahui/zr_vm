---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_frame_layout.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_call_transfer.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_frame_layout.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_call_transfer.c
  - zr_vm_core/include/zr_vm_core/execution_call_transfer.h
  - zr_vm_core/src/zr_vm_core/execution/execution_call_transfer.c
  - zr_vm_core/src/zr_vm_core/execution/execution_tail_call.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_call_transfer.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_call_transfer.c
  - zr_vm_core/include/zr_vm_core/execution_call_transfer.h
  - zr_vm_core/src/zr_vm_core/execution/execution_call_transfer.c
tests:
  - tests/core/test_ssa_call_return_tail.c
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
