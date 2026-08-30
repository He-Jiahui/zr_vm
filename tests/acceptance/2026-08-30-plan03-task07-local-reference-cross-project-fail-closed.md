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

- 完成时间：2026-08-30 09:50 +08:00。
- 状态：Task 7.38 focused 与 post-commit gate GREEN；GCC/Clang/MSVC source-contract 均为
  `70/70`、真实 exit 0，16-target 最终门禁为 `10/16`，Plan 03 Task 7/Task 8 仍未完成。
- 完成项目：source-contract RED assertion、local consumer fail-closed implementation、
  三工具链 source-contract 与同基线 16-target 真实退出复核、三套 stdio smoke 和 CLI
  `--version` 回放。
- 未完成项目：stdio smoke 共同停在 `short_circuit_unreachable` producer warning 缺失；
  其余失败为 canonical tuple、member-write、imported-type、interface fixture 与 analyzer
  producer 边界。producer/metadata ownership 收口前，LSP 不按名称、类型文本或消息增加
  兼容。
