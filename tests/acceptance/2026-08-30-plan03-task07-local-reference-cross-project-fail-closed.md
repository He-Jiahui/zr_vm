---
plan:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
task: 7.38
doc_type: acceptance-record
---

# Plan 03 Task 7.38 Acceptance: Local Reference Cross-Project Fail-Closed

## Acceptance Criteria

- Local reference append consumes parser relation queries only.
- The local-symbol branch of `LspSemanticQuery_AppendReferences` contains no
  `query->symbol->name` aggregation path.
- Missing canonical cross-project relation remains unavailable rather than being inferred
  from an equal member name.
- Existing imported-member and external metadata consumers retain their structured identity
  boundaries.
- Focused tests record real process exits; the final Plan 03 matrix records both exit status
  and test markers.

## 状态与产出记录

- 完成时间：2026-08-30 08:31 +08:00。
- 状态：Task 7.38 focused GREEN；GCC/Clang/MSVC source-contract 均真实 exit 0，生产
  `lsp_semantic_query.c` 三工具链单文件语法检查通过。
- 完成项目：source-contract RED assertion、local consumer fail-closed implementation、
  三工具链 source-contract 回归与生产语法检查。
- 未完成项目：同一基线上的 interface、stdio smoke 与最终 16-target matrix；Plan 03
  Task 7/Task 8 仍未完成。
