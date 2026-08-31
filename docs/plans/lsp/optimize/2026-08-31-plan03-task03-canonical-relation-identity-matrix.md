---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations_identity.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations_order.c
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/parser/test_semantic_query_relation_canonical_identity_matrix_cases.h
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 3.19: Canonical Relation Identity Matrix

## Scope

Freeze the remaining in-snapshot canonical identity cases requested by the
Task 3 relation matrix without adding producer or consumer heuristics.

## Characterization Matrix

Four focused cases were added:

- two nominal types named `Node`, each with a same-named `Base`, remain isolated
  by their distinct module identities and canonical TypeIds;
- open `Box<T>` and closed `Box<int>` edges remain distinct while both project
  the `lib.generic` definition module;
- an `OuterAlias -> InnerAlias -> Target` chain returns both exact SymbolId
  hops when querying the intermediate alias;
- same-named overload implementations resolve only through their exact target
  SymbolId and callable TypeId.

Existing compiler/query behavior passed each case on the first synchronized
run. This milestone therefore adds contract coverage only and does not change
production code.

## Verification

The isolated GCC and Clang relation executables both pass `28/28` with real
exit 0. The initial unsynchronized GCC invocation was discarded because its
mirror still ran 24 tests; all reported evidence comes from rebuilt binaries
after exact test-file synchronization.

MSVC, the full 16-target matrix, interface tests, and stdio smoke were not run
for this test-only milestone. Actual multi-project provider reloads and
binary/native declarations without source locations remain producer-level
gaps, so the broad Task 3 checklist item remains open.

## 状态与产出记录

- 完成时间：2026-08-31 15:04 +08:00。
- 状态：Task 3.19 子里程碑已完成；Plan 03 Task 3继续进行。
- 完成项目：同名跨module隔离；generic open/closed edge；alias逐跳identity；overload
  SymbolId隔离；GCC/Clang `28/28`。
- 后续项目：补actual multi-project generation reload与binary/native sourceless producer
  parity，再执行MSVC、16-target与stdio总门禁。
