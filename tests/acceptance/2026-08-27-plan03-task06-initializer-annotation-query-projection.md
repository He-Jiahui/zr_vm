---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_semantic_query.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_diagnostic_golden_parity_cases.h
  - tests/language_server/test_lsp_source_contract_initializer_annotation_cases.h
  - tests/language_server/stdio_initializer_annotation_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-27-plan03-task06-initializer-annotation-query-projection.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.22 Initializer Annotation Query Projection

## Required Results

- Reject a variable declaration only when both explicit type and initializer
  are absent.
- Produce stable descriptor `2017`, exact declaration-pattern range, canonical
  text fields, and explicit user-decision no-fix disposition in parser/compiler.
- Project the same persistent query fact through analyzer and stdio without a
  parallel analyzer-owned producer or generic `compiler_error`.
- Keep initializer exact-type inference failure under
  `cannot_infer_exact_type` rather than broadening this rule.
- Prove compiler-query/LSP field parity from one semantic snapshot and freeze
  the ownership boundary with a source-contract test.

## TDD Evidence

The deliberate parser RED reported `58 Tests / 1 Failure`: `var missing;` did
not set compiler error state. After the parser validator and LSP query consumer
were connected, focused parser, analyzer, source-contract, and stdio checks
passed. The expanded semantic-query run then failed its registry count and
message-table coverage checks until descriptor `2017` and its two catalog rows
were included in the public support contract.

## Final Evidence

On fixed HEAD `fb737be114a70f7b5fc9703e3163100a2d2414fe` plus the 14-path
code/test overlay, GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 each
passed the same eleven direct checks. Shared Unity totals were `58/58`,
`11/11`, `15/15`, `30/30`, `124/124`, and `127/127`; LSP suites reported
15 diagnostic, 56 analyzer, 47 fixed-snapshot source-contract, and 12
union-pattern pass markers. Dedicated stdio exited zero with no output on every
toolchain, and all run logs contained zero `Fail -` markers.

All fourteen code/test paths matched SHA-256 across the workspace and the GCC,
Clang, and MSVC source snapshots before execution. The MSVC environment was
`VSCMD_VER=17.14.38`.

## Acceptance Decision

Accepted as the initializer-annotation slice of Plan 03 Task 6. Parser/compiler
is the only producer for an untyped declaration without an initializer, and
LSP is a semantic-query projection. Task 6 remains in progress for the
remaining analyzer-owned rules.

## 状态与产出记录

- 完成时间：2026-08-27 14:41 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC 同基线 11 项验收；Task 6
  继续进行。
- 完成项目：parser RED、descriptor/range/text/no-fix contract、registry 与
  message completeness、analyzer producer 删除、query/LSP golden parity、
  source contract、stdio transport、SHA-256 与真实退出证据。
