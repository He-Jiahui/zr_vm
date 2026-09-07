---
related_code:
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_optional_capabilities_smoke.js
  - tests/language_server/stdio_capability_snapshot.js
related_module_docs:
  - docs/cli-and-tooling/lsp-optional-capability-negotiation.md
tests:
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_optional_capabilities_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/00-baseline-and-contract.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
doc_type: milestone-record
---

# Plan 03 Task 7.75: Legend Contract Test Boundary

## Failure and Correction

Task 7.74's three source-contract runs each failed the literal source assertion
`cJSON_CreateString("declaration")`. The production legend now uses
`cJSON_CreateStringArray` with a declaration modifier. The old assertion tests a
construction detail unrelated to its enclosing canonical-symbol query test.

Remove that assertion and its unused stdio source-file read. Retain the canonical
`DeclaredSymbols`, `SymbolAt` and `CanonicalTypeAt` requirements and the forbidden
AST/name reconstruction checks. The existing optional-capabilities protocol test
owns the exact legend contract: it parses initialize responses and compares the
complete capability object, including `tokenModifiers: ['declaration']`, for 21
negotiation inputs. Four further cases exercise request behavior, for 25 total.

## Validation

- Start: 2026-09-07 23:20 +08:00.
- Completed: 2026-09-07 23:26 +08:00.
- Status: completed.
- RED: the Task 7.74 source-contract logs each contain exactly the stale literal
  assertion and exit 1.
- GREEN: GCC, Clang ASan/UBSan and MSVC complete source contracts exit 0;
  all three real stdio optional-capabilities smokes pass `25/25`, including
  all 21 exact initialize capability snapshots. Clang runs with
  `detect_leaks=1:halt_on_error=1` and `UBSAN_OPTIONS=halt_on_error=1` and
  reports no sanitizer errors in this slice.
- Build targets: `zr_vm_language_server_lsp_source_contracts_test` and
  `zr_vm_language_server_stdio`, using the Task 7.74 toolchain build directories.
- Raw logs: `.codex/task775-*`.

This testing correction does not close the remaining canonical consumer,
provider-generation, runtime leak or native/Web acceptance gates.
