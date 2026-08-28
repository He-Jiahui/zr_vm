---
related_code:
  - zr_vm_parser/include/zr_vm_parser/variance.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_generic_semantics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_compiler_variance_query_diagnostics.c
  - tests/parser/test_semantic_query_diagnostics.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_variance_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6.33: Variance Fact Producer Migration

## Scope

Task 6.15 already moved the variance rule, descriptor, and diagnostic builder
into the parser, but the LSP still enumerated every violation and appended the
resulting facts itself. This submilestone moves that remaining publication
loop behind one parser API and deletes the analyzer-local producer. It does not
change the accepted variance rule set or normal compiler first-error behavior.

## TDD And Root Cause

On fixed HEAD `397b23e3f3ff18786b9b112eefa6e7349e204e2c`, the new
parser test linked RED because
`ZrParser_Variance_PublishInterfaceDiagnostics` did not exist. The fixture has
one covariant parameter used in a method input, mutable field, and settable
property, and requires all three descriptor-backed query facts without setting
`compiler.hasError`.

The root cause was an ownership split: parser code could enumerate one
violation and build its canonical diagnostic, while
`semantic_analyzer_variance.c` still owned the loop, fact construction, and
`ZrParser_SemanticFacts_AppendDiagnostic` call. That left an LSP production
module responsible for canonical semantic fact publication.

## Implementation

`ZrParser_Variance_PublishInterfaceDiagnostics` validates its compiler,
semantic-context, and interface inputs; enumerates every structured violation;
builds descriptor `2013`; and appends a deep-copied persistent fact. It returns
failure on builder or append failure and otherwise leaves the compiler's
current error state unchanged.

The LSP interface-declaration typecheck branch now invokes only that API. The
local declaration and `semantic_analyzer_variance.c` are deleted. A source
contract rejects reintroduction of the violation iterator, diagnostic builder,
fact append, old local entry, or hard-coded `invalid_variance` in the LSP
typecheck producer path.

## Verification

The fixed overlay contains seven present code/test files and one deleted LSP
producer. Workspace-to-WSL and workspace-to-MSVC SHA-256 matched `7/7`; the
deleted file was absent from both snapshots.

GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 each returned real exit zero
for the same focused gates:

- all-violation parser producer `1/1`;
- query diagnostic disposition `11/11`;
- compiler semantic query diagnostics `64/64`;
- LSP source contracts `54` pass markers;
- dedicated stdio variance diagnostic transport.

The GCC semantic-analyzer runner was also compared byte-for-byte between the
fixed parent and overlay. Both runs retained exactly the same two unrelated
failures: closed-generic receiver metadata and the prior borrow-fact range
marker. The variance analyzer case passed in the overlay. Those two baseline
failures are not counted as GREEN and are not patched through variance or LSP
fallbacks.

## 状态与产出记录

- 完成时间：2026-08-28 17:39 +08:00。
- 状态：本子里程碑已完成；Plan 03 Task 6 继续进行。
- 完成项目：parser 全量 variance fact publisher、descriptor 2013 persistent
  query facts、compiler 首错状态隔离、LSP local producer 与内部入口删除、
  source contract、stdio 精确 payload、GCC parent/overlay marker A/B、三工具链
  `1/11/64/54/stdio` 真实退出与 `7/7 + deleted 0` byte audit。
- 后续项目：继续迁移 const-assignment 与 interface const-field 等剩余
  analyzer-owned fact publication；不得在 LSP 通过符号名、类型文本、诊断
  message 或 AST pairing 重建规则。
