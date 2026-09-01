---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_diagnostic_projection.c
tests:
  - tests/parser/test_semantic_query_diagnostics.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_semantic_query_diagnostics.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_ownership_diagnostics.c
  - tests/language_server/test_lsp_source_contracts.c
plan_sources:
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6: Structured Diagnostic Closure

## Scope

This closure milestone audits the Task 6 producer migrations and freezes the
remaining architectural boundary. Parser/compiler semantic facts own rule
evaluation, stable descriptor identity, primary and related ranges, and typed
fix or explicit no-fix disposition. The language server materializes the
snapshot query and projects protocol fields without rebuilding semantic rules.

The lower-level `ZrLanguageServer_Diagnostic_New` allocator remains available
for syntax recovery, project/protocol diagnostics, and the single structured
semantic projector. It is not a semantic rule API.

## Contract

The permanent source contract enumerates every current semantic-analyzer rule
source. These files may call parser/compiler publishers, but may not call the
raw LSP diagnostic constructor, the parser diagnostic builder, or append a
semantic diagnostic fact directly. `semantic_analyzer_query_diagnostics.c`
must consume `MaterializeDiagnostics` and `Diagnostics`, then call
`Diagnostic_FromStructured`; only
`semantic_analyzer_diagnostic_projection.c` may create the protocol diagnostic
and copy related information, fixes, and no-fix disposition.

Existing compiler/LSP golden tests compare severity, code, message, cause,
suggestion, descriptor, code-description URI, primary range, related ranges,
fixes, and no-fix reason from one semantic snapshot. They cover initializer,
assignment, return, call compatibility, duplicate declaration, exact-type,
const, reachability, unresolved-reference, variance, and ownership paths.

## Verification

GCC 11.4, Clang 14, and MSVC 19.44 (`VSCMD 17.14.38`) used isolated Debug/static
snapshots containing Task 5 plus the four Task 6 closure paths. Each toolchain
passed with real process exits:

- compiler semantic-query diagnostics: `64/64`;
- parser semantic-query diagnostic disposition: `13/13`;
- LSP semantic-query diagnostics: `19/19`;
- LSP source contracts, including the new directory-level rule contract;
- source/binary/native semantic-query parity.

The full semantic-analyzer runner was also executed on GCC, Clang, and MSVC.
All compiler-to-LSP golden parity cases passed, while the runner retained its
existing type-display, reference, reachability, and ownership-fact failures.
The ownership runner likewise retained existing fact-generation failures.
Those suites are not reported as green and no message or marker allowlist was
changed. Task 8 retains ownership of the complete 16-target and stdio/CLI final
gate.

## 状态与产出记录

- 完成时间：2026-09-01 21:26 +08:00。
- 状态：已完成 Plan 03 Task 6 结构化语义诊断关闭审计；三工具链 focused
  closure matrix 与 semantic-query parity 真实 exit 0。完整 analyzer/ownership
  历史红项未计入通过证据。
- 完成项目：Task 6.1-6.41 producer/query migration audit；10-source semantic
  analyzer rule contract；structured query/projector ownership contract；三工具链
  `64/13/19` 与 source-contract/parity 关闭矩阵；golden parity 复核。
- 后续项目：继续 Task 7 consumer migration；support-first 修复完整 analyzer/
  ownership 历史 fact-generation 红项；Task 8 负责完整 16-target 与 stdio/CLI。
