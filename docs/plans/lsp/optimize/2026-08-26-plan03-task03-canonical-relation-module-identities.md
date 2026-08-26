---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
implementation_files:
  - tests/parser/test_semantic_query_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/acceptance/2026-08-26-plan03-task03-canonical-relation-module-identities.md
doc_type: milestone-record
---

# Plan 03 Task 3.9: Canonical Relation Module Identities

## 状态与产出记录

- 完成时间：2026-08-26 10:49 +08:00。
- 状态：已完成。
- 完成项目：`SZrParserSemanticRelationQuery` 为 source/target endpoint 发布
  snapshot-borrowed module identity。投影只从 relation fact 的 canonical TypeId
  出发：nominal node 直接返回其 `moduleIdentity`，generic instance 仅沿
  `definitionTypeId` 到 nominal definition；invalid、unknown、unqualified、
  primitive、structural 或 malformed cycle 均保持 unavailable。没有 URI、display
  text、symbol name、source path 或 AST fallback。
- 验证：MSVC direct relation 15/15、semantic query 30/30、semantic query calls
  10/10、canonical consumers 19/19，均真实 exit 0；WSL GCC direct relation
  15/15，exit 0；WSL Clang 14 direct relation 15/15，exit 0。
- 未完成边界：source unqualified module identity、binary/native relation producer、
  hierarchy item snapshot data 与 LSP hierarchy consumer 仍由后续里程碑处理；跨
  snapshot consumer 必须复制稳定 ids/module identity text/URI/range，不得保留
  borrowed pointer。
