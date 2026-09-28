---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_opcode.def
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_edge_identity.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/exec_ir_opcode.def
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_edge_identity.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effect_backedges.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
  - docs/plans/ssa/guides/A-execir-builder-verifier.md
  - user: 2026-09-12 SSA implementation plan
tests:
  - tests/parser/test_ssa_effects_verifier.c
  - tests/parser/test_ssa_core_model.c
  - tests/parser/test_ssa_place_promotion.c
  - tests/acceptance/ssa-value-phi-edge-order.md
  - tests/parser/test_ssa_builder_iterator_invokes.c
  - tests/parser/test_ssa_builder_control_edges.c
  - tests/acceptance/ssa-effect-chain-continuity.md
  - tests/acceptance/ssa-cfg-edge-symmetry.md
  - tests/acceptance/ssa-cfg-parallel-edge-reciprocity.md
  - tests/acceptance/ssa-exception-payload.md
  - tests/acceptance/execir-tagged-memory-required-regions.md
  - tests/parser/test_ssa_loops_specialization.c
  - tests/parser/test_ssa_pass_manager_scalar.c
  - tests/acceptance/ssa-pass-failure-snapshot.md
  - tests/cmake/ssa-tests.cmake
doc_type: module-detail
---

# ExecIR structural, SSA, and effect verifier

## Purpose

The core verifier is the loader and optimization safety boundary for ExecIR.
It validates untrusted side pools before any consumer follows a range, then
checks control-flow structure, value SSA, and observable effect tokens in a
fixed dependency order.  Parser passes call the same entry point before and
after transformations, while the parser-facing wrapper remains a thin
delegation layer.

Instruction-specific structural diagnostics also carry the instruction's
`sourceId` when its storage and ID are available.  Malformed storage and
out-of-range IDs remain safe to report without dereferencing the instruction
pool; those diagnostics have no source position.

## Verification phases

`ZrCore_ExecIr_VerifyFunction` first runs storage, range, opcode-arity, value,
and edge-ID checks.  When requested, the structural phase additionally checks
entry/terminator shape: a nonempty block must end in a terminator, and no
earlier instruction in that block may terminate it. An early terminator reports
`MISSING_TERMINATOR` at that instruction with the expected final instruction
ID. The SSA phase is implemented in
`exec_ir_verify_ssa.c`; it builds fresh CFG facts and then validates value
definitions and uses.  The effect phase in `exec_ir_verify_effects.c` checks
memory/effect token continuity and the stricter PHI predecessor count/order
contract. A graph-analysis module shared with parser effect synthesis
classifies actual loop backedges without trusting block declaration order or
serialized dominator hints.

The structural phase pairs every listed block successor with the same-numbered
occurrence of that source in the destination's predecessor row, and vice
versa. It checks bounds and valid IDs before following either adjacency list.
Missing reverse occurrences report `INVALID_BLOCK` with the edge owner's block
ID and both endpoints. After validating both ranges and their targets, the
structural phase also requires each block's final terminator to list exactly
the same number of successors in the same order as its containing block.
Separate successor-pool rows are valid when every ordered target agrees;
otherwise `INVALID_BLOCK` identifies the block, terminator and first mismatched
count or target. This keeps a terminator edge ordinal identical to the block
edge ordinal used to identify phi incoming slots, including parallel edges.

Within one block, successive observable instructions must consume exactly the
preceding observable instruction's `effectOut`: numerical growth alone does
not prove the chain. Pure instructions between observations leave this token
unchanged. A block join may now publish an explicit `effectPhiResult` and an
edge-ordered `effectPhiIncomings` range (reusing the `phiIncoming` pool). The
effect verifier checks each incoming predecessor and terminal token, requires
the merged result to advance beyond its forward inputs, and requires the first
observable instruction in the block to consume that result. A join with
distinct predecessor tokens is rejected when it omits this phi metadata;
single-token forwarding through pure blocks remains valid. Tagged memory
versions still use the region-local rule described below. Reducible
declaration-ordered loops also accept a backedge token produced after
the header phi, while requiring that it match that predecessor's terminal
token exactly.

Memory tokens now have a compatibility-preserving tagged form:
`ZR_EXEC_IR_MEMORY_TOKEN_MAKE(region, version)`. Tagged tokens carry one of
the eight declared memory regions (frame, managed heap, module global,
native/FFI, GC, ownership, scheduler/task, or I/O) and are ordered by version
within that region and within each block. Independent tagged regions therefore
do not impose a false global order, and sibling/loop-body blocks do not inherit
one another's linear monotonicity state. After the first tagged touch of a
region in a block, a read must consume exactly its current version; a later
version cannot appear without an intervening write. Writes still advance
their region, and the first touch remains subject to CFG entry and phi checks.
The verifier rejects a tagged token whose region is not
covered by the opcode's declared read/write mask, rejects zero versions, and
continues to apply the legacy function-wide monotonic rule to untagged tokens
so old artifacts remain readable during the migration. For each direction,
an all-tagged range must cover every region in the opcode's schema mask; a
repeated region cannot replace a missing one. A direction containing an
untagged token retains its compatibility path. A join may publish one tagged
`memoryPhiResult` and edge-ordered `memoryPhiIncomings` range per
region. Each incoming must match the predecessor's terminal version and
region, the result must advance beyond forward incoming versions, and the first
tagged memory consumer must consume the result. Distinct region versions
without a memory phi are rejected. A first read of an untouched region
establishes its initial version at the block exit, so a later join can
distinguish that path from a sibling write. A declaration-order backedge may carry a
later iteration's higher version, but a stale or wrong-region backedge still
fails exact predecessor-terminal matching. A reachable predecessor counts as
a backedge only when the header dominates it in the actual CFG, regardless of
either block's ID; reverse-declared acyclic joins must still advance their phi
results beyond every incoming version. Parser production handles single
blocks, acyclic CFGs and reducible loops with multiple headers or latches,
including a header declared after its latch and reverse-declared non-backedge
predecessors. Irreducible CFGs still
require a separate fixed-point construction: the effectful producer now
reports `UNSUPPORTED` instead of silently publishing empty token fields;
pure graphs do not require token synthesis.

The phases deliberately do not mutate cached analysis fields.  In particular,
`immediateDominator` is only a serialized hint: SSA verification recomputes
reachability and dominator sets from predecessor ranges for every invocation.

## SSA behavior model

The SSA verifier assigns every ordinary instruction result to its containing
block and rejects duplicate result IDs or overlapping/uncovered instruction
ranges.  A definition in the same block must occur before its use; a
definition in another block must dominate the use block.  Values without an
ordinary definition remain the model's explicit function-input/parameter
form. A block PHI defines its result at block entry. Value SSA verification
requires exactly one incoming per predecessor *edge occurrence*, in the same
order as the block's predecessor row; it checks dominance and exceptional-edge
availability for the value selected by that slot. A swapped pair of different
predecessors, a missing incoming, or a foreign predecessor reports
`PHI_PREDECESSOR_MISMATCH` at the destination block. Effect verification applies
the same exact slot count/order requirement to memory and effect phis. Two
distinct incoming slots may name the same source block when it has two parallel
edges to the destination; source-block uniqueness is not an invariant. The
current model identifies such edges by their ordered occurrence in both rows,
not a separately serialized edge ID.

The result of any value-producing terminator that may throw is committed only
on a normal continuation. This covers generic `INVOKE` plus iterator init,
advance, and current-value retrieval without hard-coding one opcode. If an
operand or PHI incoming reaches a successor marked
`ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION`, or any block reachable from that successor,
the SSA phase reports
`ZR_EXEC_IR_DIAGNOSTIC_EXCEPTION_EDGE` with the use site and defining
instruction identity; block dominance by itself is not treated as proof that
the result exists on that path.

Structural verification first requires this terminator family to own exactly
two ordered, distinct successors. Successor zero must be an ordinary block and
successor one must carry `ZR_EXEC_IR_BLOCK_FLAG_EXCEPTION`; instruction-owned
and block-owned successor rows must agree. A missing or misplaced exception
marker therefore fails closed before SSA reachability can mistake the
exceptional continuation for a normal one.

`EXCEPTION_PAYLOAD` defines the value delivered at a handler entry without
pretending it is an operand of the throwing instruction. The structural phase
requires its containing block to carry the exception flag, not be the function
entry, and be the direct exceptional target of every predecessor's may-throw
terminator. A normal or implicit function entry into the same block is invalid
because no active payload exists on that path.
A handler block may contain at most one such definition. Violations use
`EXCEPTION_EDGE` and retain the
payload instruction's block, instruction, and source identity. The parser
builder runs structure and SSA verification together before publishing its
transactional result, so a hand-built payload in an ordinary block cannot
escape through the former SSA-only publication gate.

Dominators use a reachable-block bit set and an iterative predecessor
intersection.  Unreachable rows are kept empty, so a definition from an
unreachable block cannot accidentally prove a reachable use.  The algorithm
uses checked side-pool ranges established by the structural validation and
frees all temporary matrices on both success and failure.

## Diagnostics and compatibility

Dominance failures use `ZR_EXEC_IR_DIAGNOSTIC_DOMINANCE` and preserve the
function, block, instruction, source, value, and expected-definition identity
in `SZrExecIrDiagnostic`.  Invalid ranges and values retain their existing
diagnostic codes, allowing callers to distinguish malformed storage from an
SSA ordering error.  The optional `SZrExecIrValue.definition` back-pointer is
checked when a result is declared, but it is not used as the dominance proof;
a mismatch is reported as a duplicate-definition diagnostic.

LICM treats PHI incoming values as real edge uses when deciding whether a
hoisted result is live.  This keeps a repaired zero-iteration loop fixture
semantically equivalent and prevents a value used only by an exit PHI from
being incorrectly discarded.

## Test coverage

`ssa_effects_verifier` covers linear use-before-definition, cross-branch
non-dominating uses, valid PHI edge definitions, and wrong-edge diagnostics in
addition to direct, cleanup-path, and PHI-input exceptional-edge `INVOKE`
result negatives. `ssa_builder_iterator_invokes` applies the same result
availability rule to the three iterator invoke terminators and directly
rejects missing or misplaced exception markers at the core structure boundary.
Coverage also includes matching parallel-edge PHI incoming slots, explicit
cross-block effect-token joins, missing/stale effect-phi inputs, loop-carried
effect/memory phis with stale backedge negatives, and the existing
effect-token negatives. A tagged read that skips ahead of the current
same-block memory version reports its source, block, instruction, expected
and actual versions. The tagged CALL fixture accepts its full managed-heap and
native-FFI token sets, then independently removes one input and one output
region and checks `MEMORY_TOKEN` diagnostics with function, block, instruction,
and source identity. Legacy untagged token cases remain covered. A skipped effect
version between two same-block calls yields the second call's source-identified
diagnostic; replacing it with the immediate predecessor token is accepted.
The standalone SSA consumer targets compile the split verifier source through
`tests/cmake/ssa-tests.cmake`; the full core library obtains it through the
module source glob.  The loop-specialization and scalar pass-manager fixtures
exercise PHI-aware LICM and post-pass revalidation.
`ssa_core_model` independently validates reciprocal and matching duplicate
edges, and rejects each one-sided adjacency direction and unpaired duplicate
at the structural level with endpoint identity.

## Plan scope and follow-up

This document covers the verifier/dominance foundation of SSA correctness.  It
does not claim completion of canonical SemIR lowering, typed/layout checking,
state-map validation, or the remaining effect-region classes described by the
plan; those consumers continue to add their own focused gates.
