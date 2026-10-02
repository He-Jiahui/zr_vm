---
related_code:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_opcode.def
  - tests/parser/test_ssa_invoke_result_availability.c
  - tests/cmake/ssa-invoke-result-tests.cmake
  - tests/CMakeLists.txt
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/01-core-model.md
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
tests:
  - tests/parser/test_ssa_invoke_result_availability.c
  - tests/acceptance/ssa-invoke-result-edge-availability.md
doc_type: module-detail
---

# INVOKE result edge availability

## Contract

A value-producing throwing terminator publishes its result only on its normal
successor. A value PHI consumes its incoming value on a particular predecessor
edge. When that predecessor is the defining INVOKE block, the destination must
therefore be checked: the direct normal edge can use the result, and the direct
exception edge cannot.

For ordinary operands, the verifier follows paths from the exceptional
successor. The result remains unavailable along those paths until control
returns to its defining block. Re-executing that block replaces the earlier
invocation: its normal edge establishes a new result and its exceptional edge
starts the same unavailable state again. Both the initial exceptional-successor check and the traversal stop at this
definition instead of carrying the earlier state through a retry. This also
covers an INVOKE in an exception block whose exceptional edge targets itself:
its normal PHI edge remains legal, while a PHI on that exceptional self edge
cannot consume its result.

For other PHI predecessors, the same traversal checks availability at the
predecessor block. This preserves the difference between a block operand and a
PHI edge: an exceptional handler may branch to the normal block when its PHI
uses an external value on that edge and the INVOKE result on the normal edge.
Using the INVOKE result on both incoming edges is rejected.

## Implementation boundary

`zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c` owns this check beside
ordinary SSA dominance. Structural verification already validates the
terminator's ordered normal/exception successors and the PHI predecessor slots.
The edge check does not change memory/effect tokens, exception payloads,
ownership, instruction execution, or the serialized IR schema.

The file remains a single SSA validation responsibility below 1,000 lines. This
repair extends its existing throwing-result helper rather than introducing
another analysis subsystem. If the responsibility grows further, throwing-result
availability is a coherent extraction boundary.

## Focused regression

`tests/parser/test_ssa_invoke_result_availability.c` calls the public core
`ZrCore_ExecIr_VerifyFunction` with structure and SSA verification. Twelve CFG
fixtures cover direct exceptional and normal PHIs, normal operands and PHIs
after handler retry, handler bypass operands and two edge-sensitive PHI cases,
exceptional cleanup operands, exceptional merge PHIs, and self-exception retry
normal PHIs/operands plus illegal exceptional self-edge PHIs. Negative cases check
the diagnostic code plus function, block, instruction, source, definition, and
value identities. Positive cases require the success diagnostic.

The exceptional PHI fixtures first prove the same CFG valid with an external
value before replacing only the exceptional incoming value with the INVOKE
result. The tests do not execute the callee or claim backend/source-language
parity. Validation evidence is recorded in
`tests/acceptance/ssa-invoke-result-edge-availability.md`.
