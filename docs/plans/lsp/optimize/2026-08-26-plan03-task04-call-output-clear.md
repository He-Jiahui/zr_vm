---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/acceptance/2026-08-26-plan03-task04-call-output-clear.md
doc_type: milestone-record
---

# Plan 03 Task 4.8: Call Output Clearing

## 状态与产出记录

- 完成时间：2026-08-26 07:35 +08:00。
- 状态：已完成，待本子项精确提交。
- 完成项目：invalid caller/target SymbolId 清空 reusable call-edge array；RED
  `Expected 0 Was 1` 后 GREEN `10 Tests 0 Failures`、真实退出码 0。
- 后续项目：relation output reuse audit、complete call metadata producers 与
  LSP hierarchy migration。
