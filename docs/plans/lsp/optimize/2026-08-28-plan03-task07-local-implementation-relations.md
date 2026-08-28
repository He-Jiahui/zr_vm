---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_implementation_query.c
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_interface.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
doc_type: milestone-record
---

# Plan 03 Task 7.6: Local Implementation Relations

## Scope

This slice migrates source-local implementation navigation from the legacy
definition path to parser-owned implementation relations. It covers exact
source type and interface-member implementations in one analyzer snapshot.
Cross-project, binary, and native external implementations remain open.
Syntax05-owned property, symbol, metadata, token, and interface paths are
untouched.

## TDD And Root Cause

On fixed baseline `c24d25921bd82b0134785652aaa0063e91cd763e`, the LSP
implementation request returned the queried interface declaration itself. A
new fixture also contained an unrelated same-name method, so a name scan could
not satisfy the RED correctly.

The parser relation API already exposed `ImplementationsOf`, but the analyzer
compiler lifecycle did not publish source prototype contracts after semantic
symbols existed. Initial publication also retained pointers while
`find_compiler_type_prototype` could grow and relocate the prototype array.
The producer now uses shallow snapshots with a fixed source count and compares
prototype identity canonically rather than by transient address.

## Implementation

`PublishCompilerContracts` projects base/interface, exact interface-member,
and explicit override edges from compiler prototypes into the semantic
relation graph. Declaration-backed members publish through exact AST symbol
identity. Source type placeholders use a unique compiler-registered type
symbol bridge; ambiguity fails closed. Interface member matching reuses the
parser's canonical signature and receiver-effect contract.

The analyzer invokes this producer after symbol collection and before
reference collection. The LSP implementation helper resolves one local
canonical SymbolId, calls `ImplementationsOf`, binds missing sources only
through the owning analyzer snapshot, converts exact fact ranges, and
de-duplicates locations. It does not inspect symbol/member names, the legacy
reference tracker, or source text.

## Verification

GCC 11.4, Clang 14.0.0, and MSVC 19.44.35228.0 each return real exit zero for
semantic relations `19/19`, semantic-query parity `5/5`, source contracts
`58/58`, reaching definition `3/3`, and reference tracker `5/5`. The final
producer identity change was rebuilt and reran relations `19/19` and parity
`5/5` on all three toolchains.

The full interface runner returns exit one on every toolchain with the same
`109 Pass / 4` pre-existing markers as Task 7.5; marker delta is zero and the
runner is not counted as GREEN. Workspace, WSL, and MSVC bytes match `11/11`;
`git diff --check` passes.

## 状态与产出记录

- 完成时间：2026-08-28 21:28 +08:00。
- 状态：Task 7.6 local implementation relations 子里程碑已完成；Plan 03
  Task 7 继续。
- 完成项目：compiler-owned type/interface/member/override relation producer、
  exact declaration identity bridge、snapshot-safe prototype traversal、
  `ImplementationsOf(SymbolId)` LSP projection、analyzer-only source binding、
  same-name exclusion、三工具链 focused 门禁、interface marker delta 0、三处
  `11/11` byte audit。
- 未完成项目：cross-project/binary/native external implementations、rename、
  call/type hierarchy 和其余 consumers，以及 Syntax05 当前占用的 property/
  symbol/metadata/token/interface paths。
