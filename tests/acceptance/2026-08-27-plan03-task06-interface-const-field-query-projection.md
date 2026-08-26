---
related_code:
  - zr_vm_parser/include/zr_vm_parser/interface_contract.h
  - zr_vm_parser/src/zr_vm_parser/semantic/interface_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_semantic_query.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_interface_const_field_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-27-plan03-task06-interface-const-field-query-projection.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.16 Interface Const-Field Query Projection

## Required Results

- Parser owns interface const-field requirement traversal and structured
  diagnostic construction.
- Compiler and LSP share descriptor `2014`, code, severity, exact primary and
  related ranges, help URI, message family, and no-fix disposition.
- Missing fields and fields that drop `const` are both reported.
- LSP does not retain inheritance/member policy, perform symbol-table or name
  matching, leave a missing-field TODO, or directly construct the diagnostic.
- Focused parser/query/LSP tests and dedicated stdio transport pass under GCC,
  Clang, and MSVC from one byte-exact code/test overlay.

## Evidence

The initial REDs demonstrated a plain compiler error, absent descriptor `2014`,
an analyzer-owned non-const check, and no LSP result for a missing field. The
first protocol run also exposed a non-token related range. The parser-owned
query/builder and stored interface-field name range resolved all five gaps.

The clean acceptance snapshot was fixed at HEAD `d12911e` plus 13 code/test
paths. SHA-256 comparison reported `13/13` overlay files identical between the
working tree and Windows MSVC snapshot; the same overlay tar populated the WSL
GCC/Clang snapshot. GCC 11.4, Clang 14, and MSVC 19.44 each returned real exit
zero for the same ten targets. Unity totals include compiler diagnostics
`53/53`, query disposition `8/8`, semantic facts `14/14`, semantic query
`30/30`, type inference `123/123`, and compiler integration `127/127`; all four
LSP suites also passed.

The dedicated stdio fixture requires exactly two
`const_interface_mismatch` diagnostics. The mutable implementation primary
range is zero-based line 4 characters 12..19, the missing implementation range
is line 6 characters 6..20, and both related links target line 1 characters
14..21. It also freezes descriptor `2014`, canonical full messages, registered
help URI, `requires_user_decision`, and an empty fixes payload. All three
toolchain servers passed it.

## Acceptance Decision

Accepted for interface const-field query projection. Other semantic diagnostic
families remain to migrate, so Plan 03 Task 6 remains in progress.

## 状态与产出记录

- 完成时间：2026-08-27 04:41 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC focused、compiler integration
  与独立 stdio 验收；Task 6 继续进行。
- 完成项目：canonical missing/drop-const enumeration、structured descriptor
  2014、exact primary/related ranges、no-fix disposition、compiler/LSP parity、
  duplicate LSP policy 删除与三工具链 transport 证据。
