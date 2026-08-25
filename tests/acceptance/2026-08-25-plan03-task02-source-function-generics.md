---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c
  - tests/parser/test_semantic_query_symbols.c
related_docs:
  - docs/plans/lsp/optimize/2026-08-25-plan03-task02-source-function-generics.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 2.2e Source Function Generic Scope Facts

## Scope

Validate compiler-published visible-symbol facts for source free-function type
generic parameters. The query must return only canonical identities and must
not reconstruct the generic parameter from function names, source ranges, or
AST nodes.

## Test Inventory

`test_visible_symbols_projects_source_function_generic_parameter` compiles:

```zr
fn identity<T>(value: T): T { return value; }
```

It verifies that `T` is absent at the `identity` declaration, visible at its
generic declaration, and carries a non-invalid canonical `SymbolId`, `TypeId`,
and exact function owner identity.

## Tooling Evidence

| Environment | Targets | Result |
| --- | --- | --- |
| GCC static | symbols, query, contract, compiler diagnostics | 9/9, 29/29, 3/3, 46/46; all exit 0 |
| MSVC static | symbols, query, contract, compiler diagnostics | 9/9, 29/29, 3/3, 46/46; all exit 0 |
| Clang 14 WSL static | changed parser/test source compilation | Compiled; executable link blocked by existing C11 inline unresolved references including `ZrCore_Memory_RawFreeWithType` |

## Acceptance Decision

Accepted for the GCC and MSVC source free-function generic producer
submilestone. The Clang executable gate is not claimed because the static-link
ABI failure is outside this child write set. Const/method generic parameters,
type members, imports/aliases, receiver members, binary/native parity, and LSP
consumer migration remain open.

## 状态与产出记录

- 完成时间：2026-08-25 13:32:38 +08:00
- 状态：GCC/MSVC acceptance passed；Clang executable gate 被既有 static link ABI
  失败阻断，未计入通过声明。
- 完成项目：source function generic canonical identity、function-scope
  projection、ancestor-first nested scope resolution、focused compiler-backed
  regression、Task 2 acceptance evidence。
- 后续项目：补齐剩余 Task 2 producer、source/binary/native parity 与 LSP consumer
  migration，再执行 Task 2 最终门禁。
