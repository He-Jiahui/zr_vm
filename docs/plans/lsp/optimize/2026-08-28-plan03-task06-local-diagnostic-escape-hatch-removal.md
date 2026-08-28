---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_source_contract_no_local_diagnostic_api_cases.h
  - tests/acceptance/2026-08-28-plan03-task06-local-diagnostic-escape-hatch-removal.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6.31: Local Diagnostic Escape Hatch Removal

## Scope

This submilestone removes the public semantic-analyzer API that accepted raw
severity, range, message, and code values. All production calls had already
been migrated to parser/compiler structured diagnostics, but retaining the API
allowed a future semantic rule to bypass descriptor identity, typed fixes or
explicit no-fix disposition, and the persistent semantic-query snapshot.

The lower-level `ZrLanguageServer_Diagnostic_New` allocator remains because
incremental syntax recovery, project/protocol diagnostics, and canonical
structured projection use it. It is not exposed as a semantic-analyzer rule
entry. Remaining analyzer-side structured builders are outside this removal
slice and remain explicit Task 6 migration work.

## TDD And Root Cause

Repository search found no production call to
`ZrLanguageServer_SemanticAnalyzer_AddDiagnostic`; only its public declaration,
implementation, and a test-only injection remained. A source-contract RED
required the symbol to be absent from both the public header and analyzer
implementation. It exited one with exactly two failures, one for each retained
surface.

The old `GetDiagnostics` test used the escape hatch to manufacture an
unregistered `test_error`. It now builds a parser `SZrStructuredDiagnostic`,
sets an explicit no-fix reason, projects it through
`ZrLanguageServer_Diagnostic_FromStructured`, and keeps the same diagnostics
retrieval assertion. This preserves coverage without normalizing a local
semantic producer.

## Implementation

The public declaration and implementation are deleted. A permanent source
contract rejects their reintroduction. The analyzer test uses descriptor-backed
`compiler_error`, canonical structured fields, and
`INSUFFICIENT_CONTEXT`; it never appends a raw code/message pair through a
semantic API.

## Verification

The fixed baseline is HEAD
`fced37b31268cf40d46a4a4c761d5856aa1797ce` plus five byte-identical
code/test paths. Workspace-to-WSL and workspace-to-MSVC SHA-256 audits both
reported `5/5`. GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0
(`VSCMD 17.14.38`) each passed the same six checks:

- semantic-query diagnostic runner: 16 pass markers and real exit zero;
- semantic analyzer: 70 pass markers and real exit zero;
- LSP source contracts: 55 pass markers and real exit zero;
- exact-type stdio smoke: real exit zero;
- LSP interface target: full static Debug compile/link success;
- LSP project-features target: full static Debug compile/link success.

The fixed parent `fced37b` interface runtime independently reproduced the
known canonical callable-value, lambda, and reference-call fact failures plus
one order-sensitive class-member fixture failure. The overlay reproduced the
same three canonical-call failures, so interface runtime is recorded as parent
RED and is not counted as GREEN for this slice. Full repository GREEN is not
claimed.

## 状态与产出记录

- 完成时间：2026-08-28 14:59 +08:00。
- 状态：已删除 LSP semantic analyzer 的 raw diagnostic escape hatch，并
  通过 GCC/Clang/MSVC 同一 fixed code baseline 的 6 项编译/运行门禁；Plan
  03 Task 6 继续进行。
- 完成项目：2-hit source-contract RED、public declaration 删除、production
  implementation 删除、structured projection test migration、永久 source
  contract、5-path 双快照 byte audit、三工具链 `16/70/55` marker 证据、三套
  stdio 真实退出与 interface/project 全链接证明。
- 后续项目：继续 support-first 迁移 analyzer-side structured ownership 等
  producer；修复 parent canonical call fact RED 时必须进入 parser fact/query
  层，不得在 LSP interface 增加 fallback。
