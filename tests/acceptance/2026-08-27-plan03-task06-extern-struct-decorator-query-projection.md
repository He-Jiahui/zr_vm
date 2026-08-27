---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_struct_decorators.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_extern_struct_decorator_query_diagnostics.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/ffi/test_native_extern_contract.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_extern_struct_decorator_diagnostic_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-27-plan03-task06-extern-struct-decorator-query-projection.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.26 Extern Struct Decorator Query Projection

## Required Results

- Validate extern struct/field decorators in parser/compiler, not LSP.
- Enforce canonical structured values for kind, packing, alignment, offset,
  and charset without name/text fallback.
- Preserve stable descriptor `2019`, exact full-decorator range, canonical
  text, and explicit user-decision no-fix disposition.
- Publish a persistent semantic query diagnostic and preserve every field in
  LSP and stdio projection.
- Remove both LSP struct/field rule walkers and reject parallel
  `compiler_error` projection.

## TDD Evidence

The initial parser target failed to link because the public canonical
validator did not exist. Its first implementation produced `5 Tests / 2
Failures`: both field cases started after the opening `#`. The root cause was a
partial hand-written lexer snapshot in struct member lookahead. The repository
parser cursor fixed the range. LSP inspection then confirmed two duplicate
AST walkers with incomplete integer-shape rules; these were removed after the
compiler/query producer was green.

## Final Evidence

On fixed code HEAD `7736d125d232d4630bdb07e1c615afa2f56c43a8` plus the
fifteen-path overlay, GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 each
passed the same twelve direct checks. Shared Unity totals were `5/5`, `63/63`,
`15/15`, `30/30`, `74/74`, `124/124`, `127/127`, and native extern `30/30`
except one MSVC Unix-only ignore. LSP suites reported 16 query-diagnostic, 61
analyzer, and 50 source-contract pass markers; dedicated stdio exited zero on
every toolchain.

All fifteen code/test paths matched between the workspace, WSL snapshot, and
MSVC snapshot. MSVC used `VSCMD_VER=17.14.38`; no accepted Unity summary
contained a failure.

## Acceptance Decision

Accepted as the extern struct/field decorator slice of Plan 03 Task 6.
Parser/compiler is the only producer for struct/field decorator validity, and
LSP is a canonical compiler-query projection. Remaining decorator families
stay explicit future slices.

## 状态与产出记录

- 完成时间：2026-08-27 21:20 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC 同基线 12 项验收；Task 6
  继续进行。
- 完成项目：两阶段 support-first RED、canonical struct/field value contract、
  descriptor/range/text/no-fix contract、public parser validator、persistent
  query fact、normal compiler delegation、analyzer producer 删除、golden parity、
  source contract、独立 stdio、15-path byte audit 与真实退出证据。
