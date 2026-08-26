---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/semantic_relations.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_interface.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
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

# Plan 03 Task 3.6: Source Type Hierarchy Relations

## 状态与产出记录

- 完成时间：2026-08-26 07:59 +08:00。
- 状态：已完成。
- 完成项目：compiler prototype 保留 source declaration identity；class/struct
  base 与已解析 interface implementation 发布 exact `BASE_TYPE` /
  `IMPLEMENTATION` facts；interface inheritance 发布 exact `BASE_TYPE` facts；
  relation endpoint 仅由 registered `symbol.astNode` identity 映射为
  `SymbolId`/`TypeId` 与 declaration range，不按名称回查。
- 验证：isolated MSVC direct process exit 0，relation 11/11、semantic query
  30/30、symbols 19/19、canonical consumers 19/19、compiler diagnostics 46/46。
- 未完成边界：override、alias chain、binary/native hierarchy provenance 和
  LSP hierarchy consumer 仍需各自的 canonical producer；缺少 declaration
  identity 时保持 unavailable，LSP 不得推断补边。
