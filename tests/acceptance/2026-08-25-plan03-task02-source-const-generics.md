---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - tests/parser/test_semantic_query_symbols.c
related_docs:
  - docs/plans/lsp/optimize/2026-08-25-plan03-task02-source-const-generics.md
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 2.2h Source Const Generic Scope Facts

## Scope

Validate source const generic visibility from canonical scope facts. The
candidate identity must be owned by the exact source type, not reconstructed
from a generic name or its integral annotation.

## Test Inventory

`test_visible_symbols_projects_source_const_generic_parameter` compiles:

```zr
struct Matrix<const N: int> { }
```

It verifies that `N` is absent at `Matrix`, visible at its declaration, has a
non-invalid candidate `SymbolId` and generic-parameter `TypeId`, and retains
the exact registered `Matrix` owner id.

## Tooling Evidence

| Environment | Targets | Result |
| --- | --- | --- |
| GCC static | symbols, query, contract, compiler diagnostics, compiler integration | 12/12, 29/29, 3/3, 46/46, 127/127; zero failures; process exit 0 |
| MSVC static | symbols, query, contract, compiler diagnostics, compiler integration | 12/12, 29/29, 3/3, 46/46, 127/127; zero failures; process exit 0 |
| Clang 14 WSL static | changed parser/test source compilation | Both changed source units compiled; executable link blocked by existing C11 inline unresolved references including `ZrCore_Memory_RawFreeWithType` |

## Acceptance Decision

Accepted for the GCC and MSVC source const-generic producer submilestone. The
Clang executable gate is not claimed because the static-link ABI failure is
outside this child write set. Interface method generic producers, type members,
imports/aliases, receiver members, binary/native parity, and LSP consumer
migration remain open.

## 状态与产出记录

- 完成时间：2026-08-25 14:23:25 +08:00
- 状态：GCC/MSVC acceptance passed；Clang executable gate 被既有 static link ABI
  失败阻断，未计入通过声明。
- 完成项目：const generic canonical owner/type identity、scope projection、
  declaration order、compiler integration regression、Task 2 acceptance evidence。
- 后续项目：补齐 interface method generic producer、剩余 Task 2 producer、
  source/binary/native parity 与 LSP consumer migration，再执行 Task 2 最终门禁。
