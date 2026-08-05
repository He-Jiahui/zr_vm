---
doc_type: acceptance-record
plan_id: lsp-02-diagnostics
record_id: 2026-08-05-lsp-02-declaration-body-open-safe-fix
status: validated_pending_integration
validated_at: 2026-08-05 14:52 +08:00
related_record: docs/plans/lsp/02-diagnostics/2026-08-05-declaration-body-open-safe-fix-convergence.md
---

# LSP 02 Declaration-Body Open Safe-Fix Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-05 14:52 +08:00 | 已验证，待主树集成 | `class Box`的`missing_declaration_body_open`保留EOF recovery primary、在同cursor发布零宽`{}` fix；generic LSP code action应用后v2重新绑定清除该code；GCC/Clang/MSVC定向parser与advanced editor验证均通过 |

## 验收断言

- parser builder发布唯一machine-applicable fix，replacement为`{}`，fix range为recovery cursor的零宽位置。
- LSP action不检查diagnostic code、message、declaration kind或AST来构造edit；它只消费structured fix。
- 应用action后以递增document version重新查询，原diagnostic code不再存在。
- GCC、Clang和MSVC均直接执行parser diagnostics target（35/35）与advanced editor target（0 failures），真实退出码为0。
