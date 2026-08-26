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
  - tests/acceptance/2026-08-26-plan03-task06-syntax-recovery-no-fix.md
doc_type: milestone-record
---

# Plan 03 Task 6.3: Syntax Recovery No-Fix Producers

## Goal

Classify the remaining direct syntax-recovery builders whose suggested repair
requires user-authored semantics or a source insertion range the producer does
not own.

## Contract

- Missing assignment values, right operands, condition expressions, and
  member names publish `REQUIRES_USER_DECISION`.
- Missing test-name close publishes `INSUFFICIENT_CONTEXT` because the builder
  receives only a primary range, not a validated insertion range.
- Dynamic operator and statement-kind text remains diagnostic display input;
  it does not select the structured no-fix reason.
- No LSP consumer, code/message matcher, or request position manufactures an
  edit or reason.

## Implementation

Five builders move from `diagnostic_builder.c` into the existing focused
fix-disposition module. The module reuses one cleanup-safe helper for all
explicit no-fix construction while preserving the exact stable codes,
severity, ranges, messages, causes, and suggestions. The general builder drops
from 1373 to 1270 lines; public declarations and parser call sites do not
change.

The dedicated Task 6 test invokes all five producer APIs and requires the exact
reason. Existing fix/no-fix, deep-copy, query materialization, and first-slice
producer tests remain active in the same target.

## Verification

On the byte-equivalent `628db577b36a + 3 code/test overlays` snapshot, WSL GCC
11.4 and Clang 14 directly report 4/4 for the Task 6 diagnostic target and
46/46 for compiler semantic-query diagnostics. MSVC 19.44 reports the same
4/4 and 46/46. Every direct sequence exits zero.

## 状态与产出记录

- 完成时间：2026-08-26 17:07 +08:00。
- 状态：已完成并通过 GCC/Clang/MSVC 验收；不声明 Plan 03 Task 6
  完成。
- 完成项目：5 条 syntax recovery producer 的 typed reason、动态展示与
  disposition 分离、focused module 归属和大文件缩减。
- 后续项目：pattern/import/using 与 ownership producer 分类、LSP 纯协议
  投影、compiler/LSP golden parity 与重复 analyzer 删除。
