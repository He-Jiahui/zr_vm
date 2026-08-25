---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
  - tests/parser/test_semantic_query_symbols.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
plan_sources:
  - user: 2026-08-25 参照 docs/plans/lsp 优化语义推断能力
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_symbols.c
  - tests/parser/test_semantic_query.c
  - tests/parser/test_semantic_query_contract.c
  - tests/parser/test_canonical_consumers.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/acceptance/2026-08-25-plan03-task02-native-generic-receiver-identity.md
doc_type: milestone-record
---

# Plan 03 Task 2.3b: Native Generic Receiver Identity

## Goal

Publish one snapshot-scoped declaration identity for a resolved native generic
receiver member while preserving the per-call closed canonical callable type.
The producer must consume the resolved `SZrTypeMemberInfo`; LSP consumers must
not pair calls or recover identities from member text.

## Implementation

- An external generic member no longer fails closed merely because it has no
  source declaration AST.
- The first resolved call registers a function `SymbolId` with an invalid
  declaration `TypeId`. This deliberately represents the external open
  declaration without assigning it the first closed instantiation.
- `SZrTypeMemberInfo.symbolId` caches that identity for the semantic-context
  lifetime. Inferred and explicit calls reuse it.
- Each `CALL` reference fact still receives its own resolved closed callable
  `TypeId`, signature display, and explicit zero declaration range. The
  declaration has no source AST, so a call range is never repurposed as a
  declaration range.

## Contract

For a native descriptor member `NativeEchoDevice.echo<T>(value: T): T`:

```zr
var api = import("semantic.generic_identity");
var device = new api.NativeEchoDevice();
var inferred = device.echo(1);
var explicit = device.echo<string>("text");
```

- `SymbolAt` at either `echo` token returns the same valid `SymbolId`.
- The two returned callable `TypeId` values are different closed instances.
- `CallAt` reports the same resolved target identity at each call-site.
- `FormatCall` returns `fn echo<T>(value: int): int` for the inferred call and
  `fn echo<T>(value: string): string` for the explicit call.
- Both target declaration ranges are the zero external range.

## Exclusions

This submilestone does not add binary `.zro` generic callable metadata,
external-origin URIs, native visible-symbol projection, relation facts, or any
LSP fallback. Binary generic parity remains a separate Task 2 producer slice.

## Verification

- RED: before the producer change, the new native generic fixture published a
  `CALL` fact with `isResolved == false` and invalid `SymbolId`.
- GREEN: MSVC static direct executables passed symbols 19/19, semantic query
  29/29, query contract 3/3, canonical consumers 19/19, and compiler
  semantic-query diagnostics 46/46 with process exit zero.
- A compiler-integration command overlapped with an orphaned prior invocation
  and is intentionally excluded from this record rather than treated as a
  passing regression result.
- GCC remains blocked before parser compilation by the existing Windows GCC
  4.8.3 `_Thread_local` incompatibility; no GCC result is claimed. No local
  Clang executable or WSL distribution is available; no Clang result is
  claimed.

## 状态与产出记录

- 完成时间：2026-08-25 21:28 +08:00。
- 状态：已完成并精确提交。native-generic receiver producer 的 MSVC focused
  验收完成；binary generic metadata parity 和跨工具链可执行验证未完成，不声明
  Task 2 完成。
- 完成项目：native generic receiver `SymbolId` producer、closed callable
  `TypeId` separation、SymbolAt/CallAt/FormatCall contract、RED/GREEN。
- 后续项目：binary generic declaration metadata、native/binary visible facts、
  external origin/relation facts，以及 Task 2 LSP consumer migration。
