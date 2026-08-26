---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - tests/parser/test_semantic_query_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/parser/test_semantic_query.c
  - tests/parser/test_semantic_query_symbols.c
  - tests/parser/test_canonical_consumers.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 3.6 Source Type Hierarchy Relations

The source fixture proves `Device -> Base` and `Device -> Readable` base-type
edges, `Device -> Readable` implementation, and
`StreamReadable -> Readable` interface inheritance. Every endpoint is taken
from a compiler-resolved declaration AST identity and its already registered
semantic symbol; the test has no name-based query or LSP fallback.

## 状态与产出记录

- 完成时间：2026-08-26 07:59 +08:00。
- 状态：已完成。
- 完成项目：direct isolated MSVC process exit 0; relation 11/11, semantic
  query 30/30, symbols 19/19, canonical consumers 19/19, compiler diagnostics
  46/46。
- 未完成边界：binary/native/external hierarchy and override edges require
  producer-owned declaration identity or explicit external origin metadata.
