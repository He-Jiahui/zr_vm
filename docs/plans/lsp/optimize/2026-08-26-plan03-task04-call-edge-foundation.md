---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_calls.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_calls.c
doc_type: milestone-record
---

# Plan 03 Task 4.1: Source Call-Edge Foundation

## Goal

Publish read-only call edges from existing compiler facts. A caller, target,
callable TypeId, and ranges must come from stable snapshot identity; no query
or consumer may identify a callee by source spelling.

## Implementation

- Adds snapshot-owned `SZrSemanticCallEdgeFact` storage and lifecycle to
  `SZrSemanticContext`.
- Adds `ZrParser_SemanticCalls_Publish` after source scope-fact publication.
  It joins existing `CALL` reference facts to the narrowest function-owned
  lexical scope and matching call expression fact.
- Adds `CallEdgesAt`, `OutgoingCalls`, and `IncomingCalls`. Results are
  deterministic value projections containing caller/target SymbolIds, closed
  callable TypeId, exact call-site range, target declaration range, and a
  structured resolution state.
- Resolved facts require a registered function target. Missing caller scope,
  unresolved target identity, and missing target declaration range remain
  explicit states. A same-name function is never selected as a substitute.
- Re-publication is idempotent by stable endpoint identity, callable TypeId,
  call-site range, and resolution state.

## Exclusions

This slice does not publish lambda caller identity, overload candidates,
argument mappings, conversions, receiver TypeIds, binary/native call edges, or
any LSP hierarchy consumer. It does not modify parser/type-inference call fact
production or recover targets from AST/name/text.

## Verification

- RED: the new isolated target failed because `semantic_calls.h` did not
  exist.
- GREEN: the dedicated MSVC static cache directly built and ran
  `zr_vm_semantic_query_calls_test` with `2 Tests 0 Failures 0 Ignored` and
  process exit zero. The test covers a compiled source function edge in both
  directions and an unresolved same-name target that remains unresolved.

## 状态与产出记录

- 完成时间：2026-08-26 05:34 +08:00。
- 状态：已完成并将随本子项精确提交；不声明 Plan 03 Task 4 完成。
- 完成项目：snapshot call-edge carrier、function-scope caller identity、
  resolved incoming/outgoing/position queries、explicit unresolved reason、
  幂等发布与 no-name-fallback 测试。
- 后续项目：lambda caller、overload candidate/argument mapping/conversion、
  receiver TypeId、source/binary/native external edge 与 LSP hierarchy consumers。
