---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_call_hierarchy.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_hierarchy.c
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_type_hierarchy_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 7.8: Local Call Hierarchy

## Scope

This slice migrates source-local call hierarchy from source-text and symbol-name
scans to parser-owned incoming/outgoing call-edge queries. It covers free
functions available in one analyzer snapshot. Methods, lambdas, cross-project,
binary, and native external call hierarchy remain open. Syntax05-owned property
consumers and parser property support are untouched.

## TDD And Root Cause

On fixed baseline `a20b327bd075552fc83c7c12b2af64a8ca385b90`, call
hierarchy searched source lines for callable names and reconstructed callers
from document symbols. The RED fixture invokes one target twice, mutates all
display names, adds unrelated declarations, and reuses a version-one item after
a version-two update. It requires one grouped result with two exact call-site
ranges and no stale result.

The parser already exposed `IncomingCalls` and `OutgoingCalls`, but source
analysis had not published lexical scope ownership before building call edges.
The same AST function declaration could also have a compiler callable record
and an LSP display record. Without normalization, one call site produced two
target ids and split the graph.

## Implementation

`ZrParser_SemanticCalls_PublishSource` builds source scope facts and then
publishes call edges. A resolved function target is normalized to the first
valid parser function record with the exact same AST declaration identity;
duplicate display records collapse without consulting names or type text. A
parser regression constructs two ids and two types for one declaration and
requires exactly one canonical edge.

`lsp_semantic_call_hierarchy` prepares one callable by mapping the LSP display
symbol to that declaration's parser record inside the same immutable snapshot.
Only SymbolId, callable TypeId, and document version cross the protocol
boundary. Follow-up requests re-resolve the current snapshot, require all three
values to match, and consume only `IncomingCalls`, `OutgoingCalls`, and
`DeclarationOf`. Calls to one target are grouped by SymbolId and preserve each
exact `fromRange`.

The old line scanner, token/name matcher, document-symbol caller lookup, and
range reconstruction were deleted. Missing source ownership, unresolved
edges, malformed identity, cancellation, and stale versions fail closed.

## Verification

GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 each return real exit zero for
parser call queries `11/11`, semantic-query parity `7/7`, source contracts
`60/60`, advanced editor features `73/73`, and the combined type/call hierarchy
stdio smoke. The smoke verifies identity roundtrip, display-name mutation, two
grouped call-site ranges, and stale version rejection.

The full interface runner returns exit one on every toolchain with the same
`109 Pass / 4` pre-existing markers as Task 7.7; marker delta is zero, inlay
hints remain PASS, and the runner is not counted as GREEN. Workspace, WSL, and
MSVC code/test bytes match `10/10`; `git diff --check` passes.

## 状态与产出记录

- 完成时间：2026-08-28 23:21 +08:00。
- 状态：Task 7.8 local call hierarchy 子里程碑已完成；Plan 03 Task 7
  继续。
- 完成项目：source scope-to-call-edge publication、same-AST duplicate
  function record canonicalization、incoming/outgoing query projection、
  SymbolId/TypeId/version protocol identity、multiple `fromRanges` grouping、
  display-name/stale/unresolved fail-closed、legacy source/name scanner deletion、
  三工具链 focused/stdio 门禁、interface marker delta 0、三处 `10/10` byte
  audit。
- 未完成项目：rename、method/lambda call hierarchy、cross-project/binary/native
  external call/type hierarchy and implementations，以及其余 consumer
  migrations。
