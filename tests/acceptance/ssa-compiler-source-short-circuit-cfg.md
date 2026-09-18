---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_logical.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_source_cfg.inc
status: partial
---

# SSA 01.02: source short-circuit SemanticIR CFG

## Scope

The source compiler now publishes canonical control flow for `&&` and `||`
when both operands belong to the existing linear expression subset. The entry
block owns the left operand and conditional branch. For `&&`, its true edge
enters the RHS block and its false edge skips directly to the join. For `||`,
the true edge skips to the join and the false edge enters the RHS. Both forms
retain the builder's ordered `TRUE_BRANCH`, then `FALSE_BRANCH` contract.

The left value initializes a private temporary Place. The RHS block owns its
side effects and stores its value into that Place before its normal edge to
the join. The join loads one fresh expression value. This preserves the
runtime short-circuit behavior without inventing a branch-insensitive
`SET_STACK` value or making compiler temporaries eligible for scalar-local
promotion. An unsupported operand family abandons a partial source graph, if
present, and uses the legacy two-block validation path.

## Focused fixtures

`test_source_short_circuit_and_emits_rhs_control_flow` compiles an `&&` whose
RHS assigns a second local. It checks the exact four-block topology,
true-to-RHS and false-to-join targets, the RHS-to-join edge, two join
predecessors, a semantic branch, and successful ExecIR construction.

`test_source_short_circuit_or_skips_rhs_on_true_edge` uses the same observable
RHS assignment with `||` and checks true-to-join plus false-to-RHS. The
`test_source_if_arm_composes_short_circuit_cfg` fixture verifies that the
short-circuit diamond composes inside an already active outer `if` graph. The
`test_unmodeled_short_circuit_rhs_keeps_legacy_cfg` binary-expression fixture
continues to require two blocks and no published semantic branch.

## Validation evidence (2026-09-18)

- MSVC 19.44.35228 debug build under `D:/zr-ssa-verify-871bc234` built
  `zr_vm_pre_semantic_ir_test` and passed 23/23 direct Unity cases.
- WSL GCC 11.4.0 and Clang 14.0.0 debug builds under the matching `wsl-gcc`
  and `wsl-clang` roots rebuilt the same target and each passed 23/23 direct
  Unity cases.
- On MSVC, GCC, and Clang, the adjacent `ssa_builder_cfg`,
  `ssa_builder_dominance`, `ssa_builder_control_edges`,
  `ssa_builder_fact_identity`, `ssa_place_eligibility`,
  `ssa_place_promotion`, and `ssa_value_validation` gate passed 7/7.
- The WSL GCC ASan+UBSan build under `wsl-gcc-asan` passed 23/23 with leak
  detection and both sanitizers configured to halt on the first error.

## Boundary

This checkpoint covers one short-circuit operator level whose left and right
operands are literals, identifiers, or the supported linear assignment form.
Nested logical operands, computed binary operands, calls, optional access,
cleanup, suspension, and exception-producing RHS paths remain on the
conservative fallback. It does not claim the complete 01.02 exit gate.
