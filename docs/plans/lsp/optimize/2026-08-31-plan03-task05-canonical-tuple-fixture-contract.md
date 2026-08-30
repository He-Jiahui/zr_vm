---
related_code: []
tests:
  - tests/parser/test_canonical_type_graph.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
module_docs:
  - docs/parser-and-semantics/canonical-type-graph.md
doc_type: milestone-record
---

# Plan 03 Task 5.17: Canonical Tuple Fixture Contract

## Scope

The canonical type graph gate retained one failure in
`test_tuple_ast_projects_to_tuple_type_id`. The test source used
`pair(): [int, bool]`, but keywordless function declarations were removed by
the Syntax plans. The parser rejected the declaration before tuple conversion
or canonical formatting ran.

This submilestone corrects the acceptance fixture. It does not restore legacy
syntax and does not modify parser, inference, TypeId, or formatter production
code.

## Evidence And Change

On fixed HEAD `54ccc7030937d6976192a48ef59405ca59a017a5`, GCC returned
exit one with `19 Tests / 1 Failure`. The only failing assertion was the tuple
AST fixture's non-null parse result; parser diagnostics explicitly reported
the removed keywordless declaration.

The source is now `fn pair(): [int, bool] { return 0; }`. The tuple type surface
remains `[int, bool]`, while the canonical display remains `(int, bool)`. This
keeps syntax ownership in the parser and prevents the canonical graph test from
asking the formatter to compensate for an invalid declaration.

## Verification

The test path was byte-identical between the workspace and independent WSL
source snapshot. GCC 11.4 and Clang 14.0.0 both returned real exit zero for:

- canonical type graph: `19/19`;
- parser: `74/74`;
- semantic display: `22/22`.

Expected negative-fixture compiler messages inside the canonical graph target
remain, while the outer Unity process is green. No production file changed.
MSVC, the complete 16-target matrix, the interface parent, and the three
stdio/CLI smoke suites were not run.

## 状态与产出记录

- 完成时间：2026-08-31 07:15 +08:00。
- 状态：Task 5.17 canonical tuple fixture contract 子里程碑已完成；Plan 03
  Task 5 的LSP alias consumer与Plan 03 Task 7/8继续进行。
- 完成项目：旧keywordless fixture RED归因、current `fn` syntax切换、tuple AST
  projection、canonical `(int, bool)` display、GCC/Clang `19/74/22`真实退出、
  单路径byte audit。
- 后续项目：等待Syntax05释放`semantic_type_prototypes.c`后删除LSP primitive/type-name
  reconstruction；receiver/member与binary/native display parity仍需后续门禁。
