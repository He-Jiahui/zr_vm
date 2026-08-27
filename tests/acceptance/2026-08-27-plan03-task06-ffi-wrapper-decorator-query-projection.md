---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_ffi_wrapper_decorators.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_ffi_wrapper_decorator_query_diagnostics.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/ffi/test_native_extern_contract.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_ffi_wrapper_decorator_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-27-plan03-task06-ffi-wrapper-decorator-query-projection.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.28 FFI Wrapper Decorator Query Projection

## Required Results

- Validate source FFI wrapper class decorators in parser/compiler, not LSP.
- Enforce canonical shape, value, view identity, and cross-decorator rules
  without name/text fallback in the consumer.
- Preserve descriptor `2019`, exact full-decorator range, canonical text, and
  explicit user-decision no-fix disposition.
- Publish one persistent semantic-query diagnostic and preserve every field in
  LSP and stdio projection.
- Remove the LSP class-wrapper producer and reject parallel `compiler_error`.

## TDD Evidence

The parser RED target failed to link because the public canonical wrapper
validator did not exist. The compiler and LSP contained duplicated extraction
and rule tables. Nine parser cases froze the valid full contract and every
invalid shape, value, identity, combination, and unknown-decorator boundary
before the LSP producer was removed.

## Final Evidence

On fixed code HEAD `94c78937266ed4c7ec530e948d08013d04da5240` plus the
twelve-path overlay, GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 each passed
the same thirteen direct checks. Shared Unity totals were `9/9`, `5/5`,
`63/63`, `15/15`, `30/30`, `74/74`, `124/124`, `127/127`, and native extern
`30/30` except one MSVC Unix-only ignore. LSP suites reported 16
query-diagnostic, 65 analyzer, and 52 source-contract pass markers; dedicated
stdio exited zero on every toolchain.

All twelve code/test paths matched between the workspace, WSL snapshot, and
MSVC snapshot. No accepted Unity summary contained a failure, and every runner
preserved the real process exit.

## Acceptance Decision

Accepted as the FFI wrapper class decorator slice of Plan 03 Task 6.
Parser/compiler is the only producer for wrapper decorator validity, and LSP
is a canonical compiler-query projection. Parameter decorators remain an
explicit future slice.

## 状态与产出记录

- 完成时间：2026-08-28 00:14 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC 同基线 13 项验收；Task 6
  继续进行。
- 完成项目：parser link RED、canonical wrapper contract、全部 wrapper
  decorator 形状/值域/组合规则、descriptor/range/text/no-fix contract、public
  parser validator、persistent query fact、normal compiler delegation、analyzer
  producer 删除、golden parity、source contract、独立 stdio、12-path 双快照
  byte audit 与真实退出证据。
