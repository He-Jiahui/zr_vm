---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-26-plan03-task04-lambda-callers.md
tests:
  - tests/parser/test_semantic_query_calls.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 4.4 Lambda Caller Scope Facts

## Scope

Accept source lambda caller identity only when the lambda is the initializer
of a variable declaration traversed by the source scope-fact builder. This is
not an acceptance of arbitrary expression-position lambda traversal.

## Required Results

- A call inside `var callback = fn() => { ... };` is emitted with the existing
  lambda function SymbolId as `callerSymbolId`.
- Incoming and outgoing queries retain the same lambda caller identity.
- The nearest nested `FUNCTION` scope wins selection even when its owner is
  invalid; that edge must report `CALLER_UNAVAILABLE`, never the outer caller.
- The producer must not scan a name, choose an outer same-name declaration, or
  retain an AST pointer in the query result.

## Evidence

The isolated MSVC static cache
`.codex/build-lsp-plan03-native-generic-msvc-r2` directly ran
`zr_vm_semantic_query_calls_test` after both RED states. The final executable
returned process exit zero and Unity reported `5 Tests 0 Failures 0 Ignored`.
Direct adjacent query regressions also passed: `29/0`, `19/0`, `3/0`, `19/0`,
`3/0`, and `46/0`. This is isolated MSVC evidence only, not a clean-baseline
or three-toolchain claim.

## Acceptance Decision

Accepted for variable-initialized source lambdas and invalid nested function
scope handling. Other lambda positions, call mapping, conversion/exactness,
receiver TypeIds, binary/native call facts, and LSP presentation remain
unaccepted.

## 状态与产出记录

- 完成时间：2026-08-26 06:34 +08:00。
- 状态：已完成并将随本子项精确提交。
- 完成项目：lambda caller identity、incoming/outgoing parity、最近 scope
  owner 缺失的 fail-closed 查询，以及 isolated MSVC acceptance evidence。
- 后续项目：完整 lambda scope traversal、剩余 canonical call facts 与 LSP
  consumer migration。
