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
record_id: 2026-08-05-declaration-body-close-safe-fix-convergence
status: completed
completed_at: 2026-08-05 14:14 +08:00
source_plans:
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: declaration-body-close-safe-fix-convergence
---

# Declaration-Body Close Safe-Fix Convergence

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-05 14:14 +08:00 | 已完成 | `missing_declaration_body_close`从parser structured diagnostic到machine-applicable `}` edit、generic LSP code action与v2 apply-edit-rebind闭环；primary 固定为 declaration body opening `{`，fix 固定为recovery EOF lexer cursor；GCC/Clang/MSVC的parser 34/34和advanced editor suite均真实exit 0 |

## 已实现契约

- `ZrParser_DiagnosticBuilder_BuildMissingDeclarationBodyClose`分离primary与fix location；它保留调用方的primary range，并将fix收束为零宽range后发布标题为`Insert missing '}'`、文本为`}`的machine-applicable structured fix。
- declaration parser在recovery前捕获current lexer token location。对无尾随newline的`class Box {\n    var id: int;`，primary 是opening `{`，fix 是第二行末尾的零宽EOF cursor。
- LSP production consumer未增加 declaration-kind、message、AST或源码文本分支。既有generic machine-applicable code-action/snapshot pipeline直接消费`diagnostic.fixes[]`。
- 更新后的同一document version会重新解析与绑定；fixed text不再暴露`missing_declaration_body_close`。

## TDD与根因证据

- RED先向parser builder test传入独立fix range；旧四参数API在编译期拒绝第五参数，证明尚未具备primary/fix双range合同。
- GREEN后builder test断言单一fix的title、replacement、machine applicability和EOF零宽range；primary继续断言opening `{`。
- LSP integration test经通用action取得该fix，验证edit range、文本和title；应用至v2后重新查询，不再存在该diagnostic code。

## 工具链与回归证据

- GCC 11.4、Clang 14和MSVC 19.44各以独立static build cache构建并直接执行`zr_vm_compiler_semantic_query_diagnostics_test`，均为34 Tests / 0 Failures / 0 Ignored、真实exit 0。
- 三套同样直接执行`zr_vm_language_server_lsp_advanced_editor_features_test`；新增`LSP code action inserts missing declaration body close`均Pass，suite均以`0 failure(s)`结束且真实exit 0。
- advanced suite尾部的invalid code-lens fixture会打印预期compiler diagnostic；该负例在所有工具链中测试进程仍以exit 0结束，不是本里程碑失败。

## Snapshot、Schema与协议边界

- 不改变diagnostic、semantic query、artifact/binary、code-action snapshot或JSON schema。改动只使用既有`fixes[]`和machine-applicable applicability。
- 本里程碑不修改LSP production、stdio、CLI或CMake；它们保持既有generic consumer合同。

## 未完成边界

- 未运行stdio/CLI全链smoke，原因是本次未改变其consumer；这些验收以及registry-wide coverage仍由后续LSP 02/L3里程碑覆盖。
- 其他delimiter、replacement、module/property/ownership diagnostics和性能、cancellation、乱序race证据未随此局部fix完成。
