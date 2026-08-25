---
related_code:
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/src/zr_vm_parser/type_environment_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_interface.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - tests/parser/test_semantic_query_symbols.c
related_docs:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/parser-and-semantics/semantic-query-api-foundation.md
doc_type: milestone-record
---

# Plan 03 Task 2.2c: Source Type Scope Facts

## Goal

Extend the canonical `VisibleSymbols` producer with source type declarations
without allowing the query or LSP to rebuild type lookup from names, ranges, or
AST traversal.

## Implementation

- Added `ZrParser_TypeEnvironment_RegisterTypeDeclaration`, which carries the
  source declaration AST identity into canonical named-type and type-symbol
  registration. The existing registration entrypoint remains available for
  callers without a declaration node.
- Moved type-environment registration into the cohesive
  `type_environment_types.c` module. The existing large type-system source no
  longer owns this separate registration concern.
- Changed source struct, class, and interface compilation to use the
  declaration-aware registration entrypoint.
- Extended source scope-fact publication to find an exact canonical type symbol
  by declaration-node identity, publish it as a hoisted parent-scope candidate,
  and create a type-owned scope. It fails closed when that canonical record is
  absent.
- Added a compiler-backed `VisibleSymbols` regression covering `struct Point`,
  `class Meter`, and `interface Readable` at a later source position.

## Contract

The visible type candidate uses the registered `SymbolId`, `TypeId`, and exact
declaration location from the semantic context. It is never found by matching a
type name or a source range. Duplicate registration does not publish a new
symbol identity. This child milestone only makes top-level source type
declarations visible; generic parameters, type members, receiver members,
imports, aliases, binary metadata, and native descriptors remain pending Task 2
producers.

## Verification

- RED: before declaration-aware type registration and scope projection, the
  new source test found zero visible type candidates.
- GCC static: `zr_vm_semantic_query_symbols_test` 7/7,
  `zr_vm_semantic_query_test` 29/29,
  `zr_vm_semantic_query_contract_test` 3/3, and
  `zr_vm_compiler_semantic_query_diagnostics_test` 46/46, all process exit 0.
- MSVC static: the same four targets passed 7/7, 29/29, 3/3, and 46/46 with
  process exit 0.
- Clang 14 WSL: the final parser static target and focused test source compile
  completed. Executable linking remains blocked by the existing static C11
  inline ABI unresolved references beginning at `ZrCore_Memory_RawFree`; no
  Clang executable test pass is claimed.

## 状态与产出记录

- 完成时间：2026-08-25 12:44:45 +08:00
- 状态：GCC 与 MSVC 子里程碑完成；Clang static 可执行测试被既有链接门禁阻断，
  不宣称三工具链完成。
- 完成项目：source struct/class/interface canonical type identity 注册、
  module type candidate 与 type scope projection、fail-closed producer、
  focused RED/GREEN coverage、module documentation、Task 2 子里程碑记录与验收证据。
- 后续项目：canonical generic parameter/type-member/import/alias/receiver
  producers、binary/native parity、LSP visible-symbol consumer migration，以及 Task 2
  最终验收。
