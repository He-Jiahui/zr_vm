---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/acceptance/2026-08-26-plan03-task04-lambda-callers.md
doc_type: milestone-record
---

# Plan 03 Task 4.4: Lambda Caller Scope Facts

## Goal

Publish source call edges from a variable-initialized lambda with the
compiler-registered lambda function SymbolId, and fail closed when the nearest
nested function scope does not have a valid function owner.

## Implementation

- The source scope-fact builder visits a variable declaration initializer and
  recognizes a lambda AST node structurally.
- A lambda publishes one `FUNCTION` scope keyed by its existing registered
  function SymbolId. Its parameter declarations and block inherit that owner.
- The call-edge producer chooses the narrowest containing `FUNCTION` scope
  before validating the owner. An invalid nearest owner returns
  `CALLER_UNAVAILABLE`; it never skips to an outer function.
- This slice neither creates a lambda symbol nor derives identity from a name,
  call spelling, or LSP/AST fallback. Other lambda expression positions remain
  unavailable until the scope walker covers them structurally.

## Verification

- RED: before lambda scope publication, the direct MSVC
  `zr_vm_semantic_query_calls_test` run reported no outgoing edge for the
  lambda SymbolId.
- RED: with the previous caller selector, a nested function scope with an
  invalid owner incorrectly published the enclosing function as caller.
- GREEN: the direct MSVC executable reported `5 Tests 0 Failures 0 Ignored`
  with process exit zero. It verifies both lambda caller identity and the
  no-outer-fallback failure mode.
- Regression: direct MSVC runs also passed semantic query `29/0`, query
  symbols `19/0`, query contract `3/0`, canonical consumers `19/0`, semantic
  display `3/0`, and compiler semantic-query diagnostics `46/0`.

## 状态与产出记录

- 完成时间：2026-08-26 06:34 +08:00。
- 状态：已完成并将随本子项精确提交；不声明 Plan 03 Task 4 完成。
- 完成项目：变量初始化器 lambda 的 function scope、lambda caller
  SymbolId call-edge 归属、最近无 owner scope 的 fail-closed 行为，以及
  对应 RED/GREEN/相邻查询回归。
- 后续项目：其他 lambda 表达式位置的结构化 scope 遍历、参数映射、
  conversion/exactness、receiver TypeId、binary/native call-edge producer 和
  LSP consumers。
