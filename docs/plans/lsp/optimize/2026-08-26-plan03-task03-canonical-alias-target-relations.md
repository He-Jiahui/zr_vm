---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
implementation_files:
  - tests/parser/test_semantic_query_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/acceptance/2026-08-26-plan03-task03-canonical-alias-target-relations.md
doc_type: milestone-record
---

# Plan 03 Task 3.10: Canonical Alias Target Relations

## 状态与产出记录

- 完成时间：2026-08-26 11:34 +08:00。
- 状态：已完成。
- 完成项目：source scope 构建后，从 `isAlias` visible fact、exact alias
  SymbolId、registered symbol canonical TypeId 与 declaration range 发布
  `ZR_SEMANTIC_RELATION_ALIAS_TARGET`。缺少 exact target declaration identity
  时 target SymbolId/range 保持 unavailable；import alias 同时保留独立
  `IMPORT_EXPORT_ORIGIN` edge。重复发布按 source SymbolId + target TypeId
  幂等，禁止 name、AST、URI 与 display-text fallback。
- RED：MSVC direct relation suite 为 16 tests / 1 failure；新增 type-value
  alias 查询没有 relation，证明缺少 canonical producer，而不是 LSP consumer
  显示问题。
- 验证：MSVC direct relation 16/16、semantic query 30/30、symbols 19/19、
  canonical consumers 19/19、compiler semantic query diagnostics 46/46、
  compiler integration 127/127，均真实 exit 0；WSL GCC direct relation
  16/16、WSL Clang 14 direct relation 16/16，均真实 exit 0。
- 未完成边界：alias chain 的独立 target SymbolId/declaration range、binary/native
  alias producer、project generation 传播与 LSP alias consumer 由后续子里程碑
  继续；在 lower layer 没有 exact target identity 时不得补名称推断。
