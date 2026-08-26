---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_relations.c
  - tests/parser/test_semantic_query.c
  - tests/parser/test_semantic_query_symbols.c
  - tests/parser/test_canonical_consumers.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
doc_type: milestone-record
---

# Plan 03 Task 3.12: Source Interface Member Relations

## 状态与产出记录

- 完成时间：2026-08-26 13:33 +08:00。
- 状态：已完成。
- 完成项目：source class requirement validation 在已选出并验证 exact
  `SZrTypeMemberInfo` pair 后发布 `IMPLEMENTATION` fact；source 为 class
  member canonical `SymbolId`，target 为 interface member canonical
  `SymbolId`，`TypeId` 与 declaration range 只从已注册 symbol records 投影。
  publisher 不按 member name、signature text、interface slot、inheritance
  spelling、AST traversal 或 LSP state 重建关系。
- RED：isolated MSVC relation target 为 17 Tests / 1 Failure，新增 source
  class fixture 解析和编译成功，唯一失败为 `ImplementationsOf` 对 interface
  member 返回 false；既有 16 项全部通过。
- 验证：direct process exits 0。MSVC relation 17/17、semantic query 30/30、
  symbols 19/19、canonical consumers 19/19、compiler diagnostics 46/46；
  GCC relation 17/17；Clang14 relation 17/17。
- 未完成边界：source struct 当前没有 inheritance/interface-list 语法，本项
  不扩展 syntax；binary/native/external interface member identity 和 relation
  producer 仍待后续里程碑。缺失 exact endpoint SymbolId 时保持 unavailable，
  不允许 consumer fallback。
