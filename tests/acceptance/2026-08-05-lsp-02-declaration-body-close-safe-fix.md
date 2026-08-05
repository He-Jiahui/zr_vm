---
doc_type: acceptance-record
plan_id: lsp-02-diagnostics
record_id: 2026-08-05-declaration-body-close-safe-fix
status: completed
completed_at: 2026-08-05 14:14 +08:00
related_record: docs/plans/lsp/02-diagnostics/2026-08-05-declaration-body-close-safe-fix-convergence.md
---

# LSP 02 Declaration-Body Close Safe-Fix Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-05 14:14 +08:00 | 已完成 | `class Box {\n    var id: int;`的`missing_declaration_body_close`保留opening `{` primary、在EOF发布零宽`}` fix；generic LSP code action应用后v2重新绑定清除该code；GCC/Clang/MSVC定向parser与advanced editor验证均通过 |

## 验收断言

- parser builder发布唯一machine-applicable fix，replacement为`}`，fix range为EOF零宽位置。
- LSP action不检查diagnostic code、message、declaration kind或AST来构造edit；它只消费structured fix。
- 应用action后以递增document version重新查询，原diagnostic code不再存在。
- GCC、Clang和MSVC均直接执行parser diagnostics target（34/34）与advanced editor target（0 failures），真实退出码为0。
