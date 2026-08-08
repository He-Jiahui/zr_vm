---
doc_type: acceptance-record
plan_id: lsp-02-diagnostics
record_id: 2026-08-08-lsp-02-for-foreach-header-safe-fix
status: validated_pending_integration
validated_at: 2026-08-08 12:46 +08:00
related_record: docs/plans/lsp/02-diagnostics/2026-08-08-for-foreach-header-safe-fix-convergence.md
---

# LSP 02 For/Foreach Header Safe-Fix Acceptance

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-08 12:46 +08:00 | 已验证，待主树集成 | 四类for/foreach header parser diagnostic均发布唯一machine-applicable零宽edit；generic LSP code action应用后以v2重绑定清除该code；隔离GCC/Clang/MSVC定向parser 43/43与advanced editor suite均通过 |

## 验收断言

- `for (; ready; ready = false { return 1; }`的`missing_for_header_close`保留primary`[28,29]`并在`[28,28]`插入`)`；`for (i = 0 i < 3; i = i + 1) { out i; }`的`missing_for_header_separator`保留primary`[11,12]`并在`[11,11]`插入`;`。
- `for (var item in items { return item; }`的`missing_foreach_header_close`保留primary`[23,24]`并在`[23,23]`插入`)`；`for (var item items) { return item; }`的`missing_foreach_in_keyword`保留primary`[14,19]`并在`[14,14]`插入`in `。
- 四种edit均由parser structured `fixes[]`提供。LSP generic code action不按loop AST、diagnostic code、message或源文本补偿；应用edit后以递增document version重新绑定并验证对应code消失。

## 验收结果

- GCC 11.4、Clang 14、MSVC 19.44：`zr_vm_compiler_semantic_query_diagnostics_test`均为43 Tests / 0 Failures / 0 Ignored，真实exit 0。
- GCC 11.4、Clang 14、MSVC 19.44：`zr_vm_language_server_lsp_advanced_editor_features_test`均以`0 failure(s)`完成、真实exit 0；四项新增for/foreach header测试均Pass。
- 本验收仍为隔离叶子证据，待主树精确集成并重放同一验证后才可升级为`completed`。
