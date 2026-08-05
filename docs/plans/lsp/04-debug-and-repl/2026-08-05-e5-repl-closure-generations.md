---
plan_id: lsp-04-debug-and-repl
milestone: E5
status: completed
completed_at: 2026-08-05 12:18 +08:00
baseline_revision: 8986a39
source_plans:
  - docs/plans/lsp/04-debug-and-repl.md
  - docs/superpowers/specs/2026-08-04-repl-closure-submission-design.md
implementation:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_submission.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_session.c
tests:
  - tests/parser/test_repl_submission_bindings.c
  - tests/cli/test_cli_repl_e2e.c
record_type: milestone-acceptance
---

# E5 REPL Closure Generations

## 状态与产出记录

| 完成时间 | 状态 | 完成项目 |
|---|---|---|
| 2026-08-05 12:18 +08:00 | 已完成 | E5 REPL generation-checked closure environment、canonical submission binding、ownership fail-closed gate、controlled semicolon surface 与 CLI session root 生命周期。 |

## 完成项目

1. 发布 `ZrParser_Source_CompileSubmission` 及拥有结果的 submission contract。既有 binding 以精确 `SymbolId`、`TypeId`、`PlaceId`、declaration range、capture index 和 module/environment/cell generation 注入正常 compiler pipeline。
2. 使用 `GET_CLOSURE`/`SET_CLOSURE` 将变量与 callable 持久化到 successor closure。callable 保留 formal signature；缺失、stale、重复 identity 或名称冲突一律拒绝，不按名称、slot、AST 或文本回退。
3. CLI session 使用 global root、environment closure root 和 source-label root。成功 cell 才替换 rooted environment；`:reset` 释放并推进 generation；`:type` 只做 canonical query，不执行或发布环境。
4. 按 inferred type/ownership qualifier 施行持久化政策：value、GC、`Unique`、`Shared`、`Weak` 按结构化规则处理；`ref`、ref-like、`PoolRef`、borrowed 与 loaned value 均不能跨 cell/frame。
5. REPL 只保留受控 expression wrapper。语言 parser 不启用 ASI；缺少 simple-statement semicolon 的 cell 失败且环境不变。

## 验收证据

- GCC、Clang、MSVC 的 focused suite 均真实 exit 0：`repl_submission_bindings` 11/11、semantic facts 12/12、reference escape closure/suspension 13/13、semantic query 27/27，以及 CLI REPL end-to-end。
- GCC、Clang、MSVC 均完成当前 `zr_vm_language_server_stdio`、descriptor fixture 与 CLI 的 stdio smoke，真实 exit 0。
- MSVC 使用 `VSCMD_VER=17.14.36`。验证中现有外部 LSP/stdio 工作树修改可被构建消费，但不属于本 E5 exact commit。

## 边界

本记录只关闭 LSP 04 的 E5 REPL generations。LSP 语义推断总计划及 E4 children-handle audit 等后续项仍为进行中，后续 consumer 只能消费已经发布的 canonical facts。
