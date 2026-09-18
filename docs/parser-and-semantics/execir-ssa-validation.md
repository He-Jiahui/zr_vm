---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_opcode.def
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
tests:
  - tests/parser/test_ssa_value_validation.c
  - tests/cmake/ssa-tests.cmake
  - tests/acceptance/ssa-value-validation.md
doc_type: module-detail
---

# Parser ExecIR SSA value validation

## Scope and entry point

`ZrParser_ExecIr_BuildSsa` currently verifies the stable definitions already
assigned to values produced by SemanticIR. It does not insert pruned phi
nodes, promote address-taken places, rename definitions across a dominance
frontier, or prove exceptional-result availability. The builder invokes this
step only after it emits CFG adjacency and computes immediate dominators.

The pass checks function-level storage consistency before reading instructions
or operand/value side pools: counts may not exceed allocated capacities and a
nonempty pool needs backing storage. Per instruction, the opcode must be known,
fixed operand arity must match, and `[start, start + count)` must fit within
the logical operand pool. The range comparison subtracts only after checking
`start <= operandCount`, so a `UINT32_MAX` start cannot wrap around to a small
index. Operand IDs must reference defined values in the function's value
pool. No input arrays or value definitions are modified on either path.

## Diagnostics and ownership

Invalid storage, arity or operand range reports `INVALID_RANGE`; an unknown
opcode reports `UNKNOWN_OPCODE`; an invalid or undefined value reports
`INVALID_VALUE`. Instruction-level diagnostics carry the function token and
one-based instruction ID. Function-level malformed backing storage carries
the function token and instruction ID zero. Null function input returns false
without dereferencing the diagnostic target. The pass owns no allocations;
the surrounding builder owns and releases its temporary ExecIR function if
construction fails.

## Verification boundary

`ssa_value_validation` links the production parser validation and core model.
Its positive case defines an operand before its use; failure cases cover a
logical out-of-range operand whose physical memory contains a valid value, a
wrapped range, null backing storage, and an unknown opcode. Full CFG-aware SSA
construction and differential language fixtures remain separate 01.02 work;
see `tests/acceptance/ssa-value-validation.md` for actual test runs.
