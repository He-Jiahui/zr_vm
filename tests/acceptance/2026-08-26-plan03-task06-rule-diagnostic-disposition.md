---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_ownership.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_ref_struct_rules.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder_type_mismatch.c
tests:
  - tests/parser/test_ref_struct_restrictions.c
  - tests/parser/test_reference_escape_closure_suspension.c
  - tests/parser/test_percent_syntax_cutover.c
  - tests/parser/test_semantic_query_diagnostics.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.7 Rule Diagnostic Disposition

## Required Results

- Ref-struct, reference-escape, strong-cycle, and non-convertible mismatch
  diagnostics publish `REQUIRES_USER_DECISION`.
- Legacy cutover diagnostics publish `INSUFFICIENT_CONTEXT`.
- Typed mismatch conversions retain their placeholder cast fix.
- Existing related locations and stable diagnostic fields remain unchanged.
- No policy is inferred from diagnostic text.

## Evidence

GCC 11.4, Clang 14, and MSVC 19.44 directly execute the five focused targets
at 11/11, 13/13, 7/7, 7/7, and 47/47 with zero failures. Every sequence exits
zero. GCC and Clang consume one byte-matched fixed ext4 snapshot with separate
build directories.

## Acceptance Decision

Accepted for the covered parser/compiler rule producers. Producer completeness
enforcement and LSP parity remain outside this record.

## 状态与产出记录

- 完成时间：2026-08-26 17:45 +08:00。
- 状态：已完成并通过 GCC/Clang/MSVC 验收。
- 完成项目：typed rule disposition、typed mismatch branch preservation、
  cleanup behavior 和 direct regression evidence。
