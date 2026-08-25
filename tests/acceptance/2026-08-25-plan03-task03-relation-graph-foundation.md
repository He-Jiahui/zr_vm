---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/optimize/2026-08-25-plan03-task03-relation-graph-foundation.md
tests:
  - tests/parser/test_semantic_query_relations.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 3.1 Relation Graph Foundation

## Scope

Accept only the parser snapshot carrier and read-only relation query contract.
The contract includes typed endpoints, exact endpoint ranges, external origin
URI ownership, deterministic projection, output reuse, and node-scope filtering.

## Required Results

- A relation cannot be appended without a non-invalid SymbolId or TypeId
  endpoint.
- Query output contains stable copied relation values, never a mutable fact or
  an AST pointer.
- `BaseTypesOf` and `DerivedTypesOf` read the same directed base-type edge in
  opposite directions; `ImplementationsOf` resolves the target contract.
- A relation with no published endpoint inside a node scope is unavailable.
- External origin is carried only by `externalOriginUri`; no source URI is
  invented for binary/native endpoints.

## Evidence

The isolated MSVC static cache
`.codex/build-lsp-plan03-native-generic-msvc-r2` rebuilt
`zr_vm_semantic_query_relations_test`. Direct execution returned process exit
zero and Unity reported `3 Tests 0 Failures 0 Ignored`. Direct adjacent tests
also returned process exit zero: semantic query `29/0`, semantic-query symbols
`19/0`, and semantic query contract `3/0`.

## Acceptance Decision

Accepted for the Task 3.1 relation graph foundation only. Compiler relation
producers, external metadata projection, call graph facts, and LSP consumers
remain unaccepted.

## 状态与产出记录

- 完成时间：2026-08-25 23:24 +08:00。
- 状态：已完成并随本提交精确提交。
- 完成项目：关系事实 carrier、只读 relation query、稳定排序、scope过滤、
  external origin投影测试。
- 后续项目：source/binary/native relation producers、call graph、LSP projection。
