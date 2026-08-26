---
related_code:
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder_fix_disposition.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder_fix_disposition.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_diagnostics.c
  - tests/acceptance/2026-08-26-plan03-task06-pattern-import-no-fix.md
doc_type: milestone-record
---

# Plan 03 Task 6.4: Pattern And Import No-Fix Producers

## Goal

Classify using, import, and pattern diagnostics whose repair requires choosing
a different semantic binding rather than applying a deterministic source edit.

## Contract

- Invalid using binders and non-constant import paths publish
  `REQUIRES_USER_DECISION`.
- Pattern shape, unknown-field, arity, and variant mismatches publish the same
  reason because the compiler cannot choose the intended canonical payload.
- Directive, union, variant, and field names affect display text only; they do
  not select the reason or reconstruct an edit.
- Public producer signatures, stable codes, severity, ranges, and parser call
  sites remain unchanged.

## Implementation

Six builders move from the general builder to the focused fix-disposition
module and reuse its cleanup-safe no-fix helper. Existing dynamic message,
cause, and suggestion formatting moves with the producer without behavioral
changes. The general builder drops from 1270 to 1085 lines, and the inventory
of direct unclassified `Build` returns falls from 13 to 7.

The dedicated test invokes all six APIs with concrete and default metadata and
requires `REQUIRES_USER_DECISION`. Existing Task 6 contract and producer tests
remain active.

## Verification

On the byte-equivalent `d214ca883521 + 3 code/test overlays` snapshot, WSL GCC
11.4 and Clang 14 directly report 5/5 for the Task 6 diagnostic target and
46/46 for compiler semantic-query diagnostics. MSVC 19.44 reports the same
5/5 and 46/46. Every direct sequence exits zero.

## 状态与产出记录

- 完成时间：2026-08-26 17:14 +08:00。
- 状态：已完成并通过 GCC/Clang/MSVC 验收；不声明 Plan 03 Task 6
  完成。
- 完成项目：6 条 using/import/pattern producer typed reason、dynamic display
  与 policy 分离、focused module 归属和未分类 inventory 缩减。
- 后续项目：最后 7 条 ownership/legacy producer 分类、LSP 纯协议投影、
  compiler/LSP golden parity 与重复 analyzer 删除。
