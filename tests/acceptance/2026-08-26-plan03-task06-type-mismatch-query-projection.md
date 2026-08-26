---
related_code:
  - zr_vm_parser/include/zr_vm_parser/type_inference.h
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_expected_type.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_expected_type.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_type_mismatch_diagnostic_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_diagnostic_fix_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.11 Type Mismatch Query Projection

## Required Results

- Parser/compiler compatibility owns the structured type mismatch diagnostic.
- Assignment, explicit initializer, and return mismatch facts preserve exact
  primary range, expected declaration related range, descriptor identity,
  help URI, and typed placeholder cast fix.
- LSP projects semantic query diagnostics and contains no dedicated type
  mismatch policy producer.
- A single mismatch site does not emit a broad duplicate fact.
- Focused parser, LSP, type-inference, compiler-integration, and stdio protocol
  regressions pass under GCC, Clang, and MSVC.

## Evidence

The parser RED was an undefined reference to
`ZrParser_AssignmentCompatibility_CheckDetailed`. The first implementation
made the parser fact test pass but left an LSP RED with three observed mismatch
diagnostics: one broad assignment diagnostic without related information and
two precise canonical diagnostics. Excluding assignment expressions from the
generic expression-statement inference path removed the duplicate. The final
LSP fixture requires exactly two mismatch diagnostics and validates every
canonical field on both.

GCC 11.4, Clang 14, and MSVC 19.44 each produced real exit zero for compiler
semantic query diagnostics `48/48`, LSP semantic query diagnostics, LSP source
contracts, type inference `123/123`, and compiler integration `127/127`. Each
toolchain's stdio executable also passed the structured-diagnostic fix smoke
with real exit zero.

## Acceptance Decision

Accepted for the type-mismatch query-projection slice. Remaining analyzer-owned
semantic diagnostics and final compiler/LSP golden parity remain outside this
record and keep Plan 03 Task 6 in progress.

## 状态与产出记录

- 完成时间：2026-08-26 22:55 +08:00。
- 状态：已完成本子项并通过 GCC/Clang/MSVC 与 stdio 验收；Task 6 继续进行。
- 完成项目：canonical mismatch fact、related range、typed fix、LSP-only
  projection、旧 producer 删除、重复诊断防回归与扩展矩阵证据。
