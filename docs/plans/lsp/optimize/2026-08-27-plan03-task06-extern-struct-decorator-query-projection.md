---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_decorator_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_callable_decorators.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_struct_decorators.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_extern_struct_decorator_query_diagnostics.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/ffi/test_native_extern_contract.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_extern_struct_decorator_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_extern_struct_decorator_diagnostic_smoke.js
  - tests/acceptance/2026-08-27-plan03-task06-extern-struct-decorator-query-projection.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6.26: Extern Struct Decorator Query Projection

## Scope

This submilestone moves extern struct and field decorator validation from the
LSP analyzer into parser/compiler structured diagnostics. Parser owns the
accepted declaration/field decorator sets, argument shapes and values, stable
descriptor, exact full-decorator range, canonical text, and no-fix
disposition. LSP consumes the compiler diagnostic and projects its persistent
semantic query fact.

The boundary is struct/field-only. Extern enum/member, wrapper class,
parameter, and remaining analyzer-owned semantic producers stay separate
support-first slices. This change does not authorize an LSP name, AST, source
text, message, or value-table fallback for those rules.

## TDD And Root Cause

The parser RED target could not link because no public canonical extern
struct validator existed. After introducing the validator, five focused tests
ran with two failures: field decorator locations began one byte after the
opening `#`. Struct member lookahead manually restored lexer fields but omitted
token-start offsets and lines. Replacing that partial state copy with the
existing complete parser cursor snapshot fixed the canonical range.

The LSP analyzer still had two independent walkers. They recognized only
`pack`/`align` and `offset`, checked a single integer shape, and did not enforce
canonical value domains. The parser/compiler declaration path likewise had
only name/call-shape tables. Both gaps allowed invalid values such as
`align(3)` and `charset("wide")` to diverge across compiler and editor
consumers.

## Implementation

`ZrParser_Compiler_ValidateExternStructDecorators` is the public canonical
validator. It accepts only struct declaration AST identity and validates:

- declaration `kind("struct"|"union")`;
- declaration `pack`/`align` as positive `uint32` powers of two;
- field `offset` as a nonnegative `uint32` integer;
- field `charset("utf8"|"utf16"|"ansi")`.

Invalid values publish descriptor `2019`, code `invalid_decorator`, error
severity, semantic category, exact full-decorator range, canonical
message/cause/suggestion, zero fixes, and `REQUIRES_USER_DECISION`. Callable
and struct validators now share a cohesive diagnostic reporter while keeping
their rule tables separate. Normal compilation delegates struct validation to
the public API. The analyzer invokes the same API while traversing an extern
block and consumes the compiler error through the shared diagnostic bridge;
its struct and field producers were deleted.

Golden parity compares every query/LSP structured field from one semantic
snapshot. Source contracts forbid restoring either local producer. A dedicated
stdio smoke verifies descriptor, exact UTF-16 field-decorator range, code
description, no-fix reason, empty fixes, and absence of a parallel
`compiler_error`.

## Verification

The fixed code baseline is HEAD
`7736d125d232d4630bdb07e1c615afa2f56c43a8` plus fifteen byte-identical
code/test paths. GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0
(`VSCMD_VER=17.14.38`) each directly passed the same twelve checks with real
process exits:

- extern struct decorator query diagnostics: `5/5`;
- compiler semantic-query diagnostics, including callable regression: `63/63`;
- semantic facts: `15/15`;
- semantic query: `30/30`;
- parser: `74/74`;
- type inference: `124/124`;
- compiler integration: `127/127`;
- native extern contract: GCC/Clang `30/30`, MSVC `29 pass / 1 Unix-only ignore`;
- LSP semantic-query diagnostics: 16 pass markers;
- semantic analyzer: 61 pass markers, including both new golden cases;
- LSP source contracts: 50 pass markers;
- dedicated stdio smoke: real exit zero.

Workspace-to-WSL and workspace-to-MSVC SHA-256 comparisons both reported
`15/15`. Test processes were the final commands or used fail-fast wrappers
that preserved nonzero exits; no final Unity summary contained failures. Full
repository GREEN is not claimed by this focused Task 6 slice.

## 状态与产出记录

- 完成时间：2026-08-27 21:20 +08:00。
- 状态：已完成 extern struct/field decorator 的 parser/compiler 单一生产、
  semantic query 与 LSP/stdIO 投影，并通过 GCC/Clang/MSVC 同一 fixed code
  baseline 的 12 项验收；Plan 03 Task 6 继续进行。
- 完成项目：parser link RED、field range support RED、public canonical
  validator、`kind`/`pack`/`align`/`offset`/`charset` 结构化值域、descriptor
  `2019`、persistent query fact、user-decision no-fix、共享 decorator
  diagnostic reporter、normal compiler delegation、LSP struct/field producer
  删除、完整 parser cursor range、2 项 golden parity、source contract、独立
  stdio smoke、15-path byte audit 与三工具链真实退出证据。
- 后续项目：继续 support-first 迁移 extern enum/member、wrapper class、parameter
  及其他 analyzer-owned semantic rules；LSP 不得按 decorator 名称、AST/source
  text、message 或本地 value table 重建规则。
