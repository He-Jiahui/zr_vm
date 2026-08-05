---
doc_type: acceptance-record
plan_id: lsp-02-diagnostics
record_id: 2026-08-05-lsp-02-block-close-safe-fix
status: completed
completed_at: 2026-08-05 17:13 +08:00
related_record: docs/plans/lsp/02-diagnostics/2026-08-05-block-close-safe-fix-convergence.md
---

# LSP 02 Block-Close Safe-Fix Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-05 17:13 +08:00 | 已完成 | `if (ready) { return 1;`的`missing_block_close`保留opening-brace primary、在EOF cursor发布零宽`}` fix；generic LSP code action应用后v2重新绑定清除该code；主树GCC/Clang/MSVC定向parser 38/38与advanced editor验证均通过 |

## 验收断言

- parser builder发布唯一machine-applicable fix，replacement为`}`，fix range为recovery cursor的零宽位置。
- LSP action不检查diagnostic code、message或block AST来构造edit；它只消费structured fix。
- 应用action后以递增document version重新查询，原diagnostic code不再存在。
- GCC、Clang和MSVC均直接执行主树parser diagnostics target（38/38）与advanced editor target（0 failures），真实退出码为0。
