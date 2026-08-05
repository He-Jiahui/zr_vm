---
doc_type: milestone-detail
plan_id: lsp-02-diagnostics
record_id: 2026-08-05-catch-pattern-close-safe-fix-convergence
status: validated_pending_integration
validated_at: 2026-08-05 16:53 +08:00
source_plans:
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: catch-pattern-close-safe-fix-convergence
---

# Catch-Pattern Close Safe-Fix Convergence

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-05 16:53 +08:00 | 已验证，待主树集成 | `missing_catch_pattern_close`从parser structured diagnostic到machine-applicable `)` edit、generic LSP code action与v2 apply-edit-rebind闭环；primary保留catch body `{`，fix为current lexer cursor；隔离分支GCC/Clang/MSVC的parser 38/38和advanced editor suite均真实exit 0 |

## 已实现契约

- `ZrParser_DiagnosticBuilder_BuildMissingCatchPatternClose`接受独立`fixLocation`：它保留调用方已有catch body-opening `{` primary range，并将fix收束为零宽range后发布标题`Insert missing ')'`、replacement为`)`的machine-applicable structured fix。
- `report_missing_catch_pattern_close`使用current lexer location作为fix位置。因此`try { throw 1; } catch (error { return 2; }`会产生primary `[30,31]`、fix `[30,30]`，应用edit后成为`try { throw 1; } catch (error ) { return 2; }`。
- LSP没有新增按诊断类别分支的生产代码。通用code-action层只消费`diagnostic.fixes[]`，并在文档版本递增后重新绑定；它不按catch AST、diagnostic code、message或源文本推断或重建`)`。

## TDD 与验证

- RED：parser builder测试以第四个`fixLocation`参数调用旧三参数API，编译失败，证明旧诊断没有machine fix路径。
- GREEN：parser测试断言唯一machine-applicable`)` fix及零宽cursor range；LSP测试断言generic code action应用edit后，v2诊断重新绑定不再包含`missing_catch_pattern_close`。
- 隔离分支`codex/lsp-02-catch-pattern-close-safe-fix`基于已验证的`245c9b0`，使用独立static cache。
- GCC 11.4、Clang 14和MSVC 19.44分别构建并直接执行`zr_vm_compiler_semantic_query_diagnostics_test`，均为38 Tests / 0 Failures / 0 Ignored、真实exit 0。
- 三套同样直接执行`zr_vm_language_server_lsp_advanced_editor_features_test`；新增`LSP code action inserts missing catch pattern close`均Pass，suite均以`0 failure(s)`结束且真实exit 0。
- advanced suite尾部invalid code-lens及unbound-attribute fixtures会打印预期compiler diagnostics；测试进程在三套工具链仍以exit 0结束，不是本里程碑失败。

## 集成边界

- 此记录反映隔离分支已经通过验证；主工作树正由独立任务进行全量验收，未被修改以免作废其快照。依序合入`c817e41`、`5be340b`、`245c9b0`，再合入本叶子后，才将本记录和集中状态表转为`completed`。
- 不改变diagnostic、semantic query、artifact/binary、code-action snapshot或JSON schema；不修改LSP production、stdio、CLI或CMake。
- 其他delimiter、replacement、module/property/ownership diagnostics及L3全量registry、性能、cancellation、乱序race证据仍未完成。
