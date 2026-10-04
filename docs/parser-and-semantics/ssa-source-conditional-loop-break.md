---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_loop.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_loop.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_while.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/parser/ssa_source_execbc_vm_loop_break.inc
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_loop.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg_loop.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
plan_sources:
  - docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
  - docs/plans/ssa/01-execir-ssa/05-oracle-projections.md
tests:
  - tests/parser/test_ssa_source_execbc_vm.c
  - tests/parser/ssa_source_execbc_vm_loop_break.inc
  - tests/acceptance/ssa-source-conditional-loop-break.md
doc_type: module-detail
status: msvc-validated-linux-pending
---

# Source conditional loop break

## Scope and producer boundary

This finite extension admits a statement `if` containing a plain `break`
inside an already modeled while body. The source preflight checks both arms,
including the arm that a literal condition will not execute. Conditions must
belong to the existing linear or short-circuit condition subset. A terminating
arm cannot be followed by another nonempty statement in that same arm.

The shared for/foreach body analyzer retains its existing admission rules.
Ordinary conditional `continue` and conditional cleanup transfers remain
outside this extension. Existing dedicated try/finally handling retains its
own preflight and pending-completion protocol. This work does not implement
loop-carried variable promotion, value PHIs, live Place LOAD/STORE, side effects
in conditions, or production compiler path switching.

## Control flow

`compile_while_statement` assigns the active loop's semantic break target to
the loop join and its continue target to the condition block. An admitted
conditional break uses the existing abrupt-jump emitter in
`compile_statement_flow.c`; it ends its arm at the loop join. A condition-false
arm can fall through to the following body statement. The loop body preflight
and the `begin_if` arm preflight must agree before publishing this CFG.

The while preflight first applies the existing shared body analyzer, then
uses a dedicated conditional-break fallback in `compiler_semantic_cfg_loop.c`.
Its narrow declaration lives in `compiler_semantic_cfg_loop.h`; the large
`compiler_semantic_cfg.c` keeps only arm admission and orchestration changes.
The fallback treats both arms ending in break as an unconditional while-body
exit, and rejects subsequent nonempty statements.
For a single terminating arm, the other arm retains its fallthrough path.

## Source identity

Break/continue AST nodes now capture the existing token-location helper rather
than the lexer cursor position after the keyword. The semantic abrupt branch,
ExecIR source map and VM PC map preserve that original token range. This narrow
parser correction is required for both break sites to be identified at their
actual source offsets.

## Actual execution fixture

The shared source VM harness parses and compiles each source, validates its
compiler-owned semantic CFG, builds a published ExecIR function, runs all
ExecIR verifier checks, executes the Oracle, constructs an unoptimized ExecBC
projection, materializes a Core function with canonical types, and invokes
that function through the actual Core runtime dispatcher.

Two positive sources differ only in the inner `if` condition:

```zr
while (true) { if (true) { break; } break; }
return 9;
```

The false case executes the second break. Each test requires two conditional
branches, zero value PHIs, and both source break branches targeting the loop
join that contains the final return. The Oracle returns signed i64 9 and
emits no semantic effect events. The VM PC map must identify exactly one
executed break, at the expected first or second source offset. Every observed
PC maps to its original ExecIR source ID; the collapsed block path follows
declared CFG edges and ends at the Oracle's final block.

The Oracle API exposes the final block and semantic effect events, without a
complete control-flow history. These assertions therefore establish the
expected VM branch selection and absence of effect events; they do not claim
complete Oracle/VM control trace equality.

Negative sources retain a conditional continue or put a break inside
try/finally in a conditional arm. They must compile through the legacy source
compiler while declining source CFG publication, ExecIR construction,
projection, materialization, and VM execution in this fixture.
Additional for and foreach sources require the same nonpublication boundary,
preventing this while-only extension from widening their producer contracts.

## Validation status

The current validation record is
[`ssa-source-conditional-loop-break.md`](../../tests/acceptance/ssa-source-conditional-loop-break.md).
This finite fixture does not close 01.02, 01.05, or source loop-PHI milestones.
