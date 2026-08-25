---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
tests:
  - tests/parser/test_semantic_query_calls.c
doc_type: acceptance-record
---

# Acceptance: Plan 03 Task 4.8 Call Output Clearing

Direct isolated MSVC execution reports `10 Tests 0 Failures 0 Ignored` and
process exit zero after invalid outgoing/incoming queries no longer retain a
previous call edge.

## 状态与产出记录

- 完成时间：2026-08-26 07:35 +08:00。
- 状态：已完成，待本子项精确提交。
- 完成项目：call hierarchy stale-output fail-closed acceptance。
- 后续项目：complete call facts and LSP hierarchy migration。
