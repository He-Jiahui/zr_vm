---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_reference_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_target_identity.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_external_target_identity.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_cross_snapshot_references.c
related_module_docs:
  - docs/cli-and-tooling/lsp-cross-snapshot-external-references.md
  - docs/parser-and-semantics/semantic-query-api-foundation.md
tests:
  - tests/language_server/test_lsp_cross_snapshot_external_reference_cases.h
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
doc_type: milestone-record
---

# Plan 03 Task 7.74: External Highlight Identity

## Failure and Correction

The imported-member query gate validates the target's metadata identity, but
same-document highlights gathered candidate references by SymbolId alone.
A candidate with the same local id and mismatched or incomplete external identity
can still produce a highlight. References already compare the full external
identity after Task 7.73, so the two consumers disagree on the same snapshot.

The correction selects highlight candidates from parser
`ExternalReferences` and shares exact identity comparison with cross-snapshot
references. Range projection remains document-local, preserves read/write roles,
and deduplicates equal ranges with write precedence. Ordinary source symbols
continue using their snapshot-local canonical SymbolId.

## Evidence and Scope

- Start: 2026-09-07 22:44 +08:00.
- Completed: 2026-09-07 23:17 +08:00.
- Status: completed for the external highlight candidate identity slice.
- Base phase: `4c54b6de` on shared main.
- Regression fixture: existing generated binary/native import projects, with
  the sibling AST, symbol table and reference tracker detached before queries.
- Raw logs: `.codex/task774-*`.

MSVC RED passes the prior 18 parity cases and fails both extended binary/native
cases. Clearing the candidate metadata token still returns one highlight while
the same candidate is correctly excluded from references. The raw failure is
`invalid local external identity case=0 returned 1 highlights` in
`.codex/task774-msvc-red.log` (exit 1).

The final regression extends the prior eight identity mutations with missing
external marking and invalid SymbolId. It also checks exact read/write highlight
ranges, duplicate suppression, detached legacy analyzer state, and missing/stale
target rejection. Source contracts require both consumers to use the shared
identity comparison and canonical external-reference query.

GCC and MSVC GREEN pass parity `20/20` with exit 0. Clang ASan/UBSan passes
the same 20 functional cases, then exits 1 for the Task 7.73 baseline leak of
5069 bytes in 41 allocations. All three local hover runners pass `12/12` with
exit 0. Local semantic query retains the same two failures recorded in Task 7.72:
Resolved Function Value Dependency Invalidates Directly, and Completion Fallback
Reuses Scoped Query Analyzer Cache. Clang also retains its recorded 464-byte,
six-allocation leak in that runner. Source contracts on all three toolchains
retain only the unrelated stdio `cJSON_CreateString("declaration")` assertion.
Inspection confirms the legend now uses `cJSON_CreateStringArray` and still
contains the declaration modifier; that legacy construction-specific assertion
is outside this identity correction. All new reference/highlight contracts pass.

Validation uses the existing GCC Debug, Clang ASan/UBSan and MSVC Debug static
builds. The four targets are `zr_vm_language_server_semantic_query_parity_test`,
`zr_vm_language_server_local_semantic_hover_test`,
`zr_vm_language_server_local_semantic_query_test` and
`zr_vm_language_server_lsp_source_contracts_test`. Builds use `cmake --build`
with `-j 6`; binaries run sequentially to avoid concurrent generated-fixture
writes. No native/Web or full-plan acceptance is claimed by this slice.

Module-entry and receiver type-member legacy adapters, provider-generation
publication, external rename and complete Task 3/7/8 acceptance remain pending.
This change is confined to the reference/highlight consumer group.
