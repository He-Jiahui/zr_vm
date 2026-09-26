---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_interpreter.h
  - zr_vm_core/include/zr_vm_core/execution_contract.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_run.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_resume.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_validate.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_internal.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_phi.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_oracle.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_projections.h
  - zr_vm_parser/include/zr_vm_parser/exec_ir_execbc.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_oracle.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_phi.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_phi.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_consumer.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_execbc.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_run.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_resume.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_validate.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_internal.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter_phi.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_oracle.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_phi.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_consumer.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_execbc.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
plan_sources:
  - user: 2026-09-12 SSA plan implementation
  - docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
  - docs/plans/ssa/01-execir-ssa/04-state-maps.md
  - docs/plans/ssa/guides/A-execir-builder-verifier.md
  - docs/plans/ssa/guides/E-projections-fusion-aot.md
tests:
  - tests/parser/test_ssa_oracle_resume.c
  - tests/parser/ssa_oracle_resume_fault_allocator.c
  - tests/parser/ssa_oracle_resume_fault_allocator.h
  - tests/acceptance/ssa-oracle-resume.md
  - tests/parser/test_ssa_oracle_projections.c
  - tests/parser/test_ssa_oracle_resume.c
  - tests/parser/test_ssa_oracle_memory_differential.c
  - tests/parser/test_ssa_oracle_call_differential.c
  - tests/parser/test_ssa_oracle_invoke_differential.c
  - tests/parser/test_ssa_oracle_parallel_edges.c
  - tests/acceptance/ssa-projection-cfg-flags-sparse-slots.md
  - tests/acceptance/ssa-projection-source-spans.md
  - tests/harness/ssa_differential_support.c
  - tests/harness/ssa_differential_support.h
  - tests/cmake/ssa-tests.cmake
  - tests/acceptance/ssa-oracle-parallel-edges.md
  - tests/acceptance/ssa-projection-parallel-edges.md
  - tests/acceptance/ssa-projection-phi-schedule.md
  - tests/acceptance/ssa-execbc-scalar-runner.md
  - tests/acceptance/ssa-oracle-execbc-parallel-differential.md
  - tests/acceptance/ssa-oracle-execbc-memory-differential.md
  - tests/acceptance/ssa-oracle-execbc-call-differential.md
doc_type: module-detail
---

# ExecIR oracle and initial projections

01.05 provides a pointer-free reference execution seam and two transactional,
no-optimization projections. `ZrCore_ExecIr_RunOracleEx` validates the complete
function shape before reading pools, allocates an isolated value environment,
and commits the result after a normal return, throw, suspend or requested
checkpoint pause. The
legacy `ZrCore_ExecIr_RunOracle` entry remains a compatibility counter for
callers that only need instruction coverage.

The oracle executes the scalar/control subset (constants, copies and
conversions, checked integer/floating arithmetic, comparisons, branches,
switches, phi entry, return, throw, and suspend) directly. Store, load, drop,
barrier, call, invoke, iterator, place, allocation, type-test, and exception
payload operations use explicit caller providers whenever their semantics
depend on runtime state. Provider-backed operations receive only pointer-free
operand/value records, and observable operations append bounded operand
snapshots to the event stream. `TYPE_TEST` receives the canonical
`matchTypeToken` and returns either a boolean membership result or a rejection;
false membership is a successful result, while rejection reports
`ZR_EXEC_IR_DIAGNOSTIC_ORACLE_TYPE_TEST_ERROR`. Native landing-pad state and
resume after a caught exception remain outside the pointer-free oracle.

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
`exec_ir_interpreter_phi.c`; `exec_ir_interpreter.c` owns instruction dispatch.
`exec_ir_interpreter_validate.c` preflights the input graph, while
`exec_ir_interpreter_run.c` owns result lifetime and the shared execution loop.
`exec_ir_interpreter_resume.c` validates checkpoints, restores live values and
reconstructs the cursor. Their private header exposes these module boundaries;
fresh execution and resumed execution use the same instruction semantics.

## Checkpoint execution and resume

`SZrExecIrOracleInput.stopAt` optionally selects a state-map source ID, resume ID
and phase. Its map is validated before execution. A matching dynamic occurrence
returns a result with `paused` set. BEFORE_EFFECT stops before the instruction;
AFTER_EFFECT and CLEANUP_COMPLETE stop after its committed value/effect update.
If execution finishes on another path before reaching the selector, the result
has its ordinary return/throw/suspend status. No map is needed for a run without
a selector.

`ZrCore_ExecIr_ResumeOracleEx` consumes an API-owned paused result in place. It
validates the saved function/generation/signature and checkpoint identity,
including exact zero-valued identities,
prepares a new environment containing only the map's live values, and copies
the existing event history. Initial input values never overwrite restored
state. Missing or undefined live values fail with checkpoint source/instruction
diagnostics. Preparation failures leave the old arrays and pause available for
repair and retry.

After preparation, the new environment replaces the old one and consumes the
pause token before any provider executes. A later execution error retains its
partial result with that pause consumed; repeating resume is rejected. This is
essential for providers whose side effects cannot be rolled back. Result
records must use Init/Free and must not be shallow-copied into independently
owned or independently resumed states.

The saved cursor retains a terminator's selected successor ordinal, so resumed
parallel edges select the same phi input. A checkpoint already inside a block
restores phi results directly and does not re-enter the block. Keeping the same
BEFORE_EFFECT selector skips that paused occurrence once; a later loop visit
can pause again. Each invocation has its own step budget, while instruction and
event counts accumulate across successful resumes.

An after-map includes values from both normal and exceptional successors. On
an actual exceptional edge, the throwing terminator's normal result IDs are
excluded from required/restored values; all other mapped live IDs remain
required. The execution loop clears these result slots even without a pause,
so an earlier iteration's result cannot survive a later throw. This matches
the CFG ownership analysis's result availability rule and preserves the
selected exception edge without calling the provider again.

These values are pointer-free oracle scalars/tokens. This interface establishes
reference execution recovery, not native frame switching, runtime GC object
materialization, scheduler activation, or nested inline-frame reconstruction.
The caller retains the function, constants and provider contexts throughout an
invocation and must not mutate function metadata inside a provider.

THROW and SUSPEND are observable termination boundaries in reference mode. The
oracle publishes their event into its prepared result, stops before any later
instruction, and commits `terminatedByThrow` or `suspended` respectively. The
payload-bearing SUSPEND form additionally copies its first operand to the
result slot and return-value snapshot. An explicit post-SUSPEND checkpoint can
resume at the block's single continuation successor (or the next instruction
in blockless form), without publishing a second suspend event. Consuming that
checkpoint clears the old suspension's return-value snapshot, so a later void
return or payloadless suspension cannot inherit it; mapped SSA values remain
available independently. A normal suspend
result without a requested checkpoint is not a resumable cursor. Selecting a
handler block from a runtime exception remains a separate runtime ABI.

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
source-map entry retains its source ID, projected PC, byte offsets, and
start/end line and column in both ExecBC and AOTIR; the records do not alias
the input function. An invalid source-map instruction ID rejects a candidate
without replacing the previously published AOTIR projection. This debug
metadata is not yet consumed by the C/LLVM emitters. Every value receives a
distinct slot while optimization is disabled. Phi incoming
assignments retain edge-tagged parallel-copy records. The `phiMoves` array
also orders physical-slot moves per projected predecessor edge, so executing
only moves for the selected edge reproduces simultaneous phi assignment.
For a `CONVERT` instruction with no explicit target token, lowering resolves
the effective scalar type from its result value before the runner executes it.
Dependency moves precede the overwrites they depend on; cycles save one
destination into a reusable `phiTemporarySlot` before rotating the values.
`temporarySlotCount` is zero without cycles and one otherwise. The temporary
slot follows both the largest mapped physical slot and the reserved frame
storage range. A no-reuse packed frame maps logical value IDs to the physical
slot positions in its slot array; a packed frame that reuses slots and a
custom frame whose mapped value slots alias are rejected until liveness-aware
lowering exists. Self-copies are omitted. Capacity overflow or allocation
failure rejects the candidate without replacing an earlier projection.
`valueSlotCount` counts logical values; `physicalSlotCount` includes reserved
and sparse frame slots, and only occupied slots carry `slotValues` metadata.
The pointer-free runner allocates physical capacity plus any phi temporary,
so a sparse slot or reserved range cannot be mistaken for the number of
logical values. Phi-cycle overflow is diagnosed before allocating metadata.
`ZrParser_ExecBcProjection_ExecutePhiMoves` is the first production consumer
of this plan. It validates the selected projected edge and every source/
destination slot before calling the backend's slot-copy callback. A bad range
therefore cannot begin a partial copy; if a callback rejects after earlier
moves, the callback owns any external rollback policy and the diagnostic
identifies the failing move.
Critical CFG edges and phi-bearing edges leaving a branching block are split
into synthetic empty blocks in the projection, preserving the source function
and keeping phi copies on an identifiable edge-local block. Both projections
retain original blocks' entry/exception/cleanup flags; synthetic split blocks
carry no flags. Parallel CFG
edges are paired by their occurrence number in the source successor and
destination predecessor rows. Each critical occurrence gets its own split
block; phi incoming predecessors are rewritten to the projected predecessor
row, and each nontrivial copy is tagged with that edge's projected block ID.
When a terminator refers to a distinct successor pool range, that range is
rewritten by the same target-occurrence identity as the source block's range.
Mismatched adjacency multiplicities fail preflight without replacing a
previously published projection. AOTIR owns the same move plan, but remains
non-runnable. `ZrParser_ExecBcProjection_Run` consumes the projection's owned
instruction and CFG arrays with an isolated pointer-free slot environment.
It supports the initial scalar/control opcode set, executes phi moves on the
selected projected predecessor, and follows each terminator's rewritten
successor range (which can differ from the block's successor range). An
explicit step limit bounds loops; failed runs release the candidate slots
without replacing an earlier result. Initial values are indexed by logical
value ID, and the optional constants pool is indexed by `CONSTANT.layoutId`.
The result owns its physical slots until `ZrParser_ExecBcExecutionResult_Free`.
`LOAD` and `STORE` are also executable when their verifier-preserved memory
tokens and effect flags form a valid function. They pass pointer-free operands
and the projected instruction to an optional caller-owned memory provider;
`LOAD` requires a provider and a defined returned value, while `STORE` can
also record an event without a provider, matching the oracle's event-only
mode. Successful reads and writes append owned, ordered operand snapshots
with executed instruction and source IDs to the result. Event capacity is
reserved before invoking a provider; callback rejection and invalid read
values report the failing instruction/source, release the candidate result,
and leave an earlier published result intact. The provider owns external
memory mutations and must decide how to handle its own rejected operations.
On return it also records the executed block, instruction ID, and source ID,
allowing a differential fixture to emit a return event from the executed
projection, not from an assumed source path. The parallel-edge fixture runs
the verified ExecIR through the independent direct oracle and projected
runner, compares actual result and return-event observations with
`ZrTests_Ssa_Compare`, and records both backend identities. A separate
successor range exercises the projected synthetic-edge rewrite for both
conditional branches and switches. A test-only return-source corruption
must fail at event index zero; no production fallback is involved. The
effect-memory fixture verifies STRUCTURE, SSA, and EFFECT, then executes both
backends against separate but identical caller-owned memory states. It
compares STORE, LOAD, and RETURN event order and payloads, and checks memory
contents, missing/rejected providers, invalid load values, and repeated runs.
An independently verified STORE-only function also checks the provider-free
event-only mode. Address snapshots are compared as well as stored values, and
corrupting the observed address must fail the event comparison.
The projection runner also executes ordinary `CALL` through an explicit
`FZrExecBcCall` callback. All operand values, including arguments beyond the
four-event-snapshot limit, reach the caller-owned provider in their original
order; up to four are copied into the owned CALL event after a defined return
value is checked. More than eight operands use temporary allocation, released
on both success and failure. Missing callbacks report UNSUPPORTED; rejected
callbacks report ORACLE_CALL_ERROR, and undefined callback results report
INVALID_VALUE, all at the call's source/instruction. A verified five-argument
fixture compares oracle/projected result and each event snapshot, and a
nine-argument fixture exercises the temporary-allocation path. Neither
callback ABI represents runtime exceptions or an external rollback protocol.
The runner rejects out-of-range initial-value and constant kinds during input
preflight, as the oracle does, before an invalid operand can reach the callback.
The projected runner reserves event capacity before CALL side effects; the
oracle appends its event after the callback, so their provider invocation
timing differs if event allocation fails. This fault is not covered by the
successful-event differential contract.
The projection also owns one value fact per physical slot (`slotValues`),
transferred to AOTIR and released with either projection. The projected
execution result owns a parallel `ownerStates` array. MOVE marks its source
MOVED, and DROP snapshots the owned value in a bounded event before clearing
the slot and marking it DROPPED. `DROP_IF_INITIALIZED` in a verifier-approved
cleanup block drops only an INITIALIZED owner; UNINITIALIZED, MOVED, and
DROPPED states are no-ops. Reading a consumed owner reports INVALID_VALUE at
the executed instruction/source. Verified single-block and conditional
branch-join fixtures compare the direct oracle's result, DROP event order,
and owner states against the projected runner, including an uninitialized
join-path skip. Ownership event-allocation failures are not fault-injected.
`BARRIER` also runs as a pointer-free effect event. Its one value operand is
validated before recording a bounded snapshot; the projection retains its
managed-heap/GC memory versions and source/instruction IDs. Two verified
barriers with distinct operands compare event order and values against the
direct oracle, while a bad operand preserves an earlier published result.
This records a write-barrier observation only; no production GC barrier is
installed by the projection runner.
`THROW` with a defined pointer-free payload also runs as a terminal event.
The result marks `terminatedByThrow`, leaves `returned` false, and snapshots
the operand with its source/instruction identity. A verifier-valid fixture
compares this terminal observation and payload with the direct oracle; a bad
projected payload reports INVALID_VALUE without replacing an earlier result.
This does not implement exception-handler entry, landing pads, or resume.
`SUSPEND` also records a bounded event and terminates the projected run with
`suspended` true. With an operand, its first value becomes the returned
payload and SSA result, as in the oracle. Every variadic operand is validated
before the event is published, even when only the first four are copied into
the event. Verifier-valid one- and five-operand fixtures compare the terminal
state, payload, result slot and bounded event snapshots against the oracle.
This is a terminal observation only, not checkpoint/resume execution.
Only projections whose opcodes have a runner implementation are marked
`runnable`; suspend/resume restoration and production runtime callback wiring
still require a later backend ABI.
This small runner is not the VM's default ExecBC dispatcher, does not emit
bytecode for it, and establishes no C/LLVM or full effect-event parity. The
direct differential currently covers scalar/control returns, pointer-free
LOAD/STORE memory providers, provider-backed ordinary CALL, and ownership
MOVE/DROP/conditional cleanup, BARRIER observations, terminal THROW, terminal
SUSPEND, and provider-backed INVOKE with a pointer-free handler payload.

`TYPE_TEST` is also transported by both initial projections with its separate
`matchTypeToken` side field. This preserves canonical subtype identity for a
later backend adapter, but the projections set `runnable` to false whenever a
type test is present; they do not invent an executable subtype ABI. The direct
Oracle provider described above remains the only executable reference seam.

`ALLOC` likewise remains metadata-only in ExecBC and AOT projections: a
projection containing allocation is marked non-runnable until the backend
allocator/GC ABI is connected. The direct Oracle's pointer-free allocation
provider does not change that projection boundary.

`EXCEPTION_PAYLOAD` retains its stable opcode, ranges, and source identity in
both projections. The ExecBC projection runner accepts a caller-owned
`FZrExecBcExceptionPayload` for a defined pointer-free handler result; a
missing provider is explicitly unsupported. This is not a VM exception object
or a native landing-pad ABI. AOTIR remains non-runnable.

`INVOKE` retains its result, operands, and ordered normal/exception successors.
The ExecBC projection runner accepts `FZrExecBcInvoke`, snapshots an ordered
CALL event, then selects the supplied edge. The exceptional edge clears any
normal result and can read a separate handler payload; a missing callback is
unsupported, a rejected invocation reports ORACLE_INVOKE_ERROR, and an
undefined normal result reports INVALID_VALUE. A malformed zero-successor
INVOKE remains non-runnable. The direct Oracle has equivalent pointer-free
providers; neither projection implements a native landing pad or resume.
For an `AFTER_EFFECT` checkpoint on INVOKE, the resume fixture compares the
paused CALL event with independent projected execution, then compares the
oracle's completed CALL/RETURN trace and return block after resume. Both
normal and exceptional paths must invoke their provider exactly once; the
projection still runs uninterrupted and does not reconstruct a checkpoint.

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
