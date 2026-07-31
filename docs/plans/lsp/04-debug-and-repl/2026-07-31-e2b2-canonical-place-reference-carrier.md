---
plan: docs/plans/lsp/04-debug-and-repl.md
stage: E2b2 canonical Place reference carrier
status: completed
related_code:
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c
tests:
  - tests/parser/test_expression_fragment_parser.c
---

# E2b2 Canonical Place Reference Carrier

## Contract

`ZrParser_TypeEnvironment_RegisterCanonicalVariableWithPlace` receives a canonical
`PlaceId` with its existing `SymbolId`, `TypeId`, and declaration range. The
binding retains that opaque identity and identifier inference projects it into
both `ZR_SEMANTIC_REFERENCE_READ` and `ZR_SEMANTIC_REFERENCE_WRITE` facts.
`PlaceId=0` remains the explicit unavailable value; no name-based Place lookup
is introduced. The existing `RegisterCanonicalVariable` entry remains source
compatible and delegates with an unavailable PlaceId.

Debug frame and typed-local consumers can pass their generation-validated
PlaceId into the same API. Their runtime resolution remains E3 work and is
outside this carrier milestone.

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 验证 |
| --- | --- | --- | --- |
| 2026-08-01 02:37 +08:00 | completed | Canonical external bindings retain PlaceId; resolved identifier read/write facts project it; ordinary bindings clear stale Place identity; the legacy no-Place registration API remains source compatible; module contract and parser regressions updated. | Isolated `3d67352 + E2b2 overlay` snapshots: GCC 11.4, Clang 14, and MSVC 19.44 each ran the initial parser regression 4/4 and the E3 Debug overlay 50/50 with runner exit 0. The deterministic ordinary-binding RED was 5 tests / 1 failure (`expected 0, was 8003`). Final clean-HEAD MSVC replay ran parser 5/5 and HEAD Debug 37/37 with `test_exit=0`; GCC and Clang each compiled the exact parser implementation and clean-HEAD Debug caller with exit 0. `75b9aa0` only adds an unrelated AOT design document. |

The snapshot was necessary because the shared dirty worktree's
`zr_vm_cli/CMakeLists.txt` mixes keyword and plain `target_link_libraries`
signatures, preventing CMake regeneration before any E2b2 source is compiled.
