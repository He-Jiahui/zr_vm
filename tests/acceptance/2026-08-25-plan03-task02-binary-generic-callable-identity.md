---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_export_generics.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_export_generics.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-25-plan03-task02-binary-generic-callable-identity.md
tests:
  - tests/parser/test_type_inference.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 2.3c Binary Generic Callable Identity

## Scope

Accept the lower artifact-to-parser producer path for imported generic free
functions. The scope includes typed export allocation, `.zro` write/read,
runtime materialization/free, importer projection, and canonical call facts.
It excludes LSP inference and all name-based recovery.

## Baseline

The original binary fixture could import `identity<T>`, but the typed export
carrier omitted generic rows. The importer therefore built a non-generic
member and explicit `identity<string>` failed with `Expected 'T' but found
'string'`.

## Required Results

- The source-produced binary imports `identity<T>` with one structured generic
  binder rather than a literal `T` parameter type.
- Inferred and explicit calls reuse one external declaration SymbolId while
  keeping distinct closed callable TypeIds.
- Target declaration ranges remain zero because the external artifact has no
  source declaration range contract in this slice.
- Query formatting uses the copied semantic fact, not LSP or importer text
  reconstruction.
- Parameter metadata is selected by typed-export `callableChildIndex`; a
  same-name or same-arity callable cannot be selected recursively.

## Evidence

The isolated MSVC static cache
`.codex/build-lsp-plan03-native-generic-msvc-r2` rebuilt the type-inference
target after the artifact schema change. Direct execution of
`zr_vm_type_inference_test.exe` returned process exit zero and the Unity log
reported `123 Tests 0 Failures 0 Ignored`, including
`Binary Import Generic Function Publishes Closed Contract`.

The same isolated cache rebuilt and directly executed adjacent producer
contracts with process exit zero: semantic-query symbols `19 Tests 0 Failures
0 Ignored`, semantic query `29 Tests 0 Failures 0 Ignored`, canonical consumers
`19 Tests 0 Failures 0 Ignored`, and compiler semantic-query diagnostics
`46 Tests 0 Failures 0 Ignored`.

## Acceptance Decision

Accepted for the Task 2.3c artifact/import generic callable producer contract.
This record does not accept the wider Plan03 Task2 visibility, relation,
call-graph, or LSP consumer gates.

## 状态与产出记录

- 完成时间：2026-08-25 22:57 +08:00。
- 状态：已完成并随本提交精确提交。
- 完成项目：binary generic source fixture、artifact/import producer path、
  external SymbolId/closed TypeId/display contract。
- 后续项目：binary/native visible facts、external origins/relations、LSP consumers。
