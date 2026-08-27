---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_semantic_query.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_exact_type_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-28-plan03-task06-cannot-infer-exact-type-query-projection.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.30 Cannot Infer Exact Type Query Projection

## Required Results

- Produce generic exact-type inference diagnostics in parser/compiler, not LSP.
- Preserve descriptor `2020`, exact producer-selected range, canonical text,
  type category, and explicit user-decision no-fix disposition.
- Preserve a more specific structured compiler diagnostic instead of replacing
  it with the generic inference failure.
- Preserve query/LSP golden parity and publish no parallel `compiler_error`.
- Remove analyzer-local code/message policy and same-range shadow deletion.

## TDD Evidence

The initial parser target failed to link because the public reporter was
absent. Analyzer and source-contract REDs then exposed both local producers and
the query bridge's range-based shadow deletion. The golden test required one
canonical parameter-name fact and one exact LSP projection. The first stdio
run reported a close-position point range, which isolated the remaining
producer location defect; the corrected bridge receives the AST parameter
`nameLocation` and the wire range is the full identifier.

## Final Evidence

On fixed HEAD `d3ca0bdc40af3655517ab04d4f2fdabe23ea1671` plus the
sixteen-path code/test overlay, GCC 11.4, Clang 14.0.0, and MSVC
19.44.35228.0 passed the same fifteen direct checks. Shared Unity results were
`7/7`, `9/9`, `5/5`, `5/5`, `64/64`, `15/15`, `30/30`, `74/74`,
`124/124`, `127/127`, and native extern `30/30` except one MSVC Unix-only
ignore. LSP suites reported 16 query, 70 analyzer, and 54 source-contract pass
markers; the dedicated stdio smoke exited zero on every toolchain.

All sixteen code/test paths matched from the workspace to the fixed WSL and
MSVC snapshots. Tests were serial, no accepted summary contained a real
failure, and every runner preserved its process exit.

## Acceptance Decision

Accepted as the generic exact-type inference diagnostic slice of Plan 03 Task
6. Parser/compiler is the only semantic producer and LSP is a persistent
compiler-query projection. Plan 03 Task 6 remains open for later semantic
diagnostic slices.

## 状态与产出记录

- 完成时间：2026-08-28 07:01 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC 同基线 15 项验收；Task 6
  继续进行。
- 完成项目：parser link RED、registry/query tests、analyzer golden RED/GREEN、
  stdio range RED/GREEN、public parser reporter、descriptor `2020`、persistent
  diagnostic fact、specific-error precedence、LSP duplicate producer 与 shadow
  workaround 删除、query parity、source contract、独立 stdio、16-path 双快照
  byte audit 与真实退出证据。
