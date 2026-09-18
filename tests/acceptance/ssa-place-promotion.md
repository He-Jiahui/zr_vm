# SSA Place promotion acceptance

## Scope

This checkpoint promotes screened scalar Place roots across straight-line,
diamond, and loop control flow. `ZrParser_ExecIr_BuildSsa` computes live-in
facts, iterated dominance frontiers, pruned phis, and dominator-tree renaming.
Eligible stores become `NOP` instructions and eligible loads become `COPY`
instructions, preserving existing instruction and result IDs. Ineligible
Places keep explicit memory operations.

The transform is transactional: it rewrites a deep clone, verifies the
candidate, and publishes it only on success. A read without a reaching
definition reports `ZR_EXEC_IR_DIAGNOSTIC_INVALID_VALUE` and leaves the input
unchanged. A repeated call does not append duplicate phis.

This stage does not claim non-call throwing-operation splitting,
optional-chain semantics, or the full 01.02 exit gate. Canonical typed-call
blocks are normalized into an `INVOKE` chain before this pass; existing split
`INVOKE` normal/exception edges and critical/parallel predecessor occurrences
are covered by the promotion fixture.

## Focused evidence

On 2026-09-19, MSVC 19.44.35228, WSL GCC 11.4.0, and WSL Clang 14.0.0 debug
builds under `D:\zr-ssa-verify-871bc234` built and passed the same gate:

```text
ssa_builder_cfg
ssa_builder_dominance
ssa_builder_control_edges
ssa_builder_fact_identity
ssa_place_eligibility
ssa_place_promotion
ssa_value_validation

100% tests passed, 0 tests failed out of 7 CTest targets
```

The nine-case promotion fixture checks exact straight-line replacement, a two-arm
diamond phi, a loop-header phi with entry and backedge values, repeated
invocation, an ineligible Place, transactional read-before-definition failure,
parallel predecessor occurrences through a critical edge, split `INVOKE`
normal/exception availability, and post-transform core SSA verification.

## Remaining 01.02 work

Call-shaped exceptional blocks now split at every typed call and preserve
normal-only result availability. Non-call exceptional and cleanup
predecessors, optional access, try/finally, and the full 01.02 milestone gate
remain open.
