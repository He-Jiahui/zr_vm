---
doc_type: milestone-detail
plan_id: lsp-02-diagnostics
record_id: 2026-08-08-for-foreach-header-safe-fix-convergence
status: validated_pending_integration
validated_at: 2026-08-08 12:46 +08:00
source_plans:
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: for-foreach-header-safe-fix-convergence
---

# For/Foreach Header Safe-Fix Convergence

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-08 12:46 +08:00 | 已验证，待主树集成 | `missing_for_header_close`、`missing_for_header_separator`、`missing_foreach_header_close`与`missing_foreach_in_keyword`均从parser structured diagnostic发布唯一machine-applicable edit，并由generic LSP code action在v2 apply-edit-rebind后清除对应code；隔离GCC/Clang/MSVC parser 43/43与advanced editor suite均真实exit 0 |

## 已实现契约

- `ZrParser_DiagnosticBuilder_BuildMissingForHeaderClose`、`ZrParser_DiagnosticBuilder_BuildMissingForHeaderSeparator`、`ZrParser_DiagnosticBuilder_BuildMissingForeachHeaderClose`和`ZrParser_DiagnosticBuilder_BuildMissingForeachInKeyword`均接受独立`fixLocation`。调用方保留既有recovery primary，builder把fix收束为零宽range并只经structured diagnostic fix API发布machine-applicable edit。
- `for (; ready; ready = false { return 1; }`保留primary`[28,29]`，在`[28,28]`插入`)`；`for (i = 0 i < 3; i = i + 1) { out i; }`保留primary`[11,12]`，在`[11,11]`插入`;`。
- `for (var item in items { return item; }`保留primary`[23,24]`，在`[23,23]`插入`)`；`for (var item items) { return item; }`保留primary`[14,19]`，在`[14,14]`插入`in `。
- LSP production没有新增按loop类别分支。通用code-action层只消费`diagnostic.fixes[]`，应用edit后使用新的document version重新绑定；不按diagnostic code、message、AST、member name或源文本推断或重建edit。

## TDD 与验证

- RED：parser builder测试以独立`fixLocation`调用旧三参数API，产生接口不兼容编译失败，证明旧四类header诊断没有machine-fix发布路径；LSP advanced suite随后精确报告四个新增header code action场景失败。
- GREEN：parser测试断言每个diagnostic只有一个machine-applicable fix、标题和replacement准确且fix range为零宽；advanced editor测试断言generic action应用edit后，v2诊断重新绑定不再包含对应code。
- 隔离分支`codex/lsp-02-for-foreach-header-safe-fix`基于已集成主树基线`36c4255`，使用独立static cache。
- GCC 11.4、Clang 14和MSVC 19.44分别直接执行`zr_vm_compiler_semantic_query_diagnostics_test`，均为43 Tests / 0 Failures / 0 Ignored、真实exit 0。
- 三套工具链同样直接执行`zr_vm_language_server_lsp_advanced_editor_features_test`；四项新增for/foreach header code-action测试均Pass，suite均以`0 failure(s)`结束且真实exit 0。
- advanced suite末尾invalid code-lens fixture仍会打印预期compiler diagnostic；测试进程在三套工具链均以exit 0结束，不是本里程碑失败。

## 集成边界

- 本记录对应隔离叶子，尚未写入主树完成状态。后续只允许clean index下精确集成该叶子提交，并在同一主树基线重放三工具链parser与advanced editor测试后，才可将本记录和集中计划升级为`completed`。
- 不改变diagnostic registry、semantic query、artifact/binary、code-action snapshot或JSON schema；不修改LSP production、stdio、CLI或CMake。
- 其他delimiter、replacement、module/property/ownership diagnostics及L3全量registry、性能、cancellation、乱序race证据仍未完成。
