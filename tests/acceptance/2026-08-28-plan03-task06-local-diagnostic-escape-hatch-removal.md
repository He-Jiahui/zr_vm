---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_source_contract_no_local_diagnostic_api_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-28-plan03-task06-local-diagnostic-escape-hatch-removal.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.31 Local Diagnostic Escape Hatch Removal

## Required Results

- Remove the public semantic-analyzer raw diagnostic insertion API.
- Preserve analyzer diagnostic retrieval coverage through a parser structured
  diagnostic and canonical LSP projection.
- Prevent source-level reintroduction of the declaration or implementation.
- Prove all primary LSP consumers still compile and link.
- Do not claim unrelated parent interface failures as GREEN.

## TDD Evidence

The source-contract RED exited one with exactly two matches for
`ZrLanguageServer_SemanticAnalyzer_AddDiagnostic`: one in the public header and
one in the implementation. Repository search confirmed there were no remaining
production callers. After removal, the old analyzer test was migrated from an
unregistered raw `test_error` to a descriptor-backed parser structured
diagnostic with explicit no-fix disposition.

## Final Evidence

On fixed HEAD `fced37b31268cf40d46a4a4c761d5856aa1797ce` plus the
five-path overlay, GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 each passed
the same six checks. Direct runners reported 16 semantic-query diagnostic, 70
analyzer, and 55 source-contract pass markers; exact-type stdio exited zero.
The interface and project-feature targets compiled and linked in all three
static Debug builds. Workspace-to-WSL and workspace-to-MSVC hashes matched
`5/5`.

The clean parent interface suite was also run to classify its failures. Its
canonical callable-value, lambda, and reference-call failures predate this
overlay; they remain RED and are not accepted as passing evidence here.

## Acceptance Decision

Accepted as the raw LSP semantic diagnostic escape-hatch removal slice of Plan
03 Task 6. Structured analyzer-side producers still require individual
parser/compiler fact migration before Task 6 can close.

## 状态与产出记录

- 完成时间：2026-08-28 14:59 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC 同基线 6 项门禁；Task 6
  继续进行。
- 完成项目：2-hit RED、public/API implementation 删除、structured analyzer
  test、source contract、5-path 双快照 byte audit、三工具链 query/analyzer/
  source/stdIO 真实退出与 interface/project 全链接证明、parent RED 分类。
