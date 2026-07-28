---
plan_id: lsp-04-debug-and-repl
record_id: 2026-07-28-e2a-formal-expression-fragment-parser
status: completed
completed_at: 2026-07-28 21:30 +08:00
source_plan: docs/plans/lsp/04-debug-and-repl.md
related_code:
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_fragment.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_fragment.c
plan_sources:
  - docs/plans/lsp/04-debug-and-repl.md
  - user: 2026-07-28 optimize semantic inference and record each completed LSP milestone
tests:
  - tests/parser/test_expression_fragment_parser.c
  - docs/parser-and-semantics/expression-fragment-parser.md
doc_type: milestone-record
---

# LSP 04 E2a Formal Expression Fragment Parser

##状态与产出记录

| 完成时间 | 状态 | 完成项目 | 证据 |
|---|---|---|---|
| 2026-07-28 21:30 +08:00 | 已完成 | E2a 发布正式的单表达式 parser 入口，复用既有 precedence parser 且要求 EOS；缺操作数保留结构化诊断，尾随 token 失败而不是接受前缀。 | GCC、Clang、MSVC 均构建并实际运行 `zr_vm_expression_fragment_parser_test`：`3 Tests 0 Failures 0 Ignored`，真实 exit 0。 |

## 已完成计划项

- 新增 `ZrParser_ParseExpressionWithState`。它要求调用方提供已初始化的 parser state，成功时返回 caller-owned AST，失败时返回 `ZR_NULL`。
- 正式入口只调用内部 `parse_expression`，随后检查 `ZR_TK_EOS`。没有 debug grammar、宽松 script expression、`any`、文本回退或 AST 重建分支。
- 缺失右操作数继续通过已有 `report_missing_right_operand` 进入 `structuredErrorCallback`；尾随 token 进入已有 parser error callback。
- 新独立 Unity target 覆盖完整 conditional AST、structured malformed-operand diagnostic 与 trailing-token 拒绝。

## 明确边界

- E2a 只交付 parser 入口，不执行或绑定 debug expression。E2b 必须从 `DebugEvaluationContext` 注入只读 canonical bindings，并调用此入口；不能继续维护 `debug_eval.c` 的局部递归下降语法。
- E1 已满足计划所列的 module、scope、receiver、generic context 和 visible `SymbolId` 前提。source `TypeId`/const-generic substitution 没有现有稳定的 call-frame 或 artifact fact，本项不伪造运行时 carrier；E4 只能传输已经由 canonical facts 发布的 TypeId。
- 本项不修改 `type_inference_import_metadata.c`、property binder、LSP production path 或 Syntax05 Task4 的导入 property 合同。

## Related Documentation

- [Formal expression fragment parser](../../../parser-and-semantics/expression-fragment-parser.md)
- [Debug canonical local bindings](../../../core-runtime/debug-canonical-local-bindings.md)
