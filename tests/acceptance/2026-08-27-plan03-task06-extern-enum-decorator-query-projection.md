---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_enum_decorators.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_declaration.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_extern_enum_decorator_query_diagnostics.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/ffi/test_native_extern_contract.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_extern_enum_decorator_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-27-plan03-task06-extern-enum-decorator-query-projection.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.27 Extern Enum Decorator Query Projection

## Required Results

- Validate extern enum/member decorators in parser/compiler, not LSP.
- Enforce the canonical fixed-width integer underlying domain and integer
  member-value shape without name/text fallback.
- Preserve stable descriptor `2019`, exact full-decorator range, canonical
  text, and explicit user-decision no-fix disposition.
- Publish a persistent semantic query diagnostic and preserve every field in
  LSP and stdio projection.
- Remove both LSP enum/member rule walkers and reject parallel
  `compiler_error` projection.

## TDD Evidence

The initial parser target failed to link because the public canonical enum
validator did not exist. The existing compiler and LSP paths each owned only a
partial rule table. The new parser tests froze valid `u32`/integer cases and
invalid underlying shape/value, member value shape, and unknown decorator
cases before the LSP producer was removed.

## Final Evidence

On fixed code HEAD `e94252f1978e22cd5c03758be5cd0af3b8095c88` plus the
eleven-path overlay, GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 each passed
the same twelve direct checks. Shared Unity totals were `5/5`, `63/63`,
`15/15`, `30/30`, `74/74`, `124/124`, `127/127`, and native extern `30/30`
except one MSVC Unix-only ignore. LSP suites reported 16 query-diagnostic, 63
analyzer, and 51 source-contract pass markers; dedicated stdio exited zero on
every toolchain.

All eleven code/test paths matched between the workspace, WSL snapshot, and
MSVC snapshot. GCC and Clang were clean rebuilt; MSVC used
`VSCMD_VER=17.14.38`. No accepted Unity summary contained a failure.

## Acceptance Decision

Accepted as the extern enum/member decorator slice of Plan 03 Task 6.
Parser/compiler is the only producer for enum/member decorator validity, and
LSP is a canonical compiler-query projection. Remaining decorator families
stay explicit future slices.

## 状态与产出记录

- 完成时间：2026-08-27 22:59 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC 同基线 12 项验收；Task 6
  继续进行。
- 完成项目：parser link RED、canonical enum/member value contract、
  descriptor/range/text/no-fix contract、public parser validator、persistent
  query fact、normal compiler delegation、analyzer producer 删除、golden parity、
  source contract、独立 stdio、11-path byte audit 与真实退出证据。
