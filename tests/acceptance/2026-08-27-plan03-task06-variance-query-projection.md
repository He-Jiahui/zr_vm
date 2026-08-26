---
related_code:
  - zr_vm_parser/include/zr_vm_parser/variance.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_generic_semantics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_variance.c
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_semantic_query.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_variance_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-27-plan03-task06-variance-query-projection.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.15 Variance Query Projection

## Required Results

- Parser owns interface variance traversal and structured diagnostic creation.
- Compiler and LSP share descriptor `2013`, message, severity, exact primary and
  related declaration ranges, help URI, and no-fix disposition.
- All existing interface variance contexts remain covered, including nested
  generic composition.
- LSP does not retain recursive variance policy, perform symbol-table lookup,
  infer by name, or directly construct the diagnostic.
- Focused parser/query/LSP tests and dedicated stdio transport pass under GCC,
  Clang, and MSVC from one byte-exact code/test overlay.

## Evidence

The initial REDs independently demonstrated the missing compiler structured
fact, missing registry descriptor, incomplete LSP payload, and duplicate LSP
policy. The shared parser query and builder resolved those gaps without changing
the accepted variance rule set.

GCC 11.4, Clang 14, and MSVC 19.44 each returned real exit zero for the same ten
focused targets on HEAD `67f45cc` plus the 10-path overlay. Their Unity totals
include compiler diagnostics `52/52`, query disposition `8/8`, semantic facts
`14/14`, semantic query `30/30`, type inference `123/123`, and compiler
integration `127/127`; all four LSP suites also passed. SHA-256 comparison showed
the working tree, GCC/Clang source snapshot, and MSVC source snapshot were
`10/10` byte-identical for the changed code/test paths.

The dedicated stdio fixture requires exactly one `invalid_variance` diagnostic
at zero-based line 1 characters 21..22, related declaration at line 0 characters
23..24, descriptor `2013`, canonical full message, registered help URI,
`requires_user_decision`, and no fixes. All three toolchain servers passed it.

## Acceptance Decision

Accepted for variance query projection. Other semantic diagnostic families
remain to migrate, so Plan 03 Task 6 remains in progress.

## 状态与产出记录

- 完成时间：2026-08-27 03:09 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC focused、compiler integration
  与独立 stdio 验收；Task 6 继续进行。
- 完成项目：canonical variance traversal、structured descriptor 2013、
  exact primary/related ranges、no-fix disposition、compiler/LSP parity、
  duplicate LSP semantic policy 删除与三工具链 transport 证据。
