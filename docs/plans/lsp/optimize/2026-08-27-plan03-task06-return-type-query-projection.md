---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_callable_return_inference.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_messages.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_semantic_query.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_semantic_analyzer_diagnostic_golden_parity_cases.h
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_source_contract_return_type_cases.h
  - tests/language_server/stdio_return_type_diagnostic_smoke.js
  - tests/acceptance/2026-08-27-plan03-task06-return-type-query-projection.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 6.23: Return Type Query Projection

## Scope

This submilestone moves incompatible unannotated callable return inference from
the language server into parser/compiler structured diagnostics. Parser owns
the inference decision, stable descriptor, exact callable and return ranges,
related information, and no-fix disposition. LSP consumes the public compiler
query and projects its persistent semantic diagnostic fact.

The boundary is intentionally narrow. Two exact return types that cannot form
an exact common type produce `return_type_not_provable`. A return that is still
weak or unavailable during early metadata construction remains weak metadata;
the public LSP query returns unavailable and the existing
`cannot_infer_exact_type` boundary remains separate.

## TDD And Root Cause

The first parser RED reported `60 Tests / 1 Failure`: compiling exact `int` and
`string` returns neither failed nor published a query fact. At the same time,
the analyzer passed its old test because it contained its own AST return walk,
common-type merge, and direct diagnostic producer.

After moving the rule into the shared compiler collector, expanded compiler
integration exposed nine regressions. Parameter, capture, nested-function, and
lambda metadata can legitimately be weak before all bindings are refined, so
treating a single weak result as a semantic contradiction was too broad. A
second parser RED (`61 Tests / 1 Failure`) froze this support contract. The
final implementation diagnoses only incompatible exact return pairs and keeps
weak metadata available to later refinement.

## Implementation

`ZrParser_Compiler_InferCallableReturnType` is the public parser/compiler
consumer boundary. It accepts function, method, meta-function, or lambda AST
identity and returns an exact inferred type only when one is available. Exact
conflicts publish descriptor `2018`, code `return_type_not_provable`, error
severity, the callable-name range, both return-expression ranges, canonical
message/cause/suggestion text, no fixes, and
`REQUIRES_USER_DECISION`.

The callable return collector and diagnostic producer live in
`compiler_callable_return_inference.c`. This extraction reduces
`compiler_bindings.c` to about 806 lines and leaves it responsible for binding
orchestration. Internal metadata callers may retain weak return metadata;
public exact consumers fail closed.

The analyzer no longer contains its own return collector, merge function, or
`return_type_not_provable` literal. It registers the callable parameter scope,
calls the parser API, consumes the compiler diagnostic, and projects the
persistent query fact. Golden parity compares all structured fields from one
semantic snapshot, source-contract coverage freezes ownership, and dedicated
stdio coverage verifies protocol serialization.

## Verification

The final fixed source baseline is HEAD
`d6ee3fed1502699113d246f5efad0c81de4f5cb9` plus thirteen byte-identical
code/test paths. SHA-256 comparison reported zero mismatches between the
workspace and GCC, Clang, and MSVC snapshots.

GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0
(`VSCMD_VER=17.14.38`) each directly passed the same eleven checks with real
process exits:

- compiler semantic-query diagnostics: `61/61`;
- semantic-query diagnostic disposition: `11/11`;
- semantic facts: `15/15`;
- semantic query and registry/message coverage: `30/30`;
- type inference: `124/124`;
- compiler integration: `127/127`;
- LSP semantic-query diagnostics: 15 pass markers;
- semantic analyzer: 57 pass markers;
- fixed-snapshot LSP source contracts: 48 pass markers;
- union-pattern diagnostics: 12 pass markers;
- dedicated return-type stdio smoke: real exit zero.

MSVC build/run logs contained zero `:FAIL` or `Fail -` markers. GCC and Clang
reported the same test totals and marker counts. Full repository GREEN is not
claimed by this focused Task 6 slice.

## 状态与产出记录

- 完成时间：2026-08-27 16:51 +08:00。
- 状态：已完成 `return_type_not_provable` 的 parser/compiler 单一生产、
  semantic query 与 LSP/stdIO 投影，并通过 GCC/Clang/MSVC 同基线 11 项验收；
  Plan 03 Task 6 继续进行。
- 完成项目：exact-return conflict RED、weak/unavailable support RED、descriptor
  `2018`、精确 callable/return ranges、两条 related information、explicit
  no-fix、analyzer collector/producer 删除、公共 exact query、模块拆分、
  compiler/LSP golden parity、source contract、独立 stdio smoke、13-path
  SHA-256 与真实退出证据。
- 后续项目：继续 support-first 迁移剩余 analyzer-owned semantic rules；不得把
  weak/unavailable return 当成 exact conflict，也不得在 LSP 按 AST、名称、
  source text 或 display text 重建 return inference。
