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
  - tests/acceptance/2026-08-26-plan03-task06-syntax-no-fix-producers.md
doc_type: milestone-record
---

# Plan 03 Task 6.2: Syntax No-Fix Producers

## Goal

Convert the existing syntax diagnostics that were already asserted to have no
machine edit from an ambiguous empty-fix state to explicit parser-owned no-fix
reasons.

## Contract

- Array element assignment requires the user to move or replace an expression
  and publishes `REQUIRES_USER_DECISION`.
- Missing conditional consequent and alternate expressions publish
  `REQUIRES_USER_DECISION`; the compiler cannot invent branch values.
- Missing conditional colon remains machine-fixable when an alternate
  expression is present. Without that expression, it publishes
  `INSUFFICIENT_CONTEXT` instead of an empty fix array.
- Every producer sets the structured enum through the shared setter. No
  decision depends on diagnostic code, message text, or an LSP consumer.

## Implementation

The four affected builders moved from the 1450-line general builder into
`diagnostic_builder_fix_disposition.c`. The focused module owns no-fix
construction, cleanup on failed disposition publication, and the conditional
colon fix/no-fix split. The general builder is reduced by 77 lines and parser
call sites remain unchanged.

The dedicated Task 6 diagnostic test now invokes each producer and requires
the exact enum. The earlier copy/query and mutual-exclusion tests remain in the
same target, while the established 46-case compiler diagnostic target protects
all existing builder and publication behavior.

## Verification

On the byte-equivalent `779ec8fefea4 + 3 code/test overlays` snapshot, WSL GCC
11.4 and Clang 14 directly report 3/3 for the Task 6 diagnostic target and
46/46 for compiler semantic-query diagnostics. MSVC 19.44 reports the same
3/3 and 46/46. Every direct sequence exits zero.

## 状态与产出记录

- 完成时间：2026-08-26 16:59 +08:00。
- 状态：已完成并通过 GCC/Clang/MSVC 验收；不声明 Plan 03 Task 6
  完成。
- 完成项目：4 条 syntax no-fix 分支的 typed reason、conditional colon
  双态合同、builder 模块化和 producer-level RED/GREEN。
- 后续项目：审计并分类其余 parser/compiler diagnostics、LSP 纯协议投影、
  compiler/LSP golden parity 与重复 analyzer 删除。
