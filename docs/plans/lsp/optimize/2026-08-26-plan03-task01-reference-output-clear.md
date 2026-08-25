---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query.c
  - tests/acceptance/2026-08-26-plan03-task01-reference-output-clear.md
doc_type: milestone-record
---

# Plan 03 Task 1.4: ReferencesOf Output Clearing

## 状态与产出记录

- 完成时间：2026-08-26 07:31 +08:00。
- 状态：已完成，待本子项精确提交。
- 完成项目：invalid SymbolId fail-closed 时清空复用 reference array；RED
  `Expected 0 Was 2` 后 GREEN `30 Tests 0 Failures`、真实退出码 0。
- 后续项目：remaining query output reuse audit、canonical producers 和 LSP
  navigation migration。
