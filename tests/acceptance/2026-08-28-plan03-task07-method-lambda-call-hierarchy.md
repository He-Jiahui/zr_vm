---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_call_hierarchy.c
tests:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_semantic_call_hierarchy_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_type_hierarchy_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-28-plan03-task07-method-lambda-call-hierarchy.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 7.9 Method And Lambda Call Hierarchy

## Required Results

- Same-name methods remain distinct by parser SymbolId and resolved call edge.
- A parser-owned lambda caller appears in incoming hierarchy without requiring
  an LSP symbol-table entry.
- A returned lambda hierarchy item can be used for a later outgoing request.
- Name mutation cannot select an endpoint; range or version mutation fails
  closed.
- No source-text, callable-variable-name, or AST-pair fallback is introduced.

## TDD Evidence

The method fixture passed before production changes, proving existing call
facts already covered receiver methods. The lambda fixture failed at incoming
projection while the parser lower-layer lambda call-edge tests passed. This
isolated the defect to the LSP consumer's requirement for an LSP display
symbol, not to call-edge production.

## Final Evidence

GCC, Clang, and MSVC pass parser call queries `11/11`, semantic-query parity
`9/9`, source contracts `60/60`, advanced editor features `73/73`, and the
combined type/call-hierarchy stdio smoke with real exits. Full interface keeps
exactly `109 Pass / 4` unchanged markers on all three toolchains and is not
GREEN. Workspace, WSL, and MSVC code/test bytes match `5/5`, ten comparisons,
zero mismatches.

## Acceptance Decision

Accepted for source-local receiver methods and variable-bound lambdas. External
cross-project, binary, and native call hierarchy remains open.

## 状态与产出记录

- 完成时间：2026-08-28 23:58 +08:00。
- 状态：本子项已验收；Task 7 与 Plan 03 继续进行。
- 完成项目：method/lambda canonical edge projection、stable protocol identity、
  lambda follow-up re-resolution、name/range/version fail-closed、三工具链
  focused/stdio exits、interface marker audit、byte audit。
