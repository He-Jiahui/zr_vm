---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_callable_return_inference.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_semantic_query.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_diagnostic_golden_parity_cases.h
  - tests/language_server/test_lsp_source_contract_return_type_cases.h
  - tests/language_server/stdio_return_type_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-27-plan03-task06-return-type-query-projection.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.23 Return Type Query Projection

## Required Results

- Diagnose incompatible exact returns in parser/compiler, not LSP.
- Preserve stable descriptor `2018`, callable-name range, both return ranges,
  canonical text, two related-information entries, and user-decision no-fix.
- Keep weak or unavailable early return metadata legal and separate from an
  exact return conflict.
- Make public exact return inference fail closed without a name, AST-text, or
  display-text fallback in LSP.
- Prove compiler-query/LSP parity from one semantic snapshot and preserve the
  same fields over stdio.

## TDD Evidence

The exact-conflict parser RED reported `60 Tests / 1 Failure`; exact `int` and
`string` returns were silently merged to weak object. LSP golden parity and
source-contract REDs then showed that the analyzer still owned a second return
walk, common-type merge, and direct diagnostic producer.

The first shared implementation caused nine compiler-integration failures by
diagnosing single weak results during early metadata construction. A dedicated
support RED reported `61 Tests / 1 Failure`. The corrected implementation only
diagnoses incompatible exact pairs, keeps internal weak metadata, and exposes
unavailable from the public exact query.

## Final Evidence

On fixed HEAD `d6ee3fed1502699113d246f5efad0c81de4f5cb9` plus the
thirteen-path code/test overlay, GCC 11.4, Clang 14.0.0, and MSVC
19.44.35228.0 each passed the same eleven direct checks. Shared Unity totals
were `61/61`, `11/11`, `15/15`, `30/30`, `124/124`, and `127/127`; LSP suites
reported 15 diagnostic, 57 analyzer, 48 source-contract, and 12 union-pattern
pass markers. Dedicated stdio exited zero on every toolchain.

All thirteen code/test paths matched SHA-256 across the workspace and all
three source snapshots. The MSVC environment was `VSCMD_VER=17.14.38`, and its
twelve build/run logs contained zero `:FAIL` or `Fail -` markers.

## Acceptance Decision

Accepted as the return-type slice of Plan 03 Task 6. Parser/compiler is the
only producer for exact unannotated return conflicts, and LSP is a canonical
query projection. Task 6 remains in progress for the remaining analyzer-owned
rules.

## 状态与产出记录

- 完成时间：2026-08-27 16:51 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC 同基线 11 项验收；Task 6
  继续进行。
- 完成项目：两阶段 RED、exact/weak 边界、descriptor/range/text/related/no-fix
  contract、公共 exact query、analyzer producer 删除、golden parity、source
  contract、stdio transport、模块拆分、SHA-256 与真实退出证据。
