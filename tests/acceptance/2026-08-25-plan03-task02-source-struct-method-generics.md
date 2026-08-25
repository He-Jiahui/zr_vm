---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - tests/parser/test_semantic_query_symbols.c
related_docs:
  - docs/plans/lsp/optimize/2026-08-25-plan03-task02-source-struct-method-generics.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 2.2f Source Struct Method Generic Scope Facts

## Scope

Validate canonical visible-symbol facts for struct method type generics. The
method owner must come from the compiler-published member symbol and the query
must not reconstruct it from the declaration name or type-member lookup.

## Test Inventory

`test_visible_symbols_projects_source_method_generic_parameter` compiles:

```zr
struct Box<T> { fn echo<U>(value: U): U { return value; } }
```

It verifies that `U` is unavailable at `echo`, visible at its generic
declaration, has a canonical parameter `SymbolId` and `TypeId`, retains the
exact registered method owner id, and sees enclosing `T` through type-to-method
scope parentage.

## Tooling Evidence

| Environment | Targets | Result |
| --- | --- | --- |
| GCC static | symbols, query, contract, compiler diagnostics, compiler integration | 10/10, 29/29, 3/3, 46/46, 127/127; all zero failures |
| MSVC static | symbols, query, contract, compiler diagnostics, compiler integration | 10/10, 29/29, 3/3, 46/46, 127/127; all zero failures |
| Clang 14 WSL static | changed parser/test source compilation | Compiled; executable link blocked by existing C11 inline unresolved references including `ZrCore_Memory_RawFreeWithType` |

## Acceptance Decision

Accepted for the GCC and MSVC source struct-method generic producer
submilestone. The Clang executable gate is not claimed because the static-link
ABI failure is outside this child write set. Const generic parameters,
class/interface method generic producers, type members, imports/aliases,
receiver members, binary/native parity, and LSP consumer migration remain open.

## 状态与产出记录

- 完成时间：2026-08-25 13:52:15 +08:00
- 状态：GCC/MSVC acceptance passed；Clang executable gate 被既有 static link ABI
  失败阻断，未计入通过声明。
- 完成项目：struct method canonical owner、method generic scope projection、
  declaration order、type-parent scope chain、compiler integration regression、
  Task 2 acceptance evidence。
- 后续项目：补齐 class/interface/const generic producer、剩余 Task 2 producer、
  source/binary/native parity 与 LSP consumer migration，再执行 Task 2 最终门禁。
