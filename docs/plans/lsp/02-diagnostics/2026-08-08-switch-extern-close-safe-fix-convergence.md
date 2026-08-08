---
doc_type: milestone-detail
plan_id: lsp-02-diagnostics
record_id: 2026-08-08-switch-extern-close-safe-fix-convergence
status: completed
completed_at: 2026-08-08 15:35 +08:00
source_plans:
  - docs/plans/lsp/02-diagnostics-and-errors.md
  - docs/plans/lsp/05-implementation-blueprint.md
evidence_scope: switch-extern-close-safe-fix-convergence
---

# Switch/Extern Close Safe-Fix Convergence

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-08 15:35 +08:00 | 已完成 | `missing_switch_case_header_close`、`missing_switch_body_close`和`missing_extern_spec_close`发布唯一 machine-applicable fix；generic LSP code action 应用 edit 后以新文档版本重绑定并清除对应 code。 |

## 已实现契约

- 三项 parser diagnostic 保留既有 recovery primary range，并以其 start 发布零宽 edit：switch case `[21,22] -> [21,21]` 插入`)`，switch body EOF `[35,35] -> [35,35]` 插入`}`，extern spec `[24,25] -> [24,24]` 插入`)`。
- fix 标题分别为 `Insert missing ')'`、`Insert missing '}'`、`Insert missing ')'`；均仅经 structured diagnostic `fixes[]` 发布为 machine applicable。
- LSP production 不新增按 code、message、AST 或源文本的分支。通用 action 直接消费 fact，apply 后 v2 rebind 清除 code。
- `missing_test_name_close` 没有 parser producer，本叶子不伪造 LSP surface；其 parser 语法/报告路径另行处理。

## TDD 与验证

- RED：parser suite 为 `46 Tests / 3 Failures / 0 Ignored`，advanced editor 仅新增三项 code-action 场景失败，原因均为未发布 structured fix。
- GREEN：GCC 11.4、Clang 14 和 MSVC 19.44 均直接执行 parser suite，结果均为 `46 Tests / 0 Failures / 0 Ignored`、真实 exit 0。
- 三套工具链也均直接执行 advanced editor suite，三项新增场景均 Pass，suite 以 `0 failure(s)` 结束且真实 exit 0。
- advanced suite 的 invalid code-lens fixture 会输出预期 compiler diagnostic，但所有三套测试进程均 exit 0。

## 集成边界

- 仅修改 diagnostic builder、parser/LSP tests 和本记录；不改 parser recovery、registry、semantic query、LSP production、stdio、CLI、CMake 或 schema。
- stdio/CLI 全链、其他 delimiter/replacement family、registry 全覆盖与 L3 整体仍未完成。
