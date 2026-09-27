# SSA construction builder phi acceptance

## Scope

This checkpoint exercises Place promotion via the public SemanticIR-to-ExecIR
builder, beyond the direct `BuildSsa` promotion fixture. A canonical normal
diamond stores distinct scalar values in both arms and reads at the join.
The test checks exact phi predecessor/value order, rewritten load operands,
and core structure and SSA verification. An unread local prunes the phi; a
missing reaching store reports a positioned `INVALID_VALUE` diagnostic and
does not replace an already published output function.

## Baseline and test inventory

Before this change, `ssa_construction` had five cases for null/empty input
and standalone dominators, but no successful canonical SemanticIR diamond
through `ZrParser_ExecIr_Build`. The added two cases verify builder promotion
and pruning; the diamond case also checks transactional failure. All new
assertions passed against the existing implementation. This checkpoint closes
a coverage gap, not a newly observed production defect.

## Tooling evidence and results (2026-09-27)

The WSL GCC 11.4.0 Debug cache at
`/home/hejiahui/zrvm-ssa-nested-gcc.4pVemu` compiled the test object and
relinked the test after recompiling `exec_ir_build.c` and relinking the parser
library. The focused `ssa_construction` executable reported seven tests,
zero failures, and zero ignored. The builder compilation still emits three
preexisting `-Wmissing-braces` warnings.

After filling a missing cached test executable, the GCC CTest selection
`^(ssa_construction|ssa_builder_|ssa_place_(promotion|eligibility)|ssa_source_(straight_line_cfg|while_short_circuit|for_short_circuit|cleanup_cfg))`
passed 13/13. WSL Clang 14.0.0 rebuilt the construction test object and
the current builder object; four previously absent static-parser objects
were compiled before rebuilding its archive and relinking the executable.
Clang `ctest -R '^ssa_construction$' --output-on-failure --no-tests=error`
passed 1/1. Neither result represents a complete clean build or an MSVC or
sanitizer run for this checkpoint.

## Acceptance decision

The focused normal-diamond integration checkpoint passes. This is not the
01.02 exit gate: source-level optional/short-circuit, loop, exception,
cleanup and suspend shapes, four-backend parity, and the full validation
matrix still require independent evidence.
