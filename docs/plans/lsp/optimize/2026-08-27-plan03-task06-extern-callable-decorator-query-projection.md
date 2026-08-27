---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_callable_decorators.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_ffi_contract.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_messages.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
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
  - tests/acceptance/2026-08-27-plan03-task06-extern-callable-decorator-query-projection.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6.25: Extern Callable Decorator Query Projection

## Scope

This submilestone moves extern function/delegate decorator validation from an
LSP-owned AST validator into parser/compiler structured diagnostics. Parser
owns the accepted callable decorator set, argument shapes and values, stable
descriptor, exact decorator range, canonical text, and no-fix disposition.
LSP consumes the compiler diagnostic and projects its persistent semantic
query fact.

The boundary is callable-only. Extern struct/field, enum/member, wrapper class,
and parameter decorator producers remain separate support-first slices. This
change does not authorize an LSP fallback for any of those rules.

## TDD And Root Cause

The parser RED reported `63 Tests / 1 Failure`: an invalid
`#zr.ffi.callconv(123)#` did not set a structured compiler error or publish a
persistent query diagnostic. Existing 62 tests remained green. LSP still
passed because `semantic_analyzer_typecheck.c` contained a second callable
decorator walker, local allowed-value tables, and a direct
`invalid_decorator` producer.

The first stdio projection exposed a second support defect: decorator AST
ranges were built from the parser position after token consumption, producing
a cross-line range. Capturing opening and closing `#` token locations fixed
the canonical source range for every consumer. Expanded semantic-query
coverage then reported `30 Tests / 1 Failure` because the stable registry
count still expected 65 descriptors; the test now freezes descriptor 2019 and
the 66-entry registry.

## Implementation

`ZrParser_Compiler_ValidateExternCallableDecorators` is the public canonical
validator. It accepts only extern function/delegate AST declarations and
validates structured `zr.ffi` decorator paths, call shapes, string value sets,
and nonnegative required-capability values. Both `c` and `cdecl` map to the
canonical C ABI.

Invalid values publish descriptor `2019`, code `invalid_decorator`, error
severity, semantic category, the exact full decorator range, canonical
message/cause/suggestion, zero fixes, and
`REQUIRES_USER_DECISION`. Normal compilation reuses the public validator. The
analyzer calls it and consumes the compiler error through the shared
diagnostic bridge; its callable walker and allowed-callconv table were
removed.

Callable validation lives in the cohesive 231-line
`compiler_extern_callable_decorators.c` module. The declaration compiler is
reduced to about 928 lines and retains extern declaration orchestration plus
the still-separate non-callable rules.

Golden parity compares every query/LSP structured field from one semantic
snapshot. Source contracts forbid restoring the LSP callable producer, and
stdio verifies descriptor, exact UTF-16 range, code description, no-fix
reason, and empty fixes.

## Verification

The fixed code baseline is HEAD
`9a9bb0b62f67b49b912d9b2e2468bb5cd725820c` plus fifteen byte-identical
code/test paths. During validation `main` advanced to
`6b82f5d57718e2db1a33a9eae4403243702379a3` through Syntax status-record
documentation and Python verification paths only; an overlap audit found no
C/CMake or Task 6.25 path changes. The final commit therefore applies the same
tested bytes on the newer HEAD.

GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0
(`VSCMD_VER=17.14.38`) each directly passed the same eleven checks with real
process exits:

- compiler semantic-query diagnostics: `63/63`;
- semantic-query diagnostic disposition: `11/11`;
- semantic facts: `15/15`;
- semantic query plus registry/message coverage: `30/30`;
- type inference: `124/124`;
- compiler integration: `127/127`;
- native extern contract: GCC/Clang `30/30`, MSVC `29 pass / 1 Unix-only ignore`;
- LSP semantic-query diagnostics: 16 pass markers;
- semantic analyzer: 59 pass markers;
- fixed-snapshot LSP source contracts: 49 pass markers;
- stdio diagnostic/fix smoke: real exit zero.

Workspace-to-WSL and workspace-to-MSVC SHA-256/byte comparisons both reported
`15/15`. Every runner stopped on the first nonzero process exit; MSVC logs
contained no `:FAIL` or `Fail -` marker. Full repository GREEN is not claimed
by this focused Task 6 slice.

## 状态与产出记录

- 完成时间：2026-08-27 20:09 +08:00。
- 状态：已完成 extern function/delegate callable decorator 的 parser/compiler
  单一生产、semantic query 与 LSP/stdIO 投影，并通过 GCC/Clang/MSVC 同一
  fixed code baseline 的 11 项验收；Plan 03 Task 6 继续进行。
- 完成项目：parser RED、精确 decorator range support RED、descriptor `2019`、
  66-entry registry contract、canonical argument/value validation、`c`/`cdecl`
  ABI convergence、persistent query fact、user-decision no-fix、analyzer callable
  producer/allowed table 删除、231-line 模块拆分、golden parity、source contract、
  stdio smoke、15-path byte audit 与真实退出证据。
- 后续项目：继续 support-first 迁移 extern struct/field、enum/member、wrapper
  class、parameter 及其他 analyzer-owned semantic rules；LSP 不得按 decorator
  名称、AST/source text、message 或本地 value table 重建规则。
