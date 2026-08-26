---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
tests:
  - tests/parser/test_semantic_query_contract.c
  - tests/parser/test_semantic_query.c
  - tests/parser/test_semantic_query_diagnostics.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.8 Diagnostic Completeness Gate

## Required Results

- Incomplete persistent diagnostics fail closed.
- Typed fixes and explicit no-fix reasons remain accepted.
- Conditional migration builders publish exactly one disposition branch.
- Exact replacement edits remain machine applicable.
- No policy is inferred from display text.

## Evidence

GCC 11.4, Clang 14, and MSVC 19.44 directly execute the three focused targets
at 4/4, 30/30, and 8/8 with zero failures. Every sequence exits zero. GCC and
Clang consume one byte-matched fixed ext4 snapshot with separate build
directories.

## Acceptance Decision

Accepted for the persistent semantic fact boundary and conditional migration
builders. LSP projection and compiler/LSP parity remain outside this record.

## 状态与产出记录

- 完成时间：2026-08-26 17:53 +08:00。
- 状态：已完成并通过 GCC/Clang/MSVC 验收。
- 完成项目：fail-closed disposition gate、conditional fix/no-fix branches、
  public-contract compatibility 和 direct regression evidence。
