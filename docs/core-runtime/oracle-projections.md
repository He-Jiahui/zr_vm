---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_interpreter.h
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_oracle.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_projections.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_oracle.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_execbc.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_oracle.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_execbc.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
plan_sources:
  - user: 2026-09-12 SSA plan implementation
  - docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
  - docs/plans/ssa/guides/A-execir-builder-verifier.md
  - docs/plans/ssa/guides/E-projections-fusion-aot.md
tests:
  - tests/parser/test_ssa_oracle_projections.c
  - tests/cmake/ssa-tests.cmake
doc_type: module-detail
---

# ExecIR oracle and initial projections

01.05 provides a pointer-free reference execution seam and two transactional,
no-optimization projections. `ZrCore_ExecIr_RunOracleEx` validates the complete
function shape before reading pools, allocates an isolated value environment,
and commits the result only after a normal return, throw, or suspend. The
legacy `ZrCore_ExecIr_RunOracle` entry remains a compatibility counter for
callers that only need instruction coverage.

The oracle currently executes the scalar/control subset (constants, copies and
conversions, checked integer/floating arithmetic, comparisons, branches,
switches, phi entry, return, throw, suspend, and callback-backed calls). Store,
drop, barrier, call, throw, suspend, and provider-backed allocation operations
append bounded operand snapshots to the observable event stream. Calls,
loads, and allocations require explicit caller providers; place projection and
invoke/landing-pad operations remain unsupported. Provider failures preserve a
specific oracle diagnostic and instruction/source identity, while missing
providers return `ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED`. Arithmetic faults and
infinite control flow have separate diagnostics and a caller-configurable step
limit.

`ZrParser_ExecIr_LowerExecBc` copies instruction, operand/result, CFG, source,
state-map, GC/deopt counts, and value-slot metadata into owned arrays. Every
value receives a distinct slot while optimization is disabled. Phi incoming
assignments are emitted as edge-tagged parallel-copy records; cyclic swaps set
`temporarySlotCount` so a later emitter can use a temporary slot. Critical CFG
edges are split into synthetic empty blocks in the projection, preserving the
source function and keeping phi copies on an edge-local block.

Allocation instructions are transported with the same stable opcode, ranges,
and source identity. This does not enable execution: the lowerer carries the
metadata while the runtime allocator and GC protocol remain a backend-owned
follow-up.

`ZrParser_ExecIr_LowerAot` uses the same builder and transfers ownership of the
projection arrays, adding the function token, signature hash, and execution
contract. It is an AOTIR seam only: `runnable` is deliberately false until the
07.01 adapter and 07.02 C/LLVM emitters exist. No projection stores a runtime
pointer or treats an opcode count as evidence of executable backend parity.

Both lowerers build into a prepared value and replace an existing output only
on success. Callers should initialize output records to zero and release them
with the matching `*_Free` function; oracle results use
`ZrCore_ExecIr_OracleResultInit/Free` for the same ownership discipline.
