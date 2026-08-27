---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_parameter_decorators.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_declaration.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_extern_parameter_decorator_query_diagnostics.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/ffi/test_native_extern_contract.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_extern_parameter_decorator_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-28-plan03-task06-extern-parameter-decorator-query-projection.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.29 Extern Parameter Decorator Query Projection

## Required Results

- Validate extern parameter decorators in parser/compiler, not LSP.
- Accept only canonical direction and charset shapes and values; fail closed on
  unknown decorators.
- Preserve descriptor `2019`, exact full-decorator range, canonical text, and
  explicit user-decision no-fix disposition.
- Preserve query/LSP golden parity and publish no parallel `compiler_error`.
- Keep ordinary function parameter decorators outside the extern validator.

## TDD Evidence

The initial parser target failed to link because the public validator was
absent. LSP REDs then proved valid charset rejection, absent canonical facts
for invalid charset and direction conflicts, local helper ownership, and an
ordinary-function false positive. A later parser RED isolated bare charset to
the generic unknown-decorator message. Seven parser cases and the context
test froze the corrected contract before final acceptance.

## Final Evidence

On fixed HEAD `39ceace26ca87b7845722ea4d0331b8d34e56e11` plus the
twelve-path overlay, GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 passed the
same fifteen direct checks. Shared Unity results were `7/7`, `9/9`, `5/5`,
`5/5`, `63/63`, `15/15`, `30/30`, `74/74`, `124/124`, `127/127`, and native
extern `30/30` except one MSVC Unix-only ignore. LSP suites reported 16 query,
69 analyzer, and 53 source-contract pass markers; dedicated stdio exited zero
on every toolchain.

All twelve code/test paths matched between the workspace, WSL snapshot, and
MSVC snapshot. Tests were serial, no accepted summary contained a failure,
and every runner preserved its real process exit.

## Acceptance Decision

Accepted as the extern parameter decorator slice of Plan 03 Task 6.
Parser/compiler is the only semantic producer and LSP is a canonical
compiler-query projection. Plan 03 Task 6 remains open for later semantic
diagnostic slices.

## 状态与产出记录

- 完成时间：2026-08-28 01:40 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC 同基线 15 项验收；Task 6
  继续进行。
- 完成项目：parser link RED、7 项 canonical parameter rule tests、ordinary
  parameter context RED/GREEN、bare charset RED/GREEN、public parser validator、
  persistent diagnostic fact、normal compiler delegation、LSP duplicate producer
  删除、2 项 golden parity、source contract、独立 stdio、12-path 双快照 byte
  audit 与真实退出证据。
