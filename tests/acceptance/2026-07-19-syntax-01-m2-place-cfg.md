# Syntax Plan 01 M2 Place And General CFG Acceptance

## Status

- State: completed.
- Plan: `docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md`, M2 Place and general CFG.
- Scope: Place identity and projections, four-state overlap, dynamic typed CFG edges, explicit cleanup routing, suspension representation, and union-exhaustive default reachability.

## Acceptance Inventory

- Place bases: local, parameter, `this`, static, temporary, return slot, and external handle.
- Place projections: field, dynamic index, constant index, dereference, union variant, and tuple element.
- Place metadata: stable in-session `PlaceId`, parent trace, canonical `TypeId`, flattened projection path, and source range.
- Overlap lattice: equal, disjoint, overlap, and unknown, including conservative dereference, dynamic-index, external-handle, and union-storage cases.
- CFG storage: unbounded dynamic outgoing-edge array, typed edge records, complete successor count, and a two-slot compatibility prefix that is not a semantic capacity.
- CFG edge kinds: normal, true, false, switch case, switch default, exception, cleanup, return, suspend, and resume.
- CFG blocks and terminators: cleanup/suspension block kinds, branch/switch/return/throw/break/continue/suspend/cleanup-dispatch/exit terminators, and M3 instruction-range placeholders.
- Cleanup: return and throw use required cleanup subgraphs; no direct return-to-exit bypass is added when a cleanup edge exists.
- Union switch: exhaustive union coverage marks a redundant default unreachable; a non-exhaustive switch keeps its default reachable.

## TDD Evidence

- The first M2 target failed at compile time because `zr_vm_parser/place.h` did not exist.
- The focused test then drove all Place bases/projections, four overlap states, a five-successor switch node, every typed edge kind, and builder-produced return/branch terminators.
- Cleanup review added a direct negative assertion that a return inside try/finally has no return edge to exit, followed by a positive assertion that the finally terminal owns that return edge.
- Union exhaustiveness tests cover both exhaustive and non-exhaustive switches with a default arm.

## Regression Evidence

- `zr_vm_place_cfg_graph_test`: 4 tests, 0 failures.
- `zr_vm_cfg_union_exhaustiveness_test`: 2 tests, 0 failures.
- `zr_vm_cfg_reachability_test`: 29 tests, 0 failures.
- `zr_vm_cfg_finally_abrupt_test`: 7 tests, 0 failures.
- `zr_vm_cfg_try_catch_edges_test`: 11 tests, 0 failures.
- `zr_vm_cfg_typed_catch_flow_test`: 8 tests, 0 failures.
- `zr_vm_cfg_typed_catch_loop_flow_test`: 6 tests, 0 failures.
- `zr_vm_dataflow_engine_test`: staged snapshot 4 tests, 0 failures; full worktree MSVC 7 tests, 0 failures because unrelated local additions remain unstaged.
- `zr_vm_parser_test`: 75 tests, 0 failures.
- `zr_vm_type_inference_test`: 118 tests, 0 failures.
- `zr_vm_semantic_facts_test`: 10 tests, 0 failures.

## Compiler Matrix

- GCC 11.4.0, WSL Debug staged snapshot: `/home/hejiahui/zr_vm-syntax-m2-staged-gcc-20260719-r10`; all 11 targets built and passed.
- Clang 14.0.0, WSL Debug staged snapshot: `/home/hejiahui/zr_vm-syntax-m2-staged-clang-20260719-r10`; all 11 targets built and passed.
- MSVC 19.44.35228 x64 Debug full worktree: `build-syntax-01-m1-msvc`; all 11 targets built and passed.
- The staged WSL snapshots start from the committed M1 tree and overlay only the known HEAD profile baseline required by existing `value.h` references; that profile overlay is not part of M2.

## Review And Boundaries

- Final staged review: GO, 0 Critical and 0 Important findings.
- Production parser CFG consumers use dynamic successor accessors; fixed-size successor reads remain only inside the compatibility implementation and older tests limited to binary cases.
- The execution optimizer and quickening graphs are separate from this parser CFG and retain their own successor models.
- M2 exposes suspension topology but does not implement async/generator lowering.
- M2 does not yet emit pre-execution load/store/init/move/copy/drop/borrow instructions; those and their block facts remain M3.

## Final Gate

- M2 acceptance inventory: passed.
- GCC/Clang staged-snapshot matrix: passed.
- MSVC worktree compatibility matrix: passed.
- `git diff --cached --check`: passed before documentation staging.
- Milestone commit: recorded in `docs/plans/syntax/01-canonical-type-place-cfg-artifact/m2-place-cfg.md`.
