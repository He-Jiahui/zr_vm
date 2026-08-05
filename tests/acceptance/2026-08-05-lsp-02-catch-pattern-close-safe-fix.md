---
doc_type: acceptance-record
plan_id: lsp-02-diagnostics
record_id: 2026-08-05-lsp-02-catch-pattern-close-safe-fix
status: completed
completed_at: 2026-08-05 17:13 +08:00
related_record: docs/plans/lsp/02-diagnostics/2026-08-05-catch-pattern-close-safe-fix-convergence.md
---

# LSP 02 Catch-Pattern Close Safe-Fix Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-05 17:13 +08:00 | 已完成 | malformed catch pattern的`missing_catch_pattern_close`保留catch body `{` primary、在current cursor发布零宽`)` fix；generic LSP code action应用后v2重新绑定清除该code；主树GCC/Clang/MSVC定向parser 38/38与advanced editor验证均通过 |

## 验收断言

- parser builder发布唯一machine-applicable fix，replacement为`)`，fix range为catch body `{`前recovery cursor的零宽位置。
- `try { throw 1; } catch (error { return 2; }`经code action变为`try { throw 1; } catch (error ) { return 2; }`；primary仍为`[30,31]`，fix为`[30,30]`。
- LSP通过generic structured diagnostic fix消费edit，并在v2重新绑定中清除`missing_catch_pattern_close`；没有按catch AST、code、message或源文本补偿。

## 验收结果

- GCC 11.4、Clang 14、MSVC 19.44：主树`zr_vm_compiler_semantic_query_diagnostics_test`均为38 Tests / 0 Failures / 0 Ignored，真实exit 0。
- GCC 11.4、Clang 14、MSVC 19.44：主树`zr_vm_language_server_lsp_advanced_editor_features_test`均以`0 failure(s)`完成、真实exit 0。
- 四个叶子已依序合入主树并完成集成基线复验；本记录和集中状态表已更新为`completed`。
