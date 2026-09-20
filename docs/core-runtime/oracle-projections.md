---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_interpreter.h
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_internal.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_phi.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_oracle.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_projections.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_oracle.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_execbc.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_internal.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_phi.c
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
  - tests/parser/test_ssa_oracle_parallel_edges.c
  - tests/cmake/ssa-tests.cmake
  - tests/acceptance/ssa-oracle-parallel-edges.md
  - tests/acceptance/ssa-projection-parallel-edges.md
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
load, drop, barrier, call, throw, suspend, and provider-backed allocation
operations append bounded operand snapshots to the observable event stream.
`TYPE_TEST` is also executable when the caller supplies the explicit
`FZrExecIrOracleTypeTest` provider. The provider receives the pointer-free
operand snapshot and canonical `matchTypeToken`, and returns either a boolean
membership result or a rejection; false membership is a successful result,
while rejection reports `ZR_EXEC_IR_DIAGNOSTIC_ORACLE_TYPE_TEST_ERROR`.
Calls, invokes, iterator steps, loads, allocations, type tests, and exception
payload reads require explicit caller providers. Executable place projection
and landing-pad handling remain unsupported.

`CALL` appends a bounded call event only after its provider returns a defined
value; provider rejection reports `ZR_EXEC_IR_DIAGNOSTIC_ORACLE_CALL_ERROR`
instead of returning a false result with an empty diagnostic.

`EXCEPTION_PAYLOAD` is executable at a handler-entry instruction when the
caller supplies `FZrExecIrOracleExceptionPayload`. The provider returns one
defined pointer-free value; a rejected read reports
`ZR_EXEC_IR_DIAGNOSTIC_ORACLE_EXCEPTION_PAYLOAD_ERROR`, and a missing provider
remains `ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED`. This resolves the active payload
without making the reference interpreter own a runtime exception object or
pretending that it can enter a landing pad by itself.
Provider failures preserve a specific oracle diagnostic and instruction/source
identity, while missing providers return
`ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED`. Arithmetic faults and infinite control
flow have separate diagnostics and a caller-configurable step limit.

At block entry, value phis are evaluated together against the predecessor
**edge slot**, not merely the predecessor block ID. The terminator records the
selected successor ordinal (conditional branch or switch index). For repeated
edges from one source to the same destination, the ordinal's occurrence number
selects the corresponding occurrence in the destination's predecessor range;
the phi incoming at that exact index is read before any phi result is written.
This preserves distinct incoming values even when both branch arms target the
same block. A missing or inconsistent edge occurrence reports
`PHI_PREDECESSOR_MISMATCH` rather than silently reading another arm's value.
The oracle's own preflight no longer rejects repeated predecessor IDs when
their phi incoming positions match the predecessor range. See
`tests/acceptance/ssa-oracle-parallel-edges.md` for the oracle regression;
projection-layer evidence for the same edge pattern is recorded separately
in `tests/acceptance/ssa-projection-parallel-edges.md`.

The edge-occurrence lookup and two-phase phi entry live in
`exec_ir_interpreter_phi.c`; `exec_ir_interpreter.c` remains responsible for
instruction dispatch and tracking the selected successor ordinal. Their
private header shares only diagnostic and checked-size helpers plus the block
entry call; no new public core API or alternate execution path is introduced.

THROW and SUSPEND are observable termination boundaries in reference mode. The
oracle publishes their event into its prepared result, stops before any later
instruction, and commits `terminatedByThrow` or `suspended` respectively. The
payload-bearing SUSPEND form additionally copies its first operand to the
result slot and return-value snapshot. This is not a landing-pad or resume
implementation: selecting a handler block from a runtime exception and
resuming after a catch remain outside the direct oracle until their runtime ABI
is specified.

DROP also consumes its oracle operand slot after publishing the event; a later
use is rejected as `ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE`. This is the
pointer-free reference equivalent of clearing a runtime OWN_DROP slot and does
not claim that an external destructor is executed by the oracle.

MOVE transfers its oracle value into the SSA result and consumes the source
slot, while COPY leaves its source available. A later source use after MOVE is
therefore rejected with the same invalid-value diagnostic; this models the
state-map moved-owner boundary without adding a runtime ownership action.

`ZrParser_ExecIr_LowerExecBc` copies instruction, operand/result, CFG, source,
state-map, GC/deopt counts, and value-slot metadata into owned arrays. Every
value receives a distinct slot while optimization is disabled. Phi incoming
assignments are emitted as edge-tagged parallel-copy records; cyclic swaps set
`temporarySlotCount` so a later emitter can use a temporary slot. Critical CFG
edges are split into synthetic empty blocks in the projection, preserving the
source function and keeping phi copies on an edge-local block. Parallel CFG
edges are paired by their occurrence number in the source successor and
destination predecessor rows. Each critical occurrence gets its own split
block; phi incoming predecessors are rewritten to the projected predecessor
row, and each nontrivial copy is tagged with that edge's projected block ID.
Mismatched adjacency multiplicities fail preflight without replacing a
previously published projection. This is projection metadata, not executable
bytecode or emitted AOT code.

`TYPE_TEST` is also transported by both initial projections with its separate
`matchTypeToken` side field. This preserves canonical subtype identity for a
later backend adapter, but the projections set `runnable` to false whenever a
type test is present; they do not invent an executable subtype ABI. The direct
Oracle provider described above remains the only executable reference seam.

`ALLOC` likewise remains metadata-only in ExecBC and AOT projections: a
projection containing allocation is marked non-runnable until the backend
allocator/GC ABI is connected. The direct Oracle's pointer-free allocation
provider does not change that projection boundary.

`EXCEPTION_PAYLOAD` is likewise transported by both initial projections with
its stable opcode, ranges, and source identity. The lowerers mark any
projection containing the operation non-runnable because the executable
backends do not yet expose an active exception-payload ABI; the direct Oracle
provider remains the only executable reference seam.

`INVOKE` is transported with its result, operand, and ordered normal/exception
successor ranges. Both lowerers mark a projection containing an invoke
non-runnable until the backend call/landing-pad ABI can select the exceptional
continuation and publish an active payload. The direct Oracle executes invoke
through `FZrExecIrOracleInvoke`: the callback supplies a pointer-free result
and explicitly selects the normal or exceptional successor. A rejected query
reports `ZR_EXEC_IR_DIAGNOSTIC_ORACLE_INVOKE_ERROR`; an exceptional path may
then consume the separate payload provider described above.

`PLACE_BASE` and `PLACE_PROJECT` are transported with their stable operand,
result, type, layout, and source metadata. The projections mark these place
operations non-runnable until a backend supplies the layout/address ABI; the
direct Oracle evaluates them through `FZrExecIrOraclePlace`, whose caller-owned
pointer-free result is the address token consumed by later memory providers.
Provider rejection reports `ZR_EXEC_IR_DIAGNOSTIC_ORACLE_PLACE_ERROR`; the
Oracle never manufactures a host pointer.

`ITER_INIT`, `ITER_MOVE_NEXT`, and `ITER_CURRENT` are transported with their
stable operand/result and ordered successor ranges. The projections mark the
iterator family non-runnable until the protocol and exception ABI is connected;
the direct Oracle executes them through `FZrExecIrOracleIterator`. The callback
returns a pointer-free iterator result and selects the normal or exceptional
edge; provider rejection reports `ZR_EXEC_IR_DIAGNOSTIC_ORACLE_ITERATOR_ERROR`,
and an exceptional edge may consume the payload provider above.

`ZrParser_ExecIr_LowerAot` uses the same builder and transfers ownership of the
projection arrays, adding the function token, signature hash, and execution
contract. It is an AOTIR seam only: `runnable` is deliberately false until the
07.01 adapter and 07.02 C/LLVM emitters exist. No projection stores a runtime
pointer or treats an opcode count as evidence of executable backend parity.

Both lowerers build into a prepared value and replace an existing output only
on success. Callers should initialize output records to zero and release them
with the matching `*_Free` function; oracle results use
`ZrCore_ExecIr_OracleResultInit/Free` for the same ownership discipline.
