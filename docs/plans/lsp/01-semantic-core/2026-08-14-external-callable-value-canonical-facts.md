---
related_code:
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_canonical_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_canonical_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
plan_sources:
  - user: 2026-08-13 canonical external callable-value fact contract
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_project_native_callable_signature_cases.h
  - tests/parser/test_canonical_consumers.c
  - tests/acceptance/2026-08-14-lsp-l8-external-callable-value-canonical-facts.md
doc_type: milestone-detail
---

# External Callable-Value Canonical Facts

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
|---|---|---|---|
| 2026-08-22 18:23 +08:00 | 已完成 | LSP 08 第十六个独立合同：binary metadata 与 descriptor-provider 函数成员赋给 local callable value 后发布 canonical function `TypeId` 和 signature display；external reference 明确保留 unresolved identity、invalid `SymbolId` 与空 declaration range。hover/signature 只消费 `SemanticQuery_CallAt/FormatCall`，清除 call payload 后直接 unavailable。 | GCC/Clang/MSVC canonical consumers `19/19`、semantic facts `14/14`、semantic query `29/29`、local hover、interface、project 和 stdio/CLI 均真实完成且 Unity `Fail -` 为零。 |

## Contract

Binary and descriptor-provider function members assigned to local values publish parser-owned canonical callable facts. They retain a canonical function `TypeId` and exact signature display while remaining explicitly unresolved: invalid `SymbolId`, empty declaration range, and `hasResolvedTarget == false`.

LSP hover and signature help consume only `SemanticQuery_CallAt/FormatCall`. If the fact-owned call payload is removed, the exact call-target guard prevents legacy callable lookup from rebuilding a result. No member-name, descriptor-name, initializer-AST, inferred-type, or fabricated binary/source declaration fallback is accepted.

## Evidence

- Support-first RED: binary and provider callable-value calls returned no `CallAt` expression/reference.
- Parser support publishes external member callable contracts and local aliases.
- Duplicate expression facts were fixed separately in `27468de` before consumer work continued.
- GCC 11.4 Debug shared, Clang 14 Debug shared, and MSVC 19.44 Debug static each completed canonical consumers `19/19`, semantic facts `14/14`, semantic query `29/29`, local hover, interface, and complete project runner with zero Unity `Fail -` markers.
- Each toolchain completed the stdio/CLI smoke. The peak working sets were GCC `33.08 MiB`, Clang `32.32 MiB`, and MSVC `39.13 MiB`, below the `512 MiB` limit.
- The receiver support gate now rejects AST re-inference for a missing exact construct receiver fact. A binary property may continue only when the exact `PropertyAt` identity is already published; it does not recover a property from text or accessor spelling.

## Boundary

This is the sixteenth independent L8 contract. It does not mark L8 complete. Source and lambda callable values remain governed by their earlier resolved-identity records; external callable values deliberately do not fabricate declaration identity.
