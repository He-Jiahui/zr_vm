---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_top_level_duplicate.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_duplicate_definition_diagnostic_cases.h
  - tests/language_server/test_semantic_analyzer_diagnostic_golden_parity_cases.h
  - tests/language_server/test_lsp_source_contract_duplicate_diagnostic_cases.h
  - tests/language_server/stdio_duplicate_type_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-27-plan03-task06-duplicate-type-query-projection.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.21 Duplicate Type Query Projection

## Required Results

- Produce duplicate-type semantics in parser/compiler diagnostics.
- Preserve stable code, severity, descriptor, exact primary range, prior
  declaration relation, text fields, and explicit no-fix reason.
- Project the same persistent query fact through analyzer, native LSP, and
  stdio without a second analyzer-owned diagnostic.
- Delete the legacy LSP producer and prevent its policy from returning through
  source-contract tests.
- Prove compiler-query/LSP field parity from one semantic snapshot.

## Evidence

The deliberate parser RED was `57 Tests / 1 Failure`: duplicate source classes
set a compiler error but published no `duplicate_type` query diagnostic. A
subsequent LSP run identified the non-source bootstrap symbol as the first
related candidate, producing a zero range until parser registration preferred
exact source declaration AST identity.

The final parser regression requires one descriptor-2010 error, exact one-based
class-name ranges, one prior-declaration relation, no fixes, and
`REQUIRES_USER_DECISION`. The LSP regression requires one zero-based projected
diagnostic, no parallel `compiler_error`, exact current and prior name ranges,
and the same URI. Golden parity compares all structured fields, related rows,
fix rows, help metadata, and no-fix disposition. The stdio smoke repeats the
same contract at JSON protocol level.

On HEAD `96f6b731c0572c1c91a5defaea8c5876ce0afbb7` plus the fixed overlay,
GCC, Clang, and MSVC each passed all eleven direct checks. Shared Unity totals
were `57/57`, `11/11`, `15/15`, `30/30`, `123/123`, and `127/127`; LSP suites
reported 15 diagnostic, 55 analyzer, 47 source-contract, and 12 union-pattern
pass markers. Dedicated stdio exited zero on every toolchain. Marker audit
found only the same nine compiler-integration embedded negative fixtures, with
no outer Unity failure.

All thirteen modified/added code-test paths matched SHA-256 across the
workspace and three snapshots. The two removed LSP producer paths were absent
from every snapshot.

## Acceptance Decision

Accepted as the duplicate-type slice of Plan 03 Task 6. Parser/compiler is the
only semantic producer and LSP is a query projection. Task 6 remains in
progress for the remaining analyzer-owned rules.

## 状态与产出记录

- 完成时间：2026-08-27 12:40 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC 同基线 11 项验收；Task 6
  继续进行。
- 完成项目：parser RED、descriptor/range/related/no-fix contract、legacy
  producer 删除、query/LSP golden parity、source contract、stdio transport、
  SHA-256 与真实退出证据。
