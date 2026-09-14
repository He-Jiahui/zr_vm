# ExecIR vectorization planning contract

`ZrParser_ExecIr_VectorizeLoops` performs the legality and profitability part
of 09.03.  It consumes the loop facts produced by
`ZrParser_ExecIr_AnalyzeLoops` (or computes them when no facts are supplied)
and publishes one address-free `SZrExecIrVectorizeLoopPlan` row per selected
loop.  The fixed-width ExecIR schema has no lane-bearing opcode, so this pass
does not rewrite the function.  C/LLVM/JIT lowerers may consume a `VECTOR`
row; all other rows retain the original scalar loop.

The planner rejects malformed ranges, irreducible or multi-entry loops,
unknown trip counts, loop-carried phi/dependence evidence, effectful bodies,
unknown bounds/stride, and unsafe aliases.  An unknown but checkable alias is
represented by `needsAliasGuard`; a backend must branch to the unchanged
scalar loop when that guard fails.  Negative strides are legal when the
caller-provided stride proof is true.  `usesMaskedTail` is set only when the
target advertises mask support; otherwise the tail is explicitly scalar.

Numeric legality is delegated to the 09.02 `ZrParser_ExecIr_LegalizeVector`
contract.  Thus strict ordered semantics remain scalar, while an approved
target/policy combination records lane count, vector iterations, tail lanes,
and the policy-derived contract hash in the backend's own artifact key.

The row cost model is intentionally deterministic: scalar work is
`bodyInstructions * tripCount`; vector work accounts for vector iterations,
tail work, and alias-guard bridge cost.  Small trips, code-size limits, and
bridge budgets produce named fallback reasons rather than silently enabling a
slower version.  Optional `SZrExecIrRemarkSink` rows use pass name
`vectorize`, carry source identity and the same before/after IR hash (because
planning is non-mutating), and expose the exact decision reason.

The focused fixture `tests/parser/test_ssa_vectorize_pass.c` covers a guarded
load loop with a one-lane tail, alias/effect fallback, unknown-trip and
disabled-pass decisions, bounds/stride proof failures, and a legal negative
stride.  A platform without vector capabilities still receives the same
scalar plan and remains buildable.
