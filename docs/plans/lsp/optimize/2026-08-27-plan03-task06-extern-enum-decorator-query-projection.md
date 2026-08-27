---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_enum_decorators.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_declaration.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_extern_enum_decorator_query_diagnostics.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/ffi/test_native_extern_contract.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_extern_enum_decorator_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_extern_enum_decorator_diagnostic_smoke.js
  - tests/acceptance/2026-08-27-plan03-task06-extern-enum-decorator-query-projection.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6.27: Extern Enum Decorator Query Projection

## Scope

This submilestone moves extern enum declaration and member decorator
validation from the LSP analyzer into parser/compiler structured diagnostics.
Parser owns the accepted decorator set, argument shapes and values, stable
descriptor, exact full-decorator range, canonical text, and no-fix
disposition. LSP only consumes the compiler diagnostic and projects its
persistent semantic query fact.

The boundary is enum/member-only. Wrapper class, parameter, and remaining
analyzer-owned semantic producers stay separate support-first slices. This
change does not authorize an LSP decorator-name, AST, source-text, message, or
local value-table fallback.

## TDD And Root Cause

The parser RED target failed to link because no public canonical extern enum
validator existed. Repository inspection then showed that normal compilation
only checked shallow enum/member decorator names and call shapes, while the
LSP analyzer independently checked one string or integer argument. Neither
consumer owned one canonical underlying-value contract.

The runtime FFI primitive contract already accepts the fixed-width integer
family. Task 6.27 freezes that structured source contract at the parser layer:
enum `underlying` accepts exactly one string literal in `i8`, `u8`, `i16`,
`u16`, `i32`, `u32`, `i64`, or `u64`; member `value` accepts exactly one
integer literal. Unknown decorators, invalid shapes, and invalid values fail
closed.

## Implementation

`ZrParser_Compiler_ValidateExternEnumDecorators` is the public canonical
validator. It validates the enum declaration and every enum member from AST
identity and publishes descriptor `2019`, code `invalid_decorator`, error
severity, semantic category, exact full-decorator range, canonical
message/cause/suggestion, zero fixes, and `REQUIRES_USER_DECISION`.

Normal compilation delegates enum validation to this API. The LSP analyzer
invokes the same API while traversing an extern block and consumes the compiler
error through the shared diagnostic bridge. Its enum/member walkers and local
integer-shape helper were deleted. Golden parity compares every query/LSP
structured field for invalid enum and member cases. A source contract prevents
restoring the local producer, and a dedicated stdio smoke verifies descriptor,
UTF-16 range, code description, no-fix data, empty fixes, and absence of a
parallel `compiler_error`.

## Verification

The fixed code baseline is HEAD
`e94252f1978e22cd5c03758be5cd0af3b8095c88` plus eleven byte-identical
code/test paths. GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0
(`VSCMD_VER=17.14.38`) each directly passed the same twelve checks with real
process exits:

- extern enum decorator query diagnostics: `5/5`;
- compiler semantic-query diagnostics: `63/63`;
- semantic facts: `15/15`;
- semantic query: `30/30`;
- parser: `74/74`;
- type inference: `124/124`;
- compiler integration: `127/127`;
- native extern contract: GCC/Clang `30/30`, MSVC `29 pass / 1 Unix-only ignore`;
- LSP semantic-query diagnostics: 16 pass markers;
- semantic analyzer: 63 pass markers, including both new golden cases;
- LSP source contracts: 51 pass markers;
- dedicated stdio smoke: real exit zero.

GCC and Clang used clean rebuilds; MSVC used a fresh static Debug build.
Workspace-to-WSL and workspace-to-MSVC byte comparisons both reported
`11/11`. Test processes were the final commands or used fail-fast wrappers
that preserved nonzero exits. Full repository GREEN is not claimed by this
focused Task 6 slice.

## 状态与产出记录

- 完成时间：2026-08-27 22:59 +08:00。
- 状态：已完成 extern enum/member decorator 的 parser/compiler 单一生产、
  semantic query 与 LSP/stdIO 投影，并通过 GCC/Clang/MSVC 同一 fixed code
  baseline 的 12 项验收；Plan 03 Task 6 继续进行。
- 完成项目：public canonical validator、`underlying` 固定宽度整数值域、
  `value` integer literal 形状、descriptor `2019`、exact full-decorator range、
  persistent query fact、user-decision no-fix、normal compiler delegation、LSP
  enum/member producer 删除、2 项 golden parity、source contract、独立 stdio
  smoke、11-path byte audit 与三工具链真实退出证据。
- 后续项目：继续 support-first 迁移 wrapper class、parameter 及其他
  analyzer-owned semantic rules；LSP 不得按 decorator 名称、AST/source text、
  message 或本地 value table 重建规则。
