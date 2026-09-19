---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_licm.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_ssa.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/03-effects-verifier.md
  - docs/plans/ssa/guides/A-execir-builder-verifier.md
  - user: 2026-09-12 SSA implementation plan
tests:
  - tests/parser/test_ssa_effects_verifier.c
  - tests/parser/test_ssa_core_model.c
  - tests/parser/test_ssa_builder_iterator_invokes.c
  - tests/acceptance/ssa-effect-chain-continuity.md
  - tests/acceptance/ssa-cfg-edge-symmetry.md
  - tests/parser/test_ssa_loops_specialization.c
  - tests/parser/test_ssa_pass_manager_scalar.c
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

## Verification phases

`ZrCore_ExecIr_VerifyFunction` first runs storage, range, opcode-arity, value,
and edge-ID checks.  When requested, the structural phase additionally checks
entry/terminator shape.  The SSA phase is implemented in
`exec_ir_verify_ssa.c`; it builds fresh CFG facts and then validates value
definitions and uses.  The effect phase in `exec_ir_verify_effects.c` checks
memory/effect token continuity and the stricter PHI predecessor count/order
contract.

The structural phase requires every listed block successor to be listed as a
predecessor of its destination, and every predecessor to list the block as a
successor. It checks bounds and valid IDs before following either adjacency
list. A missing reverse edge reports `INVALID_BLOCK` with the edge owner's
block ID and both endpoints. This contract checks edge presence, not duplicate
edge multiplicity or correspondence between a terminator's per-instruction
successors and the containing block; these remain separate CFG obligations.

Within one block, successive observable instructions must consume exactly the
preceding observable instruction's `effectOut`: numerical growth alone does
not prove the chain. Pure instructions between observations leave this token
unchanged. Across blocks the current verifier still checks monotonic token
order only; predecessor-edge effect PHIs and region-specific memory version
proofs remain open M1 work, not a consequence of this local check.

The phases deliberately do not mutate cached analysis fields.  In particular,
`immediateDominator` is only a serialized hint: SSA verification recomputes
reachability and dominator sets from predecessor ranges for every invocation.

## SSA behavior model

The SSA verifier assigns every ordinary instruction result to its containing
block and rejects duplicate result IDs or overlapping/uncovered instruction
ranges.  A definition in the same block must occur before its use; a
definition in another block must dominate the use block.  Values without an
ordinary definition remain the model's explicit function-input/parameter
form.  A block PHI defines its result at block entry, and each incoming value
is checked against the predecessor edge named by that incoming.  Foreign PHI
predecessors are rejected at the SSA boundary; effect verification additionally
enforces one incoming per predecessor *edge occurrence* and exact range order.
Two distinct incoming slots may name the same source block when it has two
parallel edges to the destination; source-block uniqueness is not an invariant.

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
Coverage also includes matching parallel-edge PHI incoming slots and the existing
effect-token negatives. A skipped effect
version between two same-block calls yields the second call's source-identified
diagnostic; replacing it with the immediate predecessor token is accepted.
The standalone SSA consumer targets compile the split verifier source through
`tests/cmake/ssa-tests.cmake`; the full core library obtains it through the
module source glob.  The loop-specialization and scalar pass-manager fixtures
exercise PHI-aware LICM and post-pass revalidation.
`ssa_core_model` independently validates a reciprocal edge and rejects each
one-sided adjacency direction at the structural level with endpoint identity.

## Plan scope and follow-up

This document covers the verifier/dominance foundation of SSA correctness.  It
does not claim completion of canonical SemIR lowering, typed/layout checking,
state-map validation, or the remaining effect-region classes described by the
plan; those consumers continue to add their own focused gates.
