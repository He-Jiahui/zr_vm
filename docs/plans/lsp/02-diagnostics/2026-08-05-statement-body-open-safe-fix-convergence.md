---
related_code:
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_diagnostics.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_diagnostics.c
plan_sources:
  - user: strict LSP semantic inference plan execution with per-submilestone records and commits
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/language_server/test_lsp_advanced_editor_features.c
  - tests/language_server/test_lsp_diagnostic_safe_fix_cases.h
doc_type: milestone-detail
plan_id: lsp-02-diagnostics
record_id: 2026-08-05-statement-body-open-safe-fix-convergence
status: validated_pending_integration
validated_at: 2026-08-05 16:00 +08:00
source_plans:
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: statement-body-open-safe-fix-convergence
---

# Statement-Body Open Safe-Fix Convergence

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-05 16:00 +08:00 | 已验证，待主树集成 | `missing_statement_body_open`从parser structured diagnostic到machine-applicable `{}` edit、generic LSP code action与v2 apply-edit-rebind闭环；primary保留parser recovery range，fix为current lexer cursor；隔离分支GCC/Clang/MSVC的parser 36/36和advanced editor suite均真实exit 0 |

## 已实现契约

- `ZrParser_DiagnosticBuilder_BuildMissingStatementBodyOpen`接受独立`fixLocation`：它保留调用方已有primary range，并将fix收束为零宽range后发布标题`Insert missing statement body`、replacement为`{}`的machine-applicable structured fix。
- shared parser report在recovery前读取current lexer token location。`if (ready)`的primary/fix均为EOF cursor；插入`{}`后成为语法完整的empty statement body，而不是仅插入`{`后引入新的body-close diagnostic。
- LSP production consumer未增加 statement-kind、message、AST或源码文本分支。既有generic machine-applicable code-action/snapshot pipeline直接消费`diagnostic.fixes[]`。
- 更新后的同一document v2会重新解析与绑定；`if (ready){}`不再暴露`missing_statement_body_open`。

## TDD与根因证据

- RED先向parser builder test传入独立fix range；旧四参数API在编译期拒绝第五参数，证明尚未具备primary/fix双range合同。
- GREEN后builder test断言单一fix的title、`{}` replacement、machine applicability和zero-width range；LSP integration test经generic action取得同一fix，应用至v2后重新查询，不再存在该diagnostic code。

## 工具链与回归证据

- 隔离分支`codex/lsp-02-statement-body-open-safe-fix`基于已验证的`c817e41`，使用独立static cache。
- GCC 11.4、Clang 14和MSVC 19.44分别构建并直接执行`zr_vm_compiler_semantic_query_diagnostics_test`，均为36 Tests / 0 Failures / 0 Ignored、真实exit 0。
- 三套同样直接执行`zr_vm_language_server_lsp_advanced_editor_features_test`；新增`LSP code action inserts missing statement body open`均Pass，suite均以`0 failure(s)`结束且真实exit 0。
- advanced suite尾部invalid code-lens及unbound-attribute fixtures会打印预期compiler diagnostics；测试进程在三套工具链仍以exit 0结束，不是本里程碑失败。

## 集成边界

- 此记录反映隔离分支已经通过验证；主工作树正由独立任务进行全量验收，未被修改以免作废其快照。先合入`c817e41`，再合入本叶子后，才将本记录和集中状态表转为`completed`。
- 不改变diagnostic、semantic query、artifact/binary、code-action snapshot或JSON schema；不修改LSP production、stdio、CLI或CMake。
- 其他delimiter、replacement、module/property/ownership diagnostics及L3全量registry、性能、cancellation、乱序race证据仍未完成。
