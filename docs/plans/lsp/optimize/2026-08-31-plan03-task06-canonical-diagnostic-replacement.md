---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
tests:
  - tests/language_server/test_lsp_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_semantic_query_diagnostic_replacement_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6.36: Canonical Diagnostic Replacement

## Scope

The semantic-query bridge previously treated an analyzer diagnostic with the
same range and code as authoritative. It retained that local diagnostic's
severity, message, descriptor, help URI, fixes, and no-fix disposition while
only adding missing related information from the parser query. This allowed a
stale second semantic implementation to survive canonical projection.

This submilestone makes the parser-owned structured diagnostic authoritative
for an exact duplicate. It does not add a message, member-name, token, or AST
fallback, and it does not change diagnostics with a different range or stable
code.

## TDD And Implementation

The GCC RED fixture publishes a structured `numeric_overflow` fact with an
error severity, canonical message/cause/suggestion, registry descriptor/help
URI, one exact related range, and
`REQUIRES_USER_DECISION`. It then preloads a stale analyzer warning at the same
range/code. Before the fix, the output pointer and all stale local fields were
retained.

`ZrLanguageServer_SemanticAnalyzer_AppendSemanticQueryDiagnostics` now locates
the duplicate array slot, materializes the complete object through
`ZrLanguageServer_Diagnostic_FromStructured`, frees the stale object, and
replaces that slot. Conversion failure leaves the existing object intact; it
does not append a partial projection. New ranges/codes still append normally.

The regression case is isolated in a cohesive case header so the existing
language-server test runner does not absorb another implementation block. It
asserts exact code, severity, primary range, message, cause, suggestion,
descriptor/help URI, related range/message, no-fix reason, and absence of
invented fixes.

## Verification

The fixed source snapshot was built independently with GCC 11.4 and Clang
14.0.0. Both toolchains returned real exit zero for:

- parser semantic-query diagnostics: `11/11`;
- compiler semantic-query diagnostics: `64/64`;
- LSP semantic-query diagnostics, including the replacement case;
- semantic-query parity and LSP source-contract targets.

The LSP interface parent retained exactly the same eight pre-existing producer
markers on both toolchains, so the marker delta is zero. The interface parent
itself still exits one and is not counted as green. MSVC, the complete
16-target matrix, and the three stdio/CLI smoke suites were not run for this
slice.

## 状态与产出记录

- 完成时间：2026-08-31 06:42 +08:00。
- 状态：Task 6.36 canonical diagnostic replacement 子里程碑已完成；Plan 03
  Task 6 继续进行。
- 完成项目：同range/stable-code duplicate slot识别、完整structured projection
  replacement、stale diagnostic释放、全字段LSP回归、GCC/Clang diagnostics/
  parity/source-contract门禁、interface fixed-marker delta 0。
- 后续项目：继续迁移剩余analyzer-owned semantic diagnostics并删除其producer；
  Syntax05占用的property/import paths释放前不跨边界修改，不按message/name补偿。
