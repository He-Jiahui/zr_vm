---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_implementation_query.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_implementation_query.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/language_server/test_lsp_semantic_query_parity.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_project_features.c
  - tests/acceptance/2026-08-30-plan03-task07-canonical-implementation-identity.md
doc_type: milestone-detail
---

# Plan 03 Task 7.30 Canonical Implementation Identity

## 目标

- Local implementation navigation consumes the copied canonical SymbolId when the semantic query has
  published one.
- `ImplementationsOf` is the only relation source; detaching the analyzer symbol table must not remove
  an already-resolved implementation result.
- Producer identity mismatches remain explicit producer debt; this task does not choose between ids by
  name, declaration text, or URI shape.

## 执行

1. RED resolved an interface declaration, verified canonical relation facts, then detached the analyzer
   symbol table before calling `GetImplementation`. The old implementation query still required a raw
   `query.symbol`, so relationFacts contained the matching implementation but the result was empty;
   parity was 13 Pass/1 Fail with real exit 1.
2. GREEN derives one internal SymbolId: copied canonical identity is used when raw identity is absent or
   equal; otherwise the existing raw identity is retained until its producer mismatch is repaired. The
   relation query and range projection remain unchanged.
3. The test now returns the same implementation location with the symbol table detached. Invalid or
   unavailable ids still return false and do not create a synthetic relation target.

## 状态与产出记录

- 完成时间：2026-08-30 04:54 +08:00。
- 状态：已完成。
- 完成项目：implementation RED/GREEN、copied SymbolId projection、detached symbol-table coverage、
  invalid identity boundary、三工具链 parity/source-contract/interface/project marker 审计。
- 后续边界：extern/web URI producer identity mismatch、closed-generic missing canonical view、
  imported source identity and source-local rename remain outside this consumer slice.
