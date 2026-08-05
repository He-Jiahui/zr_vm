---
doc_type: acceptance-record
plan_id: lsp-02-diagnostics
record_id: 2026-08-05-lsp-02-using-resource-close-safe-fix
status: completed
completed_at: 2026-08-05 18:19 +08:00
related_record: docs/plans/lsp/02-diagnostics/2026-08-05-using-resource-close-safe-fix-convergence.md
---

# LSP 02 Using-Resource Close Safe-Fix Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-05 18:19 +08:00 | 已完成 | malformed using resource的`missing_using_resource_close`保留using body `{` primary、在current cursor发布零宽`)` fix；generic LSP code action应用后v2重新绑定清除该code；主树GCC/Clang/MSVC定向parser 39/39与advanced editor验证均通过 |

## 验收断言

- parser builder发布唯一machine-applicable fix，replacement为`)`，fix range为using body `{`前recovery cursor的零宽位置。
- `using (resource { return resource; }`经code action变为`using (resource ) { return resource; }`；primary为`[16,17]`，fix为`[16,16]`。
- LSP通过generic structured diagnostic fix消费edit，并在v2重新绑定中清除`missing_using_resource_close`；没有按using AST、code、message或源文本补偿。

## 验收结果

- GCC 11.4、Clang 14、MSVC 19.44：主树`zr_vm_compiler_semantic_query_diagnostics_test`均为39 Tests / 0 Failures / 0 Ignored，真实exit 0。
- GCC 11.4、Clang 14、MSVC 19.44：主树`zr_vm_language_server_lsp_advanced_editor_features_test`均以`0 failure(s)`完成、真实exit 0。
- 隔离提交`cb74b15`已以主树提交`83f71a6`精确集成；本记录与集中状态表已更新为`completed`。
