---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_messages.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_semantic_query.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_exact_type_diagnostic_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_source_contract_exact_type_diagnostic_cases.h
  - tests/language_server/stdio_exact_type_diagnostic_smoke.js
  - tests/acceptance/2026-08-28-plan03-task06-cannot-infer-exact-type-query-projection.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6.30: Cannot Infer Exact Type Query Projection

## Scope

This submilestone makes parser/compiler the only semantic producer for the
generic exact-type inference failure. It freezes stable diagnostic identity,
canonical text, exact producer-selected ranges, explicit no-fix disposition,
and one persistent semantic-query fact. LSP may request that fact and project
it, but it may not own the code, rebuild policy from AST or text, or remove a
same-range local diagnostic after a more specific compiler diagnostic appears.

This is the generic fail-closed inference boundary. Existing specific
structured compiler diagnostics retain precedence; the bridge does not replace
type mismatch, ownership, decorator, or other canonical failures with the
generic exact-type diagnostic.

## TDD And Root Cause

The parser RED first failed to link because no public canonical reporter
existed. LSP REDs then proved that analyzer symbol and type-check passes still
created `cannot_infer_exact_type` directly, while the compiler-query bridge
deleted a same-range local inference diagnostic after publishing another fact.
That split ownership made descriptor metadata, cause, suggestion, no-fix
policy, and ranges unavailable from one canonical snapshot.

A golden analyzer RED used an untyped function parameter and required the
parameter name range, parser descriptor `2020`, complete query/LSP field
parity, no fixes, `REQUIRES_USER_DECISION`, and no parallel `compiler_error`.
The first stdio RED exposed a point range at the parameter close; production
now reports the exact AST `nameLocation`. A source-contract RED required the
parser reporter call and rejected the local producer, code literal, and
shadow-removal workaround.

## Implementation

`ZrParser_Compiler_ReportCannotInferExactType` builds the structured parser
diagnostic. The registry publishes descriptor `2020`, code
`cannot_infer_exact_type`, error severity, and type category. The fact carries
message `cannot infer exact type`, a canonical cause and suggestion, the exact
caller-supplied range, zero fixes, and `REQUIRES_USER_DECISION`.

`ZrLanguageServer_SemanticAnalyzer_ReportCannotInferExactType` is a narrow
consumer bridge. It preserves and publishes an existing structured compiler
error when one is present; otherwise it invokes the parser reporter and
consumes the persistent query fact. Analyzer symbol, field, foreach, variable,
return, and type-check paths now use that bridge. The two local diagnostic
helpers, code/message literals, and same-range shadow deletion are removed.

## Verification

The fixed code baseline is HEAD
`d3ca0bdc40af3655517ab04d4f2fdabe23ea1671` plus sixteen byte-identical
code/test paths. Workspace-to-WSL and workspace-to-MSVC SHA-256 audits both
reported `16/16`. GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0
(`VSCMD 17.14.38`) each passed the same fifteen checks with real process exits:

- extern parameter, FFI wrapper, extern enum, and extern struct decorator
  diagnostics: `7/7`, `9/9`, `5/5`, and `5/5`;
- compiler semantic-query diagnostics: `64/64`;
- semantic facts and semantic query: `15/15` and `30/30`;
- parser and type inference: `74/74` and `124/124`;
- compiler integration: `127/127`;
- native extern contract: GCC/Clang `30/30`, MSVC `30 tests / 0 failures /
  1 Unix-only ignore`;
- LSP semantic-query diagnostics: 16 pass markers;
- semantic analyzer: 70 pass markers;
- LSP source contracts: 54 pass markers;
- dedicated exact-type stdio smoke: real exit zero.

GCC and Clang used one fixed ext4 snapshot. The MSVC tree was copied from that
snapshot and independently hash-audited before its static Debug build. Tests
ran serially and no accepted runner masked a process exit. Full repository
GREEN is not claimed by this focused Task 6 slice.

## 状态与产出记录

- 完成时间：2026-08-28 07:01 +08:00。
- 状态：已完成 `cannot_infer_exact_type` 的 parser/compiler 单一生产、
  persistent semantic query 与 LSP/stdIO 投影，并通过 GCC/Clang/MSVC 同一
  fixed code baseline 的 15 项验收；Plan 03 Task 6 继续进行。
- 完成项目：public canonical reporter、descriptor `2020`、type category、
  exact caller/name range、canonical message/cause/suggestion、user-decision
  no-fix、specific compiler diagnostic precedence、persistent query fact、
  analyzer symbols/typecheck/field/foreach consumer migration、2 个本地 producer
  删除、shadow-removal workaround 删除、query/LSP golden parity、source
  contract、独立 stdio smoke、16-path 双快照 byte audit 与三工具链真实退出证据。
- 后续项目：继续 support-first 迁移剩余 analyzer-owned semantic rules；LSP
  不得按 code/message、AST/source text、range coincidence 或本地规则重建语义。
