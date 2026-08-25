---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-26-plan03-task04-lambda-callers.md
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/acceptance/2026-08-26-plan03-task04-returned-lambda-fail-closed.md
doc_type: milestone-record
---

# Plan 03 Task 4.5: Returned Lambda Caller Fails Closed

## Goal

Prevent a call inside a returned lambda without a compiler-registered lambda
SymbolId from being attributed to the enclosing function in the source call
graph.

## Implementation

- The source scope-fact walker structurally visits `RETURN_STATEMENT.expr`.
- A returned lambda receives the existing lambda function scope shape. When no
  registered lambda SymbolId exists, that scope owner remains invalid.
- The established narrowest-function-scope rule then publishes
  `CALLER_UNAVAILABLE`. It does not synthesize a lambda SymbolId, scan a name,
  or fall back to the enclosing return function.

## Verification

- RED: the direct MSVC `zr_vm_semantic_query_calls_test` run reported caller
  SymbolId `2` for `callee()` inside `makeRunner`'s returned lambda, proving
  the call was incorrectly attributed to the outer function.
- GREEN: the same executable reported `6 Tests 0 Failures 0 Ignored` with
  process exit zero. The edge now has an invalid caller and
  `CALLER_UNAVAILABLE`; `makeRunner` has no outgoing edge for that call.
- Regression: direct MSVC runs also passed semantic query `29/0`, query
  symbols `19/0`, query contract `3/0`, canonical consumers `19/0`, semantic
  display `3/0`, and compiler semantic-query diagnostics `46/0`.

## 状态与产出记录

- 完成时间：2026-08-26 06:44 +08:00。
- 状态：已完成并将随本子项精确提交；不声明 Plan 03 Task 4 完成。
- 完成项目：returned lambda return-expression scope 遍历、无 SymbolId
  caller 的 fail-closed edge，以及防止外层函数 outgoing graph 污染的
  RED/GREEN/回归。
- 后续项目：compiler 注册 returned lambda identity、其他表达式位置的
  traversal、参数映射、conversion/exactness、receiver TypeId、binary/native
  producer 和 LSP consumers。
