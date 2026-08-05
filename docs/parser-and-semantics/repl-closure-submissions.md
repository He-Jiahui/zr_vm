---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_submission.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_session.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_state.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
tests:
  - tests/parser/test_repl_submission_bindings.c
  - tests/cli/test_cli_repl_e2e.c
plan_sources:
  - docs/plans/lsp/04-debug-and-repl.md
  - docs/superpowers/specs/2026-08-04-repl-closure-submission-design.md
doc_type: module-detail
last_verified: 2026-08-05
---

# REPL Closure Submissions

## 目标

REPL 以代际受保护的 closure environment 持久化已提交 cell，而不是拼接或重放旧 source。每次成功提交都从当前环境派生一个 successor closure；只有成功执行后才发布 successor。失败提交不改变环境，已经发生的普通运行时副作用不做事务回滚。

## Canonical Submission Contract

`ZrParser_Source_CompileSubmission` 接收借用的 `SZrParserSubmissionContext`，并输出拥有的 `SZrParserSubmissionResult`。输入和输出 binding 都保留以下结构化 identity：

- `SymbolId`、`TypeId`、`PlaceId` 与完整 declaration range。
- capture index、module generation、environment generation 与 cell generation。
- binding kind、完整 inferred type，以及 callable 的 formal signature。

编译器只接受 generation 与 context 精确匹配的既有 binding。空 identity、过期 generation、重复 identity、同名冲突、丢失 callable signature 或不完整 capture 都 fail closed。callable 解析只消费 formal signature 和 canonical identity，不按名称、slot、AST、显示文本或重放 source 回退。

顶层变量与 callable 声明编译为既有 closure 的 `SET_CLOSURE` 槽；跨 cell 读取使用 `GET_CLOSURE`。因此后续 cell 的赋值更新的是同一个有效 capture，不会创建以同名文本推断出来的替代 binding。

## 生命周期与 GC 根

CLI session 持有一个 global、环境 closure 的 `SZrGcRootHandle` 与 source-label root。执行下一 cell 前从根重新解析 closure 和 source label，因此 compacting GC 后不会持有过期裸指针。提交成功时替换 environment root；`:reset` 释放 root、清空 binding table，并推进 environment generation。

`:type` 使用同一 canonical submission context 编译受控查询，不执行 cell，也不发布 successor closure。它不能借由临时 type 查询改变 REPL 的 environment。

## 跨 Cell Ownership 边界

持久化策略由结构化 inferred type 和 ownership qualifier 决定：

- 普通值、GC 值、`Unique` move、`Shared` copy 与 `Weak` copy 可以按各自 runtime ownership 规则进入 successor closure。
- `ref`/`ref readonly`、ref-like、`PoolRef`、borrowed 或 loaned value 一律拒绝跨 cell。
- 当前 active loan、不可持久化 callable/capture 或任何 identity 不一致同样拒绝。

这条边界不按类型名、错误 message 或 source 文本判断；REPL 不能通过持久 ref 绕过已有 borrow checker。

## Parser Surface

REPL 控制器只负责 input buffering、命令分派和受控 `return <expression>;` wrapper。普通 source grammar 不启用 ASI：cell 中的 simple statement 仍必须有 `;`。不完整输入和缺少分号都保留正式 parser 的结构化诊断，并且不会发布新 generation。

## 验证

`test_repl_submission_bindings` 覆盖 capture identity、callable signature、assignment、stale/duplicate identity、ref-like/loaned 拒绝和 owner policy。`test_cli_repl_e2e` 覆盖跨 cell 值与 callable、`:type`、`:reset`、semicolon 边界和失败后环境保持。三工具链还执行语义事实、reference escape 与 semantic query 回归，以及当前 LSP/CLI stdio smoke。
