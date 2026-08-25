---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_export_generics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_export_generics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_export_generics.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
plan_sources:
  - user: 2026-08-25 参照 docs/plans/lsp 优化语义推断能力
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_type_inference.c
  - tests/acceptance/2026-08-25-plan03-task02-binary-generic-callable-identity.md
doc_type: milestone-record
---

# Plan 03 Task 2.3c: Binary Generic Callable Identity

## Goal

Make a source-built `.zro` generic function export project to the same
canonical callable contract consumed by source and native inference. The
artifact must preserve the open declaration contract; `CallAt` then publishes
one external declaration SymbolId per imported member and one closed callable
TypeId per invocation. LSP consumers remain query-only.

## Implementation

- Patch 42 version-gates typed-export generic rows in the artifact wire format.
  Rows encode generic name, kind, variance, class/struct/new/owner constraints,
  ownership qualifier constraint, and constraint type names.
- Parser typed metadata copies the declaration's structured generic records
  into `SZrFunctionTypedExportSymbol`; the writer, IO reader, runtime copier,
  and function destructor retain the same ownership contract.
- Method-signature generic arity is emitted from the typed-export row count but
  is never treated as a substitute for the structured declaration rows.
- Binary importer restores only typed-export generic rows. It resolves callable
  parameter metadata by `callableChildIndex`, not by recursive member name and
  arity search. Missing or malformed rows leave the generic contract absent.
- The generic call fact producer consumes that imported canonical member. It
  does not create a binary-specific or LSP text/name fallback.

## Contract

```zr
// generic.zro source
fn identity<T>(value: T): T { return value; }

// importer source
let generic = import("generic");
var inferred = generic.identity(1);
var explicit = generic.identity<string>("text");
```

- Both calls resolve one valid external target SymbolId.
- The two closed callable TypeIds differ.
- Both target declaration ranges are the explicit zero external range.
- `FormatCall` returns `identity<T>(value: int): int` and
  `identity<T>(value: string): string` from the query fact.
- The free-call display deliberately matches the source free-function contract;
  `fn ` and `const fn ` prefixes remain receiver-effect displays only.

## Exclusions

This slice does not publish binary visible-symbol candidates, external origin
URIs, relation/call graph facts, property import contracts, or any LSP
consumer fallback. It does not treat the legacy module-init summary or a method
signature arity byte as a complete generic declaration contract.

## Verification

- RED: before generic rows were serialized and imported, the explicit call
  failed with `Expected 'T' but found 'string'`.
- GREEN: the isolated MSVC type-inference executable reports `123 Tests 0
  Failures 0 Ignored` after inferred and explicit binary calls share their
  external declaration identity and retain their closed types. Adjacent query,
  canonical-consumer, and compiler-diagnostic suites report 19/0, 29/0, and
  46/0 respectively, each with direct process exit zero.

## 状态与产出记录

- 完成时间：2026-08-25 22:57 +08:00。
- 状态：已完成并随本提交精确提交；不声明 Plan 03 Task 2 完成。
- 完成项目：artifact generic-row wire contract、typed export/import projection、
  `callableChildIndex` exact metadata binding、binary inferred/explicit generic
  call RED/GREEN 测试。
- 后续项目：binary/native visible facts、external origin/relation facts、call graph
  与 LSP consumer 迁移。
