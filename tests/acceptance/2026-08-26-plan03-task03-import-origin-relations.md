---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_scope_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-26-plan03-task03-import-origin-relations.md
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/parser/test_semantic_query_symbols.c
  - tests/parser/test_semantic_query.c
  - tests/parser/test_property_consumer_contracts.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 3.4 Source Import Origin Relations

## Scope

Accept only source `import(...)` alias origin production. The accepted relation
uses a snapshot-owned URI and the exact resolved local alias identity published
by the compiler source-scope pass.

## Required Results

- An import alias relation is external and uses the local alias SymbolId as its
  source endpoint.
- The external endpoint has no fabricated SymbolId or source range; it carries
  the canonical alias TypeId and normalized import URI.
- A missing URI, resolved identity, TypeId, or declaration range produces no
  relation.
- A same-name native symbol cannot replace a source destructured alias because
  test and producer use the canonical binding SymbolId rather than a name.
- Repeated publication does not duplicate the relation.

## Evidence

The dedicated MSVC static cache
`.codex/build-lsp-plan03-native-generic-msvc-r2` directly returned process exit
zero for `zr_vm_semantic_query_relations_test` (`8 Tests 0 Failures 0
Ignored`), `zr_vm_semantic_query_symbols_test` (`19 Tests 0 Failures 0
Ignored`), `zr_vm_semantic_query_test` (`29 Tests 0 Failures 0 Ignored`),
`zr_vm_property_consumer_contracts_test` (`11 Tests 0 Failures 0 Ignored`),
and `zr_vm_compiler_semantic_query_diagnostics_test` (`46 Tests 0 Failures 0
Ignored`). The shared working tree contains unrelated uncommitted work, so
this is not claimed as a clean-baseline or three-toolchain matrix result.

## Acceptance Decision

Accepted for source import-origin relations only. Alias target, binary/native
origin, other relation producers, and LSP consumers remain unaccepted.

## 状态与产出记录

- 完成时间：2026-08-26 00:51 +08:00。
- 状态：已完成并将随本子项精确提交。
- 完成项目：snapshot-owned import URI、结构化 external origin relation、
  native 同名碰撞防护、精确 local alias identity、幂等性与既有查询回归。
- 后续项目：alias/export/binary/native relation producers、call graph 和 LSP projection。
