---
related_code:
  - zr_vm_parser/include/zr_vm_parser/const_assignment.h
  - zr_vm_parser/src/zr_vm_parser/semantic/const_assignment.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_compiler_const_assignment_query_producer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_const_assignment_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-28-plan03-task06-const-assignment-fact-producer-migration.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.34 Const Assignment Fact Producer Migration

## Required Results

- Resolve assignment targets from parser query SymbolId and semantic record
  identity, never by LSP scope/name reconstruction.
- Fail closed when a resolved identity has no canonical symbol record.
- Preserve the established constructor exception and descriptor `2012`
  payload for every const target kind.
- Publish persistent facts without setting the normal compiler error state.
- Delete the LSP range extractor, scope scan, builder/append path, and module.

## TDD Evidence

The new target linked RED for the missing parser publisher. Its positive case
registers a mutable declaration under the same name before the real const
symbol; only the write reference SymbolId selects the correct declaration. Its
negative case publishes a resolved id with no semantic record and requires no
diagnostic. The final target is `2/2` on all three toolchains.

## Final Evidence

GCC, Clang, and MSVC each pass producer `2/2`, disposition `11/11`, compiler
diagnostics `64/64`, `54` source-contract markers, and dedicated stdio with
real exits. Seven present snapshot files match exactly and the removed LSP
producer is absent. GCC's two existing const analyzer scenarios also pass;
the full runner's two unrelated baseline markers remain explicitly excluded.

## Acceptance Decision

Accepted as the final const-assignment persistent-fact producer migration for
Plan 03 Task 6. Remaining analyzer-owned producers keep Task 6 active.

## 状态与产出记录

- 完成时间：2026-08-28 18:04 +08:00。
- 状态：本子项已验收；Plan 03 Task 6 继续进行。
- 完成项目：valid link RED、same-name SymbolId identity、missing-record
  fail-closed、descriptor 2012 persistent query、LSP producer 删除、三工具链
  `2/11/64/54/stdio`、`7/7` byte match 与 deleted `0`。
