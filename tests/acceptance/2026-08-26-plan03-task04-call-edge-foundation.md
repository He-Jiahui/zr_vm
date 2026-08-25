---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_calls.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-26-plan03-task04-call-edge-foundation.md
tests:
  - tests/parser/test_semantic_query_calls.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 4.1 Source Call-Edge Foundation

## Scope

Accept only source function call edges formed from compiler-owned CALL,
expression, scope, and symbol facts. This acceptance excludes all text- or
AST-driven target reconstruction and all LSP consumer migration.

## Required Results

- An outgoing edge uses the enclosing lexical function's stable SymbolId as
  its caller and the resolved function SymbolId as its target.
- An incoming query returns that same edge from the target identity, and a
  position query returns it from the exact call site.
- Closed callable TypeId and target declaration range are projected from the
  existing call/symbol contract rather than a reconstructed signature.
- Missing target identity remains `TARGET_UNRESOLVED`; it never attaches to a
  same-name function in the registry.
- Repeated producer invocation does not duplicate an edge.

## Evidence

The dedicated MSVC static cache
`.codex/build-lsp-plan03-native-generic-msvc-r2` directly returned process exit
zero for `zr_vm_semantic_query_calls_test`; Unity reported `2 Tests 0 Failures
0 Ignored`. The shared tree has unrelated uncommitted work, so this does not
claim a clean-baseline or three-toolchain matrix result.

## Acceptance Decision

Accepted for the Task 4.1 source function call-edge foundation only. Lambda,
overload, conversion, receiver, binary/native, and LSP hierarchy coverage
remain unaccepted.

## 状态与产出记录

- 完成时间：2026-08-26 05:34 +08:00。
- 状态：已完成并将随本子项精确提交。
- 完成项目：caller/target stable-id edge、incoming/outgoing/position query、
  explicit unresolved edge、republication idempotence 和同名拒绝测试。
- 后续项目：完整 CallAt overload facts、external callable edge、LSP type/call hierarchy。
