---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c
implementation_files:
  - tests/parser/test_semantic_query_relations.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_relations.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 3.11 Override Implementation Query

## Scope

`ImplementationsOf` returns both exact interface-implementation edges and exact
class-override edges whose target is the requested base SymbolId. Relation kinds
remain distinct and no transitive or name-based edge is synthesized.

## Baseline And Diagnosis

The initial MSVC RED ran 16 tests with two failures. A context containing one
`IMPLEMENTATION` and one `OVERRIDE` edge to the same target returned only the
former. A compiled source override was visible through `RelationsOfSymbol` but
`ImplementationsOf(baseMethod)` returned false. Both facts were already exact;
the lowest defect was the query kind filter rather than compiler binding or
relation publication.

## Test Inventory

- Mixed `OVERRIDE` and `IMPLEMENTATION` facts for one target both appear in the
  implementation query with stable kind ordering.
- Each result preserves the implementing/overriding source SymbolId and the
  requested base target SymbolId.
- A compiled source override is discoverable from its exact base method id.
- Invalid target ids still clear reusable output and fail closed.
- Existing hierarchy, constructor, property, alias, import, and relation scope
  cases remain green.

## Tooling Evidence

MSVC used the isolated static cache after importing the Visual Studio developer
environment and invoked each executable directly. WSL GCC used
`.codex/build-lsp-plan03-override-gcc-wsl`; WSL Clang 14 used
`.codex/build-lsp-plan03-scope-clang14-wsl` with `/usr/bin/clang`. Accepted
results include the Unity summary and direct process exit code.

## Results

- MSVC direct: relation 16/16, semantic query 30/30, and symbols 19/19; every
  process exited 0.
- WSL GCC direct: relation 16/16; process exited 0.
- WSL Clang 14 direct: relation 16/16; process exited 0.

## Acceptance Decision

Accepted for exact source override/implementation reverse lookup. The query
remains a projection over existing facts; external producers, transitive
closure, project-generation aggregation, and LSP consumers remain follow-up
work.

## 状态与产出记录

- 完成时间：2026-08-26 12:16 +08:00。
- 状态：已完成。
- 完成项目：override + implementation unified reverse query、direction/sort/
  scope/fail-closed contract 与三工具链定向验收。
- 验证：MSVC relation 16/16、semantic query 30/30、symbols 19/19；WSL GCC
  relation 16/16、WSL Clang 14 relation 16/16，全部真实 exit 0。
