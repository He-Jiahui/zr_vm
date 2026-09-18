# SSA source branch slot isolation acceptance

## Scope

This slice isolates the compiler's stack-slot-to-ValueId bridge across source
`if` arms. A branch-entry snapshot is restored before compiling the sibling arm
and at the join, including recursively nested diamonds. Canonical instructions,
values, Places, and CFG ranges from every arm remain in pre-SemanticIR.

The source regressions cover sibling locals whose temporary stack slots are
recycled, a nested diamond, and a local declared after the outer join. Both
fixtures validate pre-SemanticIR and build structurally and SSA-valid ExecIR.

## Evidence

- RED: the sibling-local fixture reached `ZrParser_ExecIr_Build` and failed SSA
  dominance because the else-arm conversion consumed a temporary defined only
  by the then arm.
- GREEN: MSVC, WSL GCC, and WSL Clang each passed the general parser test
  74/74 and `zr_vm_pre_semantic_ir_test` 19/19, including the sibling-local
  and nested/post-join ExecIR builds.
- The same three toolchains each passed `ssa_core_model`, `ssa_builder_cfg`,
  `ssa_builder_dominance`, `ssa_builder_control_edges`, and
  `ssa_builder_fact_identity` 5/5.

The dirty user-owned `ssa_construction` and `place_cfg_graph` fixtures are not
counted as evidence. This acceptance does not claim Place promotion, phi
insertion/rename, exceptional-result availability, or the 01.02 exit gate.
