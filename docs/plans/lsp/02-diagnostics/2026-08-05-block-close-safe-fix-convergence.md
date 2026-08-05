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
record_id: 2026-08-05-block-close-safe-fix-convergence
status: completed
completed_at: 2026-08-05 17:13 +08:00
source_plans:
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: block-close-safe-fix-convergence
---

# Block-Close Safe-Fix Convergence

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-05 17:13 +08:00 | 已完成 | `missing_block_close`从parser structured diagnostic到machine-applicable `}` edit、generic LSP code action与v2 apply-edit-rebind闭环；primary保留opening `{`，fix为EOF current lexer cursor；主树集成后GCC/Clang/MSVC的parser 38/38和advanced editor suite均真实exit 0 |

## 已实现契约

- `ZrParser_DiagnosticBuilder_BuildMissingBlockClose`接受独立`fixLocation`：它保留调用方已有opening-brace primary range，并将fix收束为零宽range后发布标题`Insert missing '}'`、replacement为`}`的machine-applicable structured fix。
- shared parser report在recovery前读取current lexer token location。`if (ready) { return 1;`的primary保留opening `{`，fix为EOF cursor；插入`}`后成为语法完整block。
- LSP production consumer未增加 block AST、diagnostic code、message或源码文本分支。既有generic machine-applicable code-action/snapshot pipeline直接消费`diagnostic.fixes[]`。
- 更新后的同一document v2会重新解析与绑定；`if (ready) { return 1;}`不再暴露`missing_block_close`。

## TDD与根因证据

- RED先向parser builder test传入独立fix range；旧三参数API在编译期拒绝第四参数，证明尚未具备primary/fix双range合同。
- GREEN后builder test断言单一fix的title、`}` replacement、machine applicability和zero-width range；LSP integration test经generic action取得同一fix，应用至v2后重新查询，不再存在该diagnostic code。

## 工具链与回归证据

- 隔离分支`codex/lsp-02-block-close-safe-fix`基于已验证的`5be340b`，使用独立static cache。
- GCC 11.4、Clang 14和MSVC 19.44分别构建并直接执行`zr_vm_compiler_semantic_query_diagnostics_test`，均为37 Tests / 0 Failures / 0 Ignored、真实exit 0。
- 三套同样直接执行`zr_vm_language_server_lsp_advanced_editor_features_test`；新增`LSP code action inserts missing block close`均Pass，suite均以`0 failure(s)`结束且真实exit 0。
- advanced suite尾部invalid code-lens及unbound-attribute fixtures会打印预期compiler diagnostics；测试进程在三套工具链仍以exit 0结束，不是本里程碑失败。

## 集成边界

- 隔离叶子已在`ba5da67`、`fefc44e`之后通过`aa2af25`合入主树，并在主树集成基线上由GCC、Clang和MSVC直接复验后转为`completed`。
- 不改变diagnostic、semantic query、artifact/binary、code-action snapshot或JSON schema；不修改LSP production、stdio、CLI或CMake。
- 其他delimiter、replacement、module/property/ownership diagnostics及L3全量registry、性能、cancellation、乱序race证据仍未完成。
