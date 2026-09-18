---
doc_type: acceptance-record
plan: docs/plans/ssa/01-execir-ssa/02-ssa-construction.md
implementation:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_cfg.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_while.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement_flow.c
tests:
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_pre_semantic_ir_source_cfg.inc
status: partial
---

# SSA 01.02: source `while` SemanticIR CFG

## Scope

The source compiler now emits canonical control flow for a deliberately small
`while` subset without reading ExecBC jumps. A straight-line prefix branches
to a dedicated condition header. The header owns the condition facts and an
ordered true edge to the body plus false edge to the loop join. The body owns
its facts and a normal backedge to the header; later source facts start in the
join. The existing function-finalization path adds the fifth exit block.

The loop compiler captures semantic slot state before condition emission and
restores it on the false exit. Consequently body-only temporaries do not become
available after a zero-trip loop. The produced graph lowers through
`ZrParser_ExecIr_Build`; dominator/frontier construction recognizes both
header predecessors and scalar Place promotion installs one header phi with
entry and backedge incoming occurrences.

## Focused fixture

`test_source_while_emits_typed_backedge_cfg` compiles:

```text
var flag: bool = true;
while (flag) { flag = false; }
var after: int = 1;
```

It checks the exact five-block topology, edge kinds and targets, two header
predecessors, a defined conditional-branch operand, successful ExecIR
construction, and the loop-header phi's two incoming predecessor occurrences
and defined values. The adjacent fallback fixtures continue to require the
legacy two-block graph for a loop containing `break`, including when that loop
appears after source CFG construction has already started.

## Validation evidence (2026-09-18)

- MSVC 19.44.35228 debug build under `D:/zr-ssa-verify-871bc234` built
  `zr_vm_pre_semantic_ir_test` and passed 20/20 direct Unity cases.
- WSL GCC 11.4.0 and Clang 14.0.0 debug builds under the matching `wsl-gcc`
  and `wsl-clang` roots rebuilt the same target and each passed 20/20 direct
  Unity cases.
- The adjacent `ssa_builder_cfg`, `ssa_builder_dominance`,
  `ssa_builder_control_edges`, `ssa_builder_fact_identity`,
  `ssa_place_eligibility`, `ssa_place_promotion`, and
  `ssa_value_validation` CTest gate passed 7/7 on all three toolchains.
- A fresh WSL GCC build under `wsl-gcc-asan`, compiled with
  `-fsanitize=address,undefined -fno-omit-frame-pointer`, passed 20/20 with
  leak detection and both sanitizers configured to halt on the first error.

## Boundary

This checkpoint covers source-owned straight-line `while` construction and
its end-to-end loop-carried phi. `break`, `continue`, calls, short-circuit
conditions, return/throw, cleanup, suspension, optional access, and general
exceptional loop paths remain on the conservative legacy-CFG fallback. It
does not claim the complete 01.02 exit gate.
