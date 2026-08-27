---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_extern_parameter_decorators.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_extern_parameter_decorator_query_diagnostics.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/ffi/test_native_extern_contract.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/stdio_extern_parameter_decorator_diagnostic_smoke.js
  - tests/acceptance/2026-08-28-plan03-task06-extern-parameter-decorator-query-projection.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6.29: Extern Parameter Decorator Query Projection

## Scope

This submilestone makes parser/compiler the only semantic producer for source
extern function and delegate parameter decorators. It freezes accepted
direction and charset directives, argument shapes, value domains,
cross-decorator constraints, stable diagnostic identity, exact ranges, and
explicit no-fix disposition. LSP only invokes the public parser validator and
projects its persistent diagnostic fact.

The boundary is extern parameter decorators. Ordinary function parameters
must remain outside this validator. Variadic declaration storage continues to
use the compiler's declaration path; this slice does not redesign parser
syntax or AST ownership for variadic parameters.

## TDD And Root Cause

The parser RED target first failed to link because no public parameter
validator existed. LSP also contained a separate `zr.ffi` AST path extractor,
direction counter, and diagnostic producer. Its accepted surface diverged from
normal compilation: valid charset directives were rejected, invalid charset
values did not publish canonical query facts, and conflicting directions used
the parameter range instead of the second conflicting decorator range.

Seven parser cases freeze valid `in/out/inout` and charset forms, invalid
direction arguments, bare or malformed charset calls, unsupported charset
values, conflicts, and unknown decorators. A separate RED proved that calling
the validator for every `ZR_AST_PARAMETER` incorrectly rejected ordinary
custom parameter decorators; production now gates the call on the structured
extern function/delegate compiler context.

## Implementation

`ZrParser_Compiler_ValidateExternParameterDecorators` is the public canonical
entry. The cohesive `compiler_extern_parameter_decorators.c` module validates
decorator AST identities and structured call arguments in source order.
Normal extern compilation delegates ordinary parameters to that API and
variadic decorator arrays to the same internal rule implementation.

Invalid input publishes descriptor `2019`, code `invalid_decorator`, error
severity, semantic category, exact full-decorator range, canonical
message/cause/suggestion, zero fixes, and
`REQUIRES_USER_DECISION`. The LSP analyzer's three local extraction,
validation, and diagnostic helpers were deleted. Extern function/delegate
parameter traversal establishes the compiler context, invokes the parser API,
and consumes the shared compiler diagnostic bridge. Golden tests compare
query and LSP fields; source-contract and stdio tests prevent local producer or
parallel `compiler_error` regressions.

## Verification

The fixed code baseline is HEAD
`39ceace26ca87b7845722ea4d0331b8d34e56e11` plus twelve byte-identical
code/test paths. GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 each passed the
same fifteen checks with real process exits:

- extern parameter decorator query diagnostics: `7/7`;
- FFI wrapper, extern enum, and extern struct decorator diagnostics:
  `9/9`, `5/5`, and `5/5`;
- compiler semantic-query diagnostics: `63/63`;
- semantic facts and semantic query: `15/15` and `30/30`;
- parser and type inference: `74/74` and `124/124`;
- compiler integration: `127/127`;
- native extern contract: GCC/Clang `30/30`, MSVC `29 pass / 1 Unix-only ignore`;
- LSP semantic-query diagnostics: 16 pass markers;
- semantic analyzer: 69 pass markers;
- LSP source contracts: 53 pass markers;
- dedicated stdio smoke: real exit zero.

GCC and Clang used the same fixed WSL snapshot. The MSVC source tree was
created directly from that snapshot and built as static Debug. Workspace to
WSL and workspace to MSVC comparisons both reported `12/12`. Tests ran
serially, including compiler integration, and every accepted runner preserved
its real exit. Full repository GREEN is not claimed by this focused Task 6
slice.

## 状态与产出记录

- 完成时间：2026-08-28 01:40 +08:00。
- 状态：已完成 extern parameter decorator 的 parser/compiler 单一生产、
  semantic query 与 LSP/stdIO 投影，并通过 GCC/Clang/MSVC 同一 fixed code
  baseline 的 15 项验收；Plan 03 Task 6 继续进行。
- 完成项目：public canonical validator、独立 parameter decorator module、
  direction shape/互斥规则、charset call shape/value domain、unknown fail-closed、
  descriptor `2019`、exact full-decorator range、persistent query fact、
  user-decision no-fix、normal compiler delegation、ordinary parameter context
  隔离、LSP 3 个重复 helper 删除、2 项 golden parity、source contract、独立
  stdio smoke、12-path 双快照 byte audit 与三工具链真实退出证据。
- 后续项目：继续 support-first 迁移剩余 analyzer-owned semantic rules；LSP
  不得按 decorator 名称、AST/source text、message 或本地规则表重建语义。
