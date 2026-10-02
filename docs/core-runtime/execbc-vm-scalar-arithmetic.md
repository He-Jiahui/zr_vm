---
related_code:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_validate.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_canonical_types.c
  - zr_vm_core/src/zr_vm_core/execution/execution_signed_multiply.inc
  - tests/parser/test_ssa_execbc_vm.c
  - tests/parser/ssa_execbc_vm_multiply_cases.inc
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_validate.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
tests:
  - tests/parser/test_ssa_execbc_vm.c
  - tests/parser/ssa_execbc_vm_multiply_cases.inc
  - tests/core/test_execution_checked_multiply.c
  - tests/acceptance/2026-10-02-ssa-execbc-vm-multiply.md
doc_type: module-detail
---

# ExecBC scalar arithmetic materialization

The no-optimization ExecBC VM materializer validates a projection before
allocating a Core function. Its signed scalar arithmetic subset includes
ADD, SUB, and MUL. Each requires exactly two input values and one result,
with instruction, input, and result type tokens all equal to runtime i64.
Other numeric representations remain rejected by this consumer. A malformed
operand count reports `INVALID_VALUE`; a mismatched arithmetic type reports
`UNSUPPORTED`. Both preserve the input projection and publish an empty emission.

`exec_ir_execbc_vm_validate.c` owns these checks. The shared ADD/SUB/MUL
validation branch resolves value-to-physical-slot mappings and applies the
same i64 contract to all three operations. MUL introduces no special handling
for source ASTs, literal values, names, or constant folding.

`exec_ir_execbc_vm.c` emits `ADD_SIGNED`, `SUB_SIGNED`, or `MUL_SIGNED` after
validation. All three use the result's physical slot and two input slots,
and follow the same instruction-plan append path. The materializer preserves
the originating block, instruction, and source IDs in its native PC map.
The canonical-type entry adapter continues to resolve canonical IDs into
runtime scalar tokens on a copy, leaving the original projection unchanged.

## Checked multiplication dependency

The emitted `MUL_SIGNED` executes through Core's checked multiplication
implementation. Commit `8759ccc2` supplied that runtime support before MUL
was admitted by this materializer. On overflow, runtime raises an arithmetic
error before the destination store and retains the faulting PC for exception
handling. The materializer emits the operation; it does not infer numeric
ranges or prove a literal product representable.

The multiplication test has separate Oracle and VM overflow assertions.
The Oracle rejects `INT64_MAX * 2` and `INT64_MIN * -1` with
`ARITHMETIC_ERROR`. The VM test independently builds and materializes those
well-typed operations, invokes the emitted function through `ZrCore_Execute`
inside `ZrCore_Exception_TryRun`, and requires `RUNTIME_ERROR`, a normalized
current exception with the same status, and a fault PC mapping to instruction
3/source 903. It resets the thread, changes the same emitted function's
constant pool to `-13` and `7`, then executes on the same state and requires
`-91`, cleared exception state, and `FINE` status. Oracle failure alone is
not treated as VM overflow evidence.

## Focused verification

`ssa_execbc_vm_multiply_cases.inc` holds the multiplication fixture and tests.
The existing main test file only includes and registers these cases, keeping
the arithmetic-specific lifecycle and diagnostics separate from its CFG,
phi, frame, and state-map fixtures.

The formal target is `zr_vm_ssa_exec_ir_execbc_vm_test`, CTest name
`ssa_exec_ir_execbc_vm`. The multiplication cases check four representable
products, native `MUL_SIGNED` emission, projection immutability, malformed
operand/type rejection before publication, Oracle overflow, and materialized
VM overflow/recovery. The existing cases continue to exercise ADD/SUB,
canonical adapters, branch/phi paths, sparse slots, and unsupported metadata.

See [the multiplication acceptance record](../../tests/acceptance/2026-10-02-ssa-execbc-vm-multiply.md)
for actual commands, results, and unverified platforms. The source-owned
conditional-arm extension is a separate consumer and acceptance slice.
