---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_callable_decorators.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/ffi/test_native_extern_contract.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_semantic_query.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_diagnostic_golden_parity_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_diagnostic_fix_smoke.js
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-27-plan03-task06-extern-callable-decorator-query-projection.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 6.25 Extern Callable Decorator Query Projection

## Required Results

- Validate extern function/delegate decorators in parser/compiler, not LSP.
- Preserve stable descriptor `2019`, exact full-decorator range, canonical
  text, and explicit user-decision no-fix disposition.
- Accept the documented `cdecl` spelling as canonical C ABI without weakening
  the remaining ABI value set.
- Publish a persistent semantic query diagnostic and preserve every field in
  LSP and stdio projection.
- Remove the LSP callable rule walker and forbid name/text/value-table
  reconstruction.

## TDD Evidence

The initial parser RED was `63 Tests / 1 Failure`: invalid callable decorator
arguments produced no structured compiler/query fact. LSP had a duplicate AST
walker and local allowed-callconv/charset tables. The first stdio run then
showed a cross-line decorator range, traced to post-consumption parser
locations. After fixing token-boundary capture, expanded registry coverage
reported `30 Tests / 1 Failure` until descriptor 2019 and the 66-entry count
were frozen explicitly.

## Final Evidence

On fixed code HEAD `9a9bb0b62f67b49b912d9b2e2468bb5cd725820c`
plus the fifteen-path overlay, GCC 11.4, Clang 14.0.0, and MSVC
19.44.35228.0 each passed the same eleven direct checks. Shared Unity totals
were `63/63`, `11/11`, `15/15`, `30/30`, `124/124`, `127/127`, and native
extern `30/30` except one MSVC Unix-only ignore. LSP suites reported 16 query
diagnostic, 59 analyzer, and 49 source-contract pass markers; stdio exited zero
on every toolchain.

All fifteen code/test paths matched between the workspace, WSL snapshot, and
MSVC snapshot. The final shared HEAD advance to `6b82f5d` changed only Syntax
status-record docs/Python verification files and had no overlap with tested
build inputs. MSVC used `VSCMD_VER=17.14.38`; no final log contained a failure
marker.

## Acceptance Decision

Accepted as the extern callable decorator slice of Plan 03 Task 6.
Parser/compiler is the only producer for function/delegate decorator validity,
and LSP is a canonical compiler-query projection. Non-callable decorator
families remain explicit future slices.

## 状态与产出记录

- 完成时间：2026-08-27 20:09 +08:00。
- 状态：本子项已完成并通过 GCC/Clang/MSVC 同基线 11 项验收；Task 6
  继续进行。
- 完成项目：两阶段 support-first RED、descriptor/range/text/no-fix contract、
  public parser validator、`cdecl` ABI convergence、persistent query fact、
  analyzer producer 删除、模块拆分、golden parity、source contract、stdio、
  15-path byte audit 与真实退出证据。
