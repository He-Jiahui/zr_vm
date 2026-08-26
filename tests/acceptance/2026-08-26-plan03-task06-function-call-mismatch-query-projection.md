---
related_code:
  - zr_vm_parser/include/zr_vm_parser/type_inference.h
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_diagnostics.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_type_mismatch_diagnostic_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_diagnostic_fix_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.12 Function Call Mismatch Query Projection

## Required Results

- A resolved single free-function candidate owns the structured argument type
  mismatch diagnostic.
- The fact preserves descriptor `2011`, exact argument primary range,
  parameter-type related range, and typed placeholder fix.
- LSP consumes the persistent query fact and does not directly convert or
  recreate the transient compiler diagnostic.
- Overload, generic, ownership, arity, and `ref`/`out` behavior is not widened.
- Parser, query, LSP, type-inference, compiler-integration, and stdio regressions
  pass under GCC, Clang, and MSVC on one fixed overlay.

## Evidence

The RED fixture `pick(2.5)` originally produced broad fallback diagnostics and
did not expose the parameter declaration as related information. The GREEN
parser fixture requires one persistent `type_mismatch` fact at line 3 columns
10..13, related to the parameter type at line 1 columns 16..19, with one
`<int> <expression>` user-input placeholder fix. The LSP and stdio fixtures
require the corresponding zero-based ranges and exact diagnostic cardinality.

The LSP source contract bounds the compiler-error consumer and rejects direct
calls to `Diagnostic_FromStructured` and `SemanticAnalyzer_AddDiagnostic`.
Publication failure leaves the transient compiler error intact instead of
silently replacing it with a broad LSP diagnostic.

GCC 11.4, Clang 14, and MSVC 19.44 each produced real exit zero for compiler
semantic query diagnostics `49/49`, diagnostic disposition `8/8`, LSP semantic
query diagnostics, LSP source contracts, type inference `123/123`, compiler
integration `127/127`, semantic-analyzer and union-pattern focused regressions,
and the stdio structured-diagnostic fix smoke.

## Acceptance Decision

Accepted for the single free-function call argument mismatch slice. Receiver
method-call mismatch remains outside this record because its canonical producer
path is frozen by the concurrent receiver-contract milestone. Plan 03 Task 6
therefore remains in progress.

## 状态与产出记录

- 完成时间：2026-08-26 23:51 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC 与 stdio 验收；Task 6 继续进行。
- 完成项目：精确 call argument mismatch fact、parameter related range、typed
  fix、query-only LSP projection、fail-closed consumer、source contract 与三工
  具链固定 overlay 证据。
