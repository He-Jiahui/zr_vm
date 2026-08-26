---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_relations.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
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

# Acceptance: Plan 03 Task 3.12 Source Interface Member Relations

The source fixture compiles `interface Readable.read` and the exact
`class Device.read` implementation, then queries the interface member's stable
SymbolId. It requires one `IMPLEMENTATION` edge from the class member to the
interface member with both canonical TypeIds and both declaration ranges. The
test never searches a member name or asks an LSP consumer to recover the edge.

## 状态与产出记录

- 完成时间：2026-08-26 13:33 +08:00。
- 状态：已完成。
- 完成项目：source class interface-member producer 和 reverse
  `ImplementationsOf` acceptance 已完成；关系只在 canonical class requirement
  validation 成功后发布，并保留 exact source/target identity 与 range。
- RED：MSVC relation 17 Tests / 1 Failure，唯一失败为新增 implementation
  query 返回 false；source parse/compile 无错误，既有 16 项通过。
- 验证：direct process exits 0。MSVC relation 17/17、semantic query 30/30、
  symbols 19/19、canonical consumers 19/19、compiler diagnostics 46/46；
  GCC relation 17/17；Clang14 relation 17/17。
- 未完成边界：source struct interface syntax、binary/native/external
  implementation relation producers 和 LSP relation consumer 不在本项范围；
  不存在 stable target SymbolId 时不发布关系。
