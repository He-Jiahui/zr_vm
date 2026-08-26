---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_ownership.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_ref_struct_rules.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder_type_mismatch.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_ownership.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_ref_struct_rules.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder_type_mismatch.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_ref_struct_restrictions.c
  - tests/parser/test_reference_escape_closure_suspension.c
  - tests/parser/test_percent_syntax_cutover.c
  - tests/parser/test_semantic_query_diagnostics.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/acceptance/2026-08-26-plan03-task06-rule-diagnostic-disposition.md
doc_type: milestone-record
---

# Plan 03 Task 6.7: Rule Diagnostic Disposition

## Goal

Classify parser/compiler rule diagnostics that are built outside the focused
diagnostic-builder modules and therefore remained incomplete after the direct
builder inventory reached zero.

## Contract

- Ref-struct restrictions, reference escapes, resource strong-cycle warnings,
  and type mismatches without a typed conversion publish
  `REQUIRES_USER_DECISION`.
- Type mismatch with a canonical conversion hint retains its typed placeholder
  cast fix and no no-fix reason.
- Removed legacy syntax publishes `INSUFFICIENT_CONTEXT`; migration guidance
  is not an exact replacement edit.
- Reference-origin related information and all existing stable diagnostic
  fields remain unchanged.

## Implementation

Each compiler/parser producer sets its disposition immediately after building
the diagnostic and frees it before falling back to the existing plain error
path if that operation fails. No producer classifies from message, code, type
spelling, or member name.

The tests exercise source-level ref-struct and escape failures, percent and
non-percent cutover forms, a resource self-cycle warning, and both branches of
the typed mismatch builder. The dirty shared-resource test remains untouched;
the strong-cycle assertion uses an independent compiler-query fixture.

## Verification

On the byte-equivalent `ba538f5 + 10 code/test overlays` snapshot, WSL GCC 11.4
and Clang 14 directly report 11/11 ref-struct, 13/13 reference escape, 7/7
cutover, 7/7 semantic diagnostic, and 47/47 compiler diagnostic tests. MSVC
19.44 reports the same results. Every direct sequence exits zero.

## 状态与产出记录

- 完成时间：2026-08-26 17:45 +08:00。
- 状态：已完成并通过 GCC/Clang/MSVC 验收；不声明 Plan 03 Task 6
  完成。
- 完成项目：5 类 parser/compiler rule producer typed reason、type-mismatch
  fix/no-fix 分支、failure cleanup 和跨层端到端断言。
- 后续项目：剩余 producer 完整性门禁、LSP 纯协议投影、compiler/LSP
  golden parity 与重复 analyzer 删除。
