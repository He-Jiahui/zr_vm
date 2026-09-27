# Source while short-circuit CFG (2026-09-27)

## Scope

Advance SSA plan 01.02 source-owned CFG production for statement-form `while`
whose top-level condition is `&&` or `||` with linear operands. The existing
logical expression producer emits conditional RHS and join blocks; the loop
branches at the logical join and its body backedge targets the original
condition entry. No nested logical operands, unmodeled RHS operations, or
new ExecBC reconstruction are claimed.

## Test and baseline

`tests/parser/test_ssa_source_while_short_circuit.c` checks both edge orders,
the two predecessors at the loop entry and logical join, RHS-only source
instructions, strict ExecIR build, and an unmodeled equality RHS remaining
analysis-only. Oracle cases compare returned booleans and source-backed RHS
assignment execution counts for short-circuit and evaluated paths of both
operators, with a 128-instruction limit guarding against a broken backedge.

Before the condition preflight change, the focused MSVC Debug static CTest
failed 2/3 cases at `preSemanticIrCfgActive`: `&&` and `||` each abandoned the
source CFG. The unmodeled RHS negative already passed. After the producer
change and the additional runtime cases, the focused CTest passed 5/5 cases.

## Validation boundary

The focused gate is `ctest --test-dir build/codex-ssa-conversion-msvc-static
-R '^ssa_source_while_short_circuit$' --output-on-failure --no-tests=error`.
The MSVC Debug static target build succeeded. The adjacent CTest selection
`ssa_construction`, `ssa_builder_cfg`, `ssa_source_straight_line_cfg`,
`ssa_source_while_short_circuit`, and `ssa_source_cleanup_cfg` passed 5/5.
The static `zr_vm_pre_semantic_ir_test.exe` passed 101/101. Windows MSVC 19.44
was used; the mounted WSL Ninja regeneration timeout seen in prior stages
was not counted as GCC or Clang validation for this stage. This gate does not
claim completion of the full SSA plan.
