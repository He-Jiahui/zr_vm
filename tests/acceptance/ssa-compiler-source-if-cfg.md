---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_source_cfg.inc
status: partial
---

# SSA 01.02: source `if` Semantic IR CFG

## Scope and failing baseline

The source compiler emitted ExecBC conditional jumps, but
`ZrParser_Compiler_ValidatePreSemanticIr` discarded any producer CFG and rebuilt
every function as one entry block connected to one exit block. The new
source-backed test failed with two blocks instead of the five required for an
entry, then arm, else arm, join, and exit.

The compiler now records a conditional SemIR `BRANCH` using the condition's
defined `ValueId`, ordered `TRUE_BRANCH` and `FALSE_BRANCH` edges, per-arm
unconditional branches, and non-overlapping instruction ranges. Nested `if`
diamonds are supported. If a later arm contains an unmodeled loop, return,
throw, cleanup, suspension, or switch path, the compiler rebuilds the non-branch
instruction and operand arrays, remaps value definitions, source entries, and
cleanup ranges, discards the partial CFG, and retains the legacy validation
path instead of publishing incomplete facts or rejecting valid source.

## Validation evidence (2026-09-18)

- MSVC 19.44 from `D:/zr-ssa-verify-871bc234`: latest target rebuild and
  direct `zr_vm_pre_semantic_ir_test.exe`, 17/17 passing.
- WSL GCC 11 from `D:/zr-ssa-verify-871bc234/wsl-gcc`: latest target rebuild
  and direct `zr_vm_pre_semantic_ir_test`, 17/17 passing.
- WSL Clang 14 from `D:/zr-ssa-verify-871bc234/wsl-clang`: regenerated build,
  full parser static-library rebuild, and direct focused test, 17/17 passing.
- The focused coverage includes a source `if` diamond, nested diamonds with
  every SemIR instruction owned by exactly one block, an initially unsupported
  loop arm, a later unsupported arm that abandons an already-started CFG, and
  a nested condition without a tracked ValueId that propagates that fallback
  to the outer `if`. The loop fallback case specifically contains `break`;
  the straight-line source `while` subset is covered by
  `ssa-compiler-source-while-cfg.md`, and the later short-circuit checkpoint is
  covered by `ssa-compiler-source-short-circuit-cfg.md`.

## Boundary

This is the source conditional-CFG slice of 01.02, not complete SSA
construction. General loop control (`break`/`continue`), return, optional
access, exception, cleanup and suspension edges remain to be modeled. Source
Place provenance, straight-line `while`, and linear-operand short-circuit now
build through ExecIR, but this historical `if` checkpoint by itself makes no
Oracle-equivalence or complete 01.02 acceptance claim.
