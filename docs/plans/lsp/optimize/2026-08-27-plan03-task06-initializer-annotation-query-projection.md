---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_messages.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_semantic_query.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_diagnostic_golden_parity_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_source_contract_initializer_annotation_cases.h
  - tests/language_server/stdio_initializer_annotation_diagnostic_smoke.js
  - tests/acceptance/2026-08-27-plan03-task06-initializer-annotation-query-projection.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6.22: Initializer Annotation Query Projection

## Scope

This submilestone moves the untyped, uninitialized variable rule from the
language server into parser/compiler structured diagnostics. It covers normal
source compilation, analyzer symbol collection, semantic-query projection, and
native stdio transport while keeping exact-type inference failure as a
separate diagnostic boundary.

The fixed overlay contains fourteen code/test paths. Module, milestone, and
acceptance documentation add three paths to the final exact commit.

## TDD And Root Cause

The parser RED compiled `var missing;` without an error and published no
structured query fact. The analyzer remained green because both symbol
collection and typecheck contained a local
`initializer_requires_annotation` diagnostic builder. That duplicated policy
and did not preserve descriptor identity, no-fix disposition, or compiler/LSP
field parity.

The first expanded GCC run exposed a second support-layer gap after descriptor
`2017` was added: the registry-count test still expected 63 entries and the
message table had 126 rows for 64 descriptors. The public registry contract
and canonical title/message pair were updated before broader consumers were
accepted.

## Implementation

`ZrParser_Compiler_ValidateVariableDeclaration` now owns the declaration rule.
When both explicit type and initializer are absent, it publishes descriptor
`2017`, error severity, the exact pattern range, canonical message, cause and
suggestion, no fixes, and `REQUIRES_USER_DECISION`. Statement compilation and
LSP symbol collection call this API; the analyzer consumes the compiler error
and later projects the persistent query fact.

The two local LSP annotation builders are removed. Failed exact inference for
an existing initializer now remains `cannot_infer_exact_type`, so this slice
does not broaden the annotation rule. Source-contract coverage forbids the
annotation code literal in analyzer production, and same-snapshot golden parity
compares every compiler-query field with the projected analyzer diagnostic. A
dedicated stdio smoke freezes the serialized range, text, descriptor, help URI,
and no-fix contract.

## Verification

The final source baseline is HEAD
`fb737be114a70f7b5fc9703e3163100a2d2414fe` plus fourteen byte-identical
code/test paths. SHA-256 auditing matched all fourteen files across the
workspace, GCC snapshot, Clang snapshot, and MSVC snapshot before execution.

GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0
(`VSCMD_VER=17.14.38`) each directly passed the same eleven checks with real
process exits:

- compiler semantic-query diagnostics: `58/58`;
- semantic-query diagnostic disposition: `11/11`;
- semantic facts: `15/15`;
- semantic query and registry/message coverage: `30/30`;
- type inference: `124/124`;
- compiler integration: `127/127`;
- LSP semantic-query diagnostics: 15 pass markers;
- semantic analyzer: 56 pass markers, including annotation golden parity;
- LSP source contracts: 47 pass markers from each fixed source snapshot;
- union-pattern diagnostics: 12 pass markers;
- dedicated initializer-annotation stdio smoke: real exit zero.

All run logs contained zero `Fail -` markers. The stdio logs were empty because
the protocol assertions completed without output. Full repository GREEN is not
claimed by this focused Task 6 slice.

## 状态与产出记录

- 完成时间：2026-08-27 14:41 +08:00。
- 状态：已完成 initializer annotation 的 parser/compiler structured
  diagnostic、semantic query 与 LSP/stdIO 单一投影，并通过 GCC/Clang/MSVC
  同基线验收；Plan 03 Task 6 继续进行。
- 完成项目：descriptor `2017` 单一生产、精确 declaration-pattern range、
  canonical message/cause/suggestion、explicit no-fix、compiler/LSP 全字段
  golden parity、analyzer producer 删除、source contract、独立 stdio smoke、
  registry/message completeness 与三工具链 11 项证据。
- 后续项目：继续 support-first 迁移 `cannot_infer_exact_type` 等仍由 analyzer
  直接生产的语义诊断；不得把 initializer inference failure 并入本规则，
  也不得在 LSP 按名称、AST 或消息重建。
