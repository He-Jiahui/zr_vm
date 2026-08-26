---
related_code:
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder_fix_disposition.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder_ownership.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder_fix_disposition.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder_ownership.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_diagnostics.c
  - tests/acceptance/2026-08-26-plan03-task06-ownership-no-fix.md
doc_type: milestone-record
---

# Plan 03 Task 6.5: Ownership And Legacy No-Fix Producers

## Goal

Classify the final direct diagnostic-builder producers whose ownership repair
cannot be represented by an existing deterministic source edit.

## Contract

- Weak wake, borrow escape, loan escape, owner-to-plain escape, ownership
  mismatch, and use-after-move publish `REQUIRES_USER_DECISION`.
- The legacy ownership syntax warning publishes `INSUFFICIENT_CONTEXT`
  because the builder has no exact typed replacement range.
- Expected, actual, legacy qualifier, and wrapper names remain display data;
  they never select the reason or reconstruct an edit.
- Public producer signatures, stable codes, severity, ranges, and call sites
  remain unchanged.

## Implementation

Six general producers move from the general builder to the focused
fix-disposition module and reuse its cleanup-safe typed helper. The
ownership-specific use-after-move producer stays in its focused module and
sets the same explicit disposition after constructing the existing
diagnostic.

The general builder drops from 1085 to 972 lines. The direct unclassified
`return ZrParser_DiagnosticBuilder_Build(...)` inventory drops from seven to
zero. The dedicated test calls all seven APIs and verifies the exact reason.

## Verification

On the byte-equivalent `7b51bec673dc + 4 code/test overlays` snapshot, WSL GCC
11.4 and Clang 14 directly report 6/6 for the Task 6 diagnostic target and
46/46 for compiler semantic-query diagnostics. MSVC 19.44 reports the same
6/6 and 46/46. Every direct sequence exits zero.

## 状态与产出记录

- 完成时间：2026-08-26 17:26 +08:00。
- 状态：已完成并通过 GCC/Clang/MSVC 验收；不声明 Plan 03 Task 6
  完成。
- 完成项目：7 条 ownership/legacy producer typed reason、直接未分类
  builder inventory 清零、focused module 收口和三工具链回归。
- 后续项目：间接 no-fix producer 全量审计、LSP 纯协议投影、compiler/LSP
  golden parity 与重复 analyzer 删除。
