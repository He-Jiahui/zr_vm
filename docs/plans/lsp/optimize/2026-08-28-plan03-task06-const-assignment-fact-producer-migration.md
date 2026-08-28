---
related_code:
  - zr_vm_parser/include/zr_vm_parser/const_assignment.h
  - zr_vm_parser/src/zr_vm_parser/semantic/const_assignment.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_compiler_const_assignment_query_producer.c
  - tests/parser/test_semantic_query_diagnostics.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_const_assignment_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6.34: Const Assignment Fact Producer Migration

## Scope

Task 6.14 placed const-assignment legality and diagnostic construction in the
parser, but LSP still extracted the target range, resolved a SymbolId, scanned
its own SymbolTable for the declaration AST, and appended the fact. This slice
makes parser/compiler the complete persistent-fact producer and deletes that
LSP projection module. The established local, parameter, instance/static
field, and constructor rules do not change.

## TDD And Root Cause

On fixed HEAD `ee95607d07fda0ba945febe00eae2d1f1123c349`, the new
parser test compiled and then linked RED only for the missing
`ZrParser_ConstAssignment_PublishDiagnostic` API. The fixture registers a
same-name mutable decoy first, then publishes a write reference whose SymbolId
identifies the real const declaration. This rejects any first-name-match
implementation.

The root cause was a split identity pipeline. Parser semantic records already
retain `SymbolId -> astNode`, but LSP converted the query identity back through
its private scopes before invoking the parser evaluator. It then owned the
builder, fact append, and diagnostic lifetime.

## Implementation

The parser publisher extracts the identifier or final member range, calls
`SymbolAt`, and resolves the returned id with
`ZrParser_Semantic_FindSymbolById`. A resolved id without a record or AST node
fails closed. If no resolved reference exists, only the existing unique
current-prototype field context may be considered. The parser then evaluates
const legality, builds descriptor `2012`, and appends a persistent fact without
setting `compiler.hasError`.

LSP typecheck now invokes this API directly. Its target-range helper, semantic
id scan across LSP scopes, evaluator/builder calls, append path, internal entry,
and `semantic_analyzer_const_assignment.c` are deleted. The source contract
forbids their reintroduction in the remaining typecheck consumer.

## Verification

The final overlay has seven present code/test files and one deleted LSP
producer. Workspace-to-WSL bytes and workspace-to-MSVC SHA-256 match `7/7`;
the deleted file is absent from both snapshots.

GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 each return real exit zero for:

- exact SymbolId and broken-record producer cases `2/2`;
- query diagnostic disposition `11/11`;
- compiler semantic query diagnostics `64/64`;
- LSP source contracts `54` pass markers;
- dedicated const-assignment stdio transport.

GCC additionally passes both existing const-assignment analyzer cases: field
constructor context and local/parameter/static target coverage. The full
analyzer runner retains only the same closed-generic and borrow-range markers
recorded in Task 6.33; they are not counted as GREEN here.

## 状态与产出记录

- 完成时间：2026-08-28 18:04 +08:00。
- 状态：本子里程碑已完成；Plan 03 Task 6 继续进行。
- 完成项目：parser const fact publisher、exact SymbolId→semantic record AST
  identity、same-name decoy RED/GREEN、broken-record fail-closed、descriptor
  2012 persistent fact、LSP SymbolTable scan/producer 删除、source contract、
  三工具链 `2/11/64/54/stdio` 与 `7/7 + deleted 0` byte audit。
- 后续项目：interface const-field producer 当前由并行 Syntax05 property
  阶段持有的 `semantic_analyzer_symbols.c` 消费，须等待 exact ownership 释放；
  继续选择无重叠 analyzer producer，禁止跨会话兼容修改。
