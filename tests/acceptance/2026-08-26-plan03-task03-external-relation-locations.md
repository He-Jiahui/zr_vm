---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
implementation_files:
  - tests/parser/test_semantic_query_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_relations.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 3.13 External Relation Locations

The parser relation test publishes an external relation with no source or
target range. It first proves that an origin without a virtual declaration URI
is rejected, then supplies both metadata values and queries the exact source
SymbolId. The result must retain both URI contents and explicit unavailable
range flags without consulting an AST, module spelling, or LSP state.

## 状态与产出记录

- 完成时间：2026-08-26 14:01 +08:00。
- 状态：已完成。
- 完成项目：external no-source-range validation、snapshot URI retention 与
  `RelationsOfSymbol` 双 URI 投影回归。
- RED：MSVC relation target 编译失败，明确显示 fact/query 缺少
  `virtualDeclarationUri`。
- 验证：direct process exits 0。MSVC relation 18/18、semantic query 30/30、
  symbols 19/19、canonical consumers 19/19、compiler diagnostics 46/46；
  WSL GCC relation 18/18；WSL Clang14 relation 18/18。
- 未完成边界：本项冻结 metadata-to-query contract，不生成 binary/native
  virtual document，不迁移 LSP navigation consumer，也不允许 consumer URI
  fallback。
