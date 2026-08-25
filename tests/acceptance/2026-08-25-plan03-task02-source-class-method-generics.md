---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_type_member.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - tests/parser/test_semantic_query_symbols.c
related_docs:
  - docs/plans/lsp/optimize/2026-08-25-plan03-task02-source-class-method-generics.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 2.2g Source Class Method Generic Scope Facts

## Scope

Validate canonical visible-symbol facts for source class method type generics.
The method owner must originate in the compiler-published member symbol. The
scope query cannot recreate it from a class member name, source text, or AST
pairing.

## Test Inventory

`test_visible_symbols_projects_source_class_method_generic_parameter` compiles:

```zr
class Crate<T> { fn echo<U>(value: U): U { return value; } }
```

It verifies that the method declaration has a real canonical function symbol;
`U` is unavailable at `echo`; exactly one `U` appears at its generic
declaration; that candidate has the method's exact owner id and a canonical
generic-parameter `TypeId`; and the enclosing `T` remains visible through the
published class-to-method parent scope.

## Tooling Evidence

| Environment | Targets | Result |
| --- | --- | --- |
| GCC static | symbols, query, contract, compiler diagnostics, compiler integration | 11/11, 29/29, 3/3, 46/46, 127/127; zero failures; process exit 0 |
| MSVC static | symbols, query, contract, compiler diagnostics, compiler integration | 11/11, 29/29, 3/3, 46/46, 127/127; zero failures; process exit 0 |
| Clang 14 WSL static | changed parser/test source compilation | All five changed source units compiled; executable link blocked by existing C11 inline unresolved references including `ZrCore_Memory_RawFreeWithType` |

## Acceptance Decision

Accepted for the GCC and MSVC source class-method generic producer submilestone.
The Clang executable gate is not claimed because its static-link ABI failure is
outside this child write set. Const/interface method generic producers, type
members, imports/aliases, receiver members, binary/native parity, and LSP
consumer migration remain open.

## 状态与产出记录

- 完成时间：2026-08-25 14:09:50 +08:00
- 状态：GCC/MSVC acceptance passed；Clang executable gate 被既有 static link ABI
  失败阻断，未计入通过声明。
- 完成项目：class method canonical owner、共享 type-member symbol registration、
  method generic scope projection、declaration order、type-parent scope chain、
  compiler integration regression、Task 2 acceptance evidence。
- 后续项目：补齐 interface/const generic producer、剩余 Task 2 producer、
  source/binary/native parity 与 LSP consumer migration，再执行 Task 2 最终门禁。
