---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_ffi_wrapper_decorators.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_ffi_wrapper_decorator_query_diagnostics.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/ffi/test_native_extern_contract.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_ffi_wrapper_decorator_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_ffi_wrapper_decorator_diagnostic_smoke.js
  - tests/acceptance/2026-08-27-plan03-task06-ffi-wrapper-decorator-query-projection.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6.28: FFI Wrapper Decorator Query Projection

## Scope

This submilestone moves source class FFI wrapper decorator validation from the
LSP analyzer into one parser/compiler contract. Parser owns accepted decorator
identities, argument shapes, value domains, cross-decorator constraints,
stable diagnostic identity, exact range, canonical text, and no-fix
disposition. Normal compilation and LSP consume the same validator.

The boundary is wrapper class decorators only. Parameter decorators and other
remaining analyzer-owned semantic producers stay separate support-first
slices. LSP must not reconstruct this contract from decorator names, AST,
source text, diagnostic messages, or a local value table.

## TDD And Root Cause

The initial parser RED target failed to link because no public wrapper
validator existed. Inspection showed that `compiler_class.c` and the LSP
analyzer each carried their own wrapper extraction and validation tables. That
duplication allowed metadata compilation and editor diagnostics to diverge.

The canonical parser tests freeze all wrapper rules: `lowering` accepts
`value`, `pointer`, or `handle_id`; `viewType` must name a source extern struct
in the same script; `underlying` accepts the fixed-width integer family;
`ownerMode` accepts `borrowed` or `owned`; `releaseHook` is one string literal;
unknown `zr.ffi` decorators fail closed. `underlying` is legal only with
`handle_id`, and `handle_id` requires `underlying`.

## Implementation

`ZrParser_Compiler_ValidateFfiWrapperDecorators` is the public canonical
validator. The cohesive `compiler_ffi_wrapper_decorators.c` module extracts a
structured `SZrFfiWrapperDecoratorContract`, validates declarations in source
order, and publishes descriptor `2019`, code `invalid_decorator`, error
severity, semantic category, exact full-decorator range, canonical
message/cause/suggestion, zero fixes, and `REQUIRES_USER_DECISION`.

`compiler_class.c` delegates to the same binder before projecting wrapper
metadata. The LSP analyzer invokes the public validator and consumes the error
through the shared compiler-diagnostic bridge. Its class-wrapper validator,
integer table, view lookup, call-shape helper, and text-set helper were removed.
Two golden cases compare every query/LSP field. A source contract prevents the
deleted producer from returning, and dedicated stdio coverage verifies exact
range, descriptor, code description, no-fix data, empty fixes, and absence of
a parallel `compiler_error`.

## Verification

The fixed code baseline is HEAD
`94c78937266ed4c7ec530e948d08013d04da5240` plus twelve byte-identical
code/test paths. GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0
(`VSCMD_VER=17.14.38`) each directly passed the same thirteen checks with real
process exits:

- FFI wrapper decorator query diagnostics: `9/9`;
- extern enum decorator query diagnostics: `5/5`;
- compiler semantic-query diagnostics: `63/63`;
- semantic facts: `15/15`;
- semantic query: `30/30`;
- parser: `74/74`;
- type inference: `124/124`;
- compiler integration: `127/127`;
- native extern contract: GCC/Clang `30/30`, MSVC `29 pass / 1 Unix-only ignore`;
- LSP semantic-query diagnostics: 16 pass markers;
- semantic analyzer: 65 pass markers, including both new golden cases;
- LSP source contracts: 52 pass markers;
- dedicated stdio smoke: real exit zero.

GCC and Clang used clean fixed snapshots; MSVC used a static Debug snapshot.
Workspace-to-WSL and workspace-to-MSVC byte comparisons both reported
`12/12`. Every accepted runner preserved its real process exit. Full repository
GREEN is not claimed by this focused Task 6 slice.

## 状态与产出记录

- 完成时间：2026-08-28 00:14 +08:00。
- 状态：已完成 FFI wrapper class decorator 的 parser/compiler 单一生产、
  semantic query 与 LSP/stdIO 投影，并通过 GCC/Clang/MSVC 同一 fixed code
  baseline 的 13 项验收；Plan 03 Task 6 继续进行。
- 完成项目：public canonical validator、结构化 wrapper contract、五类 decorator
  形状/值域、`handle_id`/`underlying` 组合约束、descriptor `2019`、exact
  full-decorator range、persistent query fact、user-decision no-fix、normal
  compiler delegation、LSP class-wrapper producer删除、2项 golden parity、source
  contract、独立 stdio smoke、12-path 双快照 byte audit 与三工具链真实退出证据。
- 后续项目：继续 support-first 迁移 parameter decorator 及其他
  analyzer-owned semantic rules；LSP 不得按 decorator 名称、AST/source text、
  message 或本地 value table 重建规则。
