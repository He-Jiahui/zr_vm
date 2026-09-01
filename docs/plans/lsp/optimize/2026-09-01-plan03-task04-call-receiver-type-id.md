---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_canonical.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
tests:
  - tests/parser/test_canonical_consumers.c
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_query_call_selection_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 4.28: Call Receiver Type Identity

## Scope

Publish the canonical receiver TypeId for a resolved member call through the
parser semantic reference fact and `ZrParser_SemanticQuery_CallAt`. The slice
does not add an LSP-side receiver resolver, a member-name fallback, or
binary/native callable parity.

## TDD And Implementation

The RED extended source free-call, generic receiver-call, readonly receiver,
and mutable receiver canonical consumer cases. GCC failed compilation because
`SZrParserSemanticCallQuery.receiverTypeId` did not exist.

The member-call producer now receives the already-resolved receiver
`SZrInferredType`, interns it in the semantic context, and stores the resulting
TypeId on the same `CALL` reference fact that carries the closed callable
TypeId and resolved target identity. Free and constructor call producers leave
the field invalid.

`CallAt` copies the receiver TypeId only from the selected reference fact. It
requires member calls to carry a valid receiver TypeId and non-member calls to
carry none. Equal resolved target facts with conflicting nonzero receiver
TypeIds are rejected. The lower-layer query test freezes all three states and
verifies that failed queries clear the value output.

## Verification

The implementation was synchronized into the isolated source snapshot at
`/home/hejiahui/.cache/zr-lsp-inline-417-src`. GCC and Clang used independent
build caches and ran the same 12 targets with real process exit 0.

Parser totals were canonical consumers 21, semantic facts 17, semantic query
30, call query 31, public contract 6, diagnostics 13, relations 29, symbols 24,
and type inference 124, all with zero failures. LSP semantic-query parity,
source contracts, and the complete interface suite also passed on both
toolchains.

MSVC, the repository-wide final matrix, and stdio/CLI smoke were not run for
this focused submilestone. Source, `.zro`, and native descriptor callable parity
remains the other unchecked Task 4 item.

## 状态与产出记录

- 完成时间：2026-09-01 18:22 +08:00。
- 状态：Task 4.28 call receiver TypeId子里程碑已完成；Plan 03 Task 4与Task 8继续进行。
- 完成项目：member-call canonical receiver TypeId fact；`CallAt.receiverTypeId`投影；
  member/free形态一致性门禁；冲突receiver identity fail-closed；source free/generic/readonly/
  mutable receiver覆盖；GCC/Clang 12-target focused验证。
- 后续项目：source/`.zro`/native descriptor callable parity；MSVC、完整target matrix、
  stdio/CLI smoke与Task 8总验收。
