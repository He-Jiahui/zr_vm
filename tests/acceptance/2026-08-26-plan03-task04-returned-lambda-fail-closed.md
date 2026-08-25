---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-26-plan03-task04-returned-lambda-fail-closed.md
tests:
  - tests/parser/test_semantic_query_calls.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 4.5 Returned Lambda Caller Fails Closed

## Scope

Accept only the fail-closed caller result for a returned source lambda that
lacks a compiler-registered lambda SymbolId. This does not accept synthetic
lambda identity, returned-lambda outgoing navigation, or arbitrary expression
traversal.

## Required Results

- A returned lambda is reached structurally through `RETURN_STATEMENT.expr`.
- Its nearest function scope has no owner when no exact compiler SymbolId
  exists.
- A resolved call in that lambda body returns `CALLER_UNAVAILABLE`.
- The enclosing function has no outgoing edge for the returned lambda's call.
- No target or caller is recovered from text, name, or a retained AST pointer.

## Evidence

The isolated MSVC static cache
`.codex/build-lsp-plan03-native-generic-msvc-r2` directly ran
`zr_vm_semantic_query_calls_test`. The pre-fix executable failed because the
caller SymbolId was the enclosing `makeRunner` SymbolId `2`. The final direct
run returned process exit zero and Unity reported `6 Tests 0 Failures 0
Ignored`. Adjacent query regressions passed `29/0`, `19/0`, `3/0`, `19/0`,
`3/0`, and `46/0`, all with direct process exit zero. This is isolated MSVC
evidence only, not a clean-baseline or three-toolchain claim.

## Acceptance Decision

Accepted for containment of returned source lambda calls with unavailable
caller identity. Compiler registration of returned lambdas, other expression
positions, remaining call facts, and LSP consumers remain unaccepted.

## 状态与产出记录

- 完成时间：2026-08-26 06:44 +08:00。
- 状态：已完成并将随本子项精确提交。
- 完成项目：returned lambda caller fail-closed、外层 outgoing-edge
  污染防护，以及 isolated MSVC acceptance evidence。
- 后续项目：exact lambda SymbolId producer、完整 expression traversal 与
  LSP call hierarchy consumption。
