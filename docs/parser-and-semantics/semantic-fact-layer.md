---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_values.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_binding_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_internal.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_extern.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_constant_eval.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_constant_eval.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_completion_semantic_facts.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_inlay_hints.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_signature_semantic_facts.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
  - zr_vm_language_server/stdio/stdio_inline_value.c
  - zr_vm_language_server/stdio/stdio_inline_value_scan.h
  - zr_vm_language_server/stdio/stdio_inline_value_scan.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_expression_text.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_expression_text.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_constant_condition.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_reachability.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_expression_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_references.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/CMakeLists.txt
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/value.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_child_shape.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval_diagnostics.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval_internal.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_facts.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_reference_summary.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_input_scan.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_input_scan.h
  - zr_vm_cli/src/zr_vm_cli/repl/repl_semantic_facts.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_semantic_facts.h
  - zr_vm_cli/src/zr_vm_cli/repl/repl_semantic_fact_walkers.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_semantic_expression_walk.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_semantic_expression_walk.h
  - tests/parser/test_semantic_facts.c
  - tests/parser/test_expression_fact_emission.c
  - tests/parser/test_reference_fact_emission.c
  - tests/parser/test_logical_fact_emission.c
  - tests/parser/test_object_literal_key_lowering.c
  - tests/parser/test_instruction_execution.c
  - tests/parser/test_parser.c
  - tests/parser/test_type_inference.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_ownership_diagnostics.c
  - tests/language_server/test_lsp_inlay_semantic_facts.c
  - tests/language_server/test_lsp_logical_semantic_query.c
  - tests/language_server/test_lsp_local_semantic_query.c
  - tests/language_server/test_lsp_local_semantic_hover.c
  - tests/language_server/test_lsp_computed_member_hover.c
  - tests/language_server/test_lsp_expression_fact_hover.c
  - tests/language_server/test_lsp_parser_diagnostics.c
  - tests/language_server/test_lsp_declaration_parser_diagnostics.c
  - tests/language_server/test_lsp_statement_parser_diagnostics.c
  - tests/language_server/test_lsp_reachability_semantic_query.c
  - tests/language_server/test_lsp_call_member_semantic_query.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/stdio_smoke.js
  - tests/language_server/stdio_inline_value_semantic_smoke.js
  - tests/debug/test_debug_expression_diagnostics.c
  - tests/debug/test_debug_variable_child_shape.c
  - tests/cli/test_cli_repl_e2e.c
  - tests/cli/repl_expression_aggregate_smoke.js
  - tests/cli/repl_expression_array_display_smoke.js
  - tests/cli/repl_expression_object_smoke.js
  - tests/cli/repl_type_call_reference_smoke.js
  - tests/cli/repl_type_member_receiver_fact_smoke.js
  - tests/cli/repl_type_logical_comparison_smoke.js
  - tests/CMakeLists.txt
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/include/zr_vm_parser/type_system.h
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_values.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_binding_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_internal.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_extern.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_literals.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_constant_eval.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_constant_eval.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_completion_semantic_facts.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_inlay_hints.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_signature_semantic_facts.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
  - zr_vm_language_server/stdio/stdio_inline_value.c
  - zr_vm_language_server/stdio/stdio_inline_value_scan.h
  - zr_vm_language_server/stdio/stdio_inline_value_scan.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_expression_text.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_expression_text.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_constant_condition.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_reachability.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_expression_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_references.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_language_server/CMakeLists.txt
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/value.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_child_shape.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval_diagnostics.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval_internal.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_internal.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_semantic_facts.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_reference_summary.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_input_scan.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_input_scan.h
  - zr_vm_cli/src/zr_vm_cli/repl/repl_semantic_fact_walkers.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_semantic_expression_walk.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_semantic_expression_walk.h
  - tests/parser/test_instruction_execution.c
  - tests/parser/test_parser.c
  - tests/parser/test_expression_fact_emission.c
  - tests/parser/test_reference_fact_emission.c
  - tests/parser/test_logical_fact_emission.c
  - tests/parser/test_object_literal_key_lowering.c
  - tests/debug/test_debug_expression_diagnostics.c
  - tests/debug/test_debug_variable_child_shape.c
  - tests/cli/test_cli_repl_e2e.c
  - tests/cli/repl_expression_aggregate_smoke.js
  - tests/cli/repl_expression_array_display_smoke.js
  - tests/cli/repl_expression_object_smoke.js
  - tests/language_server/test_ownership_diagnostics.c
  - tests/language_server/test_lsp_inlay_semantic_facts.c
  - tests/language_server/test_lsp_logical_semantic_query.c
  - tests/language_server/test_lsp_local_semantic_query.c
  - tests/language_server/test_lsp_local_semantic_hover.c
  - tests/language_server/test_lsp_computed_member_hover.c
  - tests/language_server/test_lsp_expression_fact_hover.c
  - tests/language_server/test_lsp_parser_diagnostics.c
  - tests/language_server/test_lsp_declaration_parser_diagnostics.c
  - tests/language_server/test_lsp_statement_parser_diagnostics.c
  - tests/language_server/test_lsp_call_member_semantic_query.c
  - tests/CMakeLists.txt
plan_sources:
  - user: 2026-06-03 语义推断、诊断、LSP、Debug、REPL 能力增强目标
  - docs/superpowers/specs/2026-06-03-zr-vm-semantic-inference-design.md
  - docs/superpowers/plans/2026-06-03-zr-vm-semantic-inference-fact-layer.md
  - .codex/plans/ZR_LSP 现代能力对齐计划.md
  - .codex/plans/Rust-First using  Ownership 语义收敛计划.md
  - docs/plans/using/01-ownership-as-generics.md
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_semantic_facts.c
  - tests/parser/test_expression_fact_emission.c
  - tests/parser/test_reference_fact_emission.c
  - tests/parser/test_logical_fact_emission.c
  - tests/parser/test_object_literal_key_lowering.c
  - tests/parser/test_type_inference.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_ownership_diagnostics.c
  - tests/language_server/test_lsp_inlay_semantic_facts.c
  - tests/language_server/test_lsp_logical_semantic_query.c
  - tests/language_server/test_lsp_local_semantic_query.c
  - tests/language_server/test_lsp_local_semantic_hover.c
  - tests/language_server/test_lsp_computed_member_hover.c
  - tests/language_server/test_lsp_expression_fact_hover.c
  - tests/language_server/test_lsp_parser_diagnostics.c
  - tests/language_server/test_lsp_declaration_parser_diagnostics.c
  - tests/language_server/test_lsp_statement_parser_diagnostics.c
  - tests/language_server/test_lsp_call_member_semantic_query.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/stdio_smoke.js
  - tests/language_server/stdio_inline_value_semantic_smoke.js
  - tests/debug/test_debug_expression_diagnostics.c
  - tests/debug/test_debug_variable_child_shape.c
  - tests/cli/test_cli_repl_e2e.c
  - tests/cli/repl_expression_aggregate_smoke.js
  - tests/cli/repl_expression_array_display_smoke.js
  - tests/cli/repl_expression_object_smoke.js
  - tests/cli/repl_type_expression_fact_smoke.js
  - tests/cli/repl_type_ownership_smoke.js
  - tests/cli/repl_type_call_member_smoke.js
  - tests/cli/repl_type_member_receiver_fact_smoke.js
  - tests/cli/repl_type_logical_comparison_smoke.js
  - tests/cli/repl_type_reachability_smoke.js
  - tests/acceptance/2026-06-03-semantic-inference-fact-layer.md
  - tests/acceptance/2026-06-17-ownership-generics-p1.md
doc_type: module-detail
---

# Semantic Fact Layer

`SZrSemanticContext` 是编译期语义事实的唯一共享容器。parser、type inference、semantic analyzer 负责把表达式类型、引用、数值、可达性、逻辑和所有权事实写入这里；LSP 已开始查询这些事实，Debug 和 REPL 已开始接入同一诊断/表达式方向，完整事实复用仍在后续推进，避免各自复制一套局部推断规则。

当前阶段建立容器和查询契约，并已开始从 type inference 写入表达式、数值、确定性逻辑和 ownership builtin 事实，从 LSP semantic analyzer 写入声明/使用引用事实、不可达事实、编辑器短路/分支逻辑事实和所有权违规事实。parser 也开始把代表性语法错误转换成结构化诊断，再通过 LSP 展示具体原因和建议。LSP 局部查询已接入表达式事实、数值事实、引用事实、诊断失败和显式 unknown 分流；hover/rich hover 现在也能把 expression kind/exactness/constant 和调用/成员 expression payload 作为 `Expression:` / escaped `Constant:` / `Call:` / `Member:` 行及稳定 role 暴露给编辑器，并能在 `seed[index]` 的 `[` 这类非 identifier 位置通过 member-access reference fact 恢复 computed member payload。Stdio inline values 也复用同一 local semantic query，覆盖 local initializer、简单 return、简单 line-start expression statement（包括 `!true;` / `-42;` 这类 unary 起始形式，`pick(42);` / `seed.value;` / `seed[index];` 这类 call/member 起始形式，以及 `[1 + 2];` / `[true || false];` 这类 array literal aggregate 起始形式、`{[1 + 2]: 4};`、`{a: 1 + 2};`、`{\n a: 1 + 2\n};` 这类 object aggregate 起始形式）、多行 return expression、return 后换行开始的 expression、等号后换行的 local initializer，以及 request range 从 continuation line 开始的简单 operator-split expression statement 的 fact-backed `InlineValueText`；`stdio_inline_value_scan.c/.h` 只定位 expression range 和 query token，事实仍由 `ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt` 提供。当同一 query 返回 reference fact 时，inline value 会追加 `reference ...`，例如 computed member read 的 `reference member access`，array/object aggregate statement 则只显示共享 nested element/operator facts，例如 `range 3..3` 或 `logical true, short-circuits`。Debug 和 REPL 已开始以安全局部方式消费诊断和表达式能力；Debug 数据面在协议边界报告 value preview / evaluate result 的 named/indexed child shape，帮助 adapter 做局部展开判断，并且成功 `evaluate` 结果会通过 `debug_semantic_facts.c` 追加 parser/type-inference 已证明的 expression/numeric/logical/reachability facts、expression payloads 以及已有 reference facts，例如 computed member evaluate 会显示 `member index` / `reference member access index`，当前暂停帧里的 `inside + 1` 会在临时 type environment replay 后显示 parser-owned `reference read inside`，稳定 Debug global 如 `zr` 会显示 parser-owned `reference read zr`，编译入口函数的 top-level callable metadata 会注册进临时 type environment，所以 `pick(1 + 2)` 这类 Debug semantic-summary 查询可以追加 parser-owned `call pick args=1`、`reference call pick` 以及实参表达式的折叠事实。REPL 的 `:type <expression>` 已是第一个命令行事实消费者：它直接调用 parser/type inference，并能在同一 REPL 会话中复用 prior declaration 源码上下文来输出同一 `SZrSemanticContext` 里的数值/逻辑/所有权事实、调用/成员 expression payload、member-access reference facts 和 member-write reference facts。REPL 的普通裸表达式执行也接受完整的 aggregate-start 表达式，例如 `[1 + 2][0]` 会经内部 expression wrapping 执行并打印 `3`，`[1 + 2]` 会通过 shared array value stringification 打印 `[3]`；该路径依赖 parser/compiler/runtime 正常创建数组字面量和 `ZrCore_Value_ConvertToString` 正常展示数组，而不是走 `:type` 的 compile-time fact display。更完整的 Debug write/user-global/imported/generic-call reference parity、长期 REPL runtime cell 作用域和跨工具共享查询 parity 仍在后续任务中接入。

REPL 的普通裸表达式执行现在也覆盖行首对象字面量。`repl_input_scan.c` 会把空对象、标识符/字符串键后跟 `:`、或计算键 `[...]` 后跟 `:` 的 `{...}` 识别为对象表达式，所以 `{a: 1 + 2}.a` 和 `{[1 + 2]: 4}[3]` 会走和数组 aggregate-start 表达式相同的内部 `return <expr>;` 路径；普通 `{ var ... }` / `{ if ... }` 块仍留给 parser 语句路径。

Debug runtime snapshot 还会在协议层报告 `semanticSummary` / `referenceSummary`。`semanticSummary` 保留 runtime value preview 摘要，同时对成功的 `evaluate` 表达式追加 parser/type-inference 能证明的共享事实；例如 `evaluate("1 + 2")` 会在 `integer 3` 后追加 `expression binary exact`、`constant 3`、`range 3..3` 和 `unsigned range 3..3`，`evaluate("true || false")` 会追加 `short-circuits` 与 `unreachable because short-circuit skips evaluation`，`evaluate("true ? 1 : 2")` 会追加 constant-branch skipped-path reachability，字符串常量会按 REPL/LSP 相同规则转义 quote、backslash、`\n`、`\t`、`\r`、`\b`、`\f` 和其它控制字节，例如追加 `constant "a\"b\\c\n\t"` 形式的单行摘要，`evaluate("zr[1]")` 还会追加 parser-owned `reference member access`，`evaluate("inside + 1")` 会在暂停帧有可见 `inside` slot 时追加 parser-owned `reference read inside`，`evaluate("zr")` 会为稳定 Debug global 追加 parser-owned `reference read zr`。若 Debug agent 有编译入口函数，`debug_semantic_facts.c` 还会把该 entry function 的 top-level callable bindings、child function return type、parameter metadata 注册为临时 parser 函数签名；因此语义摘要查询 `pick(1 + 2)` 可以复用 parser/type inference 记录 `call pick args=1`、`reference call pick`、`expression binary exact`、`constant 3`、`range 3..3` 和 `unsigned range 3..3`，`seed[index]` 也可显示 `member index`、`reference member access index` 和索引 token 的 `reference read index`，但这仍不让 safe evaluator 执行函数调用或写入。 这条桥接由 `debug_semantic_facts.c` 负责：它把 evaluate 源码解析成临时表达式语句；若存在暂停帧，会先用可见 frame slots 的 runtime value type 填充临时 parser type environment，并注册稳定 Debug globals（`loadedModules`、`zr`、当前异常存在时的 `error`）和普通入口 callable metadata，再调用 `ZrParser_ExpressionType_Infer`，最后只格式化已有 `SZrSemanticExpressionFact` / `SZrSemanticNumericFact` / `SZrSemanticReferenceFact` / `SZrSemanticLogicalFact` / `SZrSemanticReachabilityFact` / `SZrSemanticOwnershipFact`；Debug safe evaluator 不复制类型推断、常量折叠、范围推断、引用分类、分支语义推断、调用/成员 payload 推断、unsigned range payload 推断或字符串常量转义策略。`referenceSummary` 现在能覆盖稳定顶层 scope 引用、简单 identifier evaluate、`inside + 1` 这类复合 safe-evaluate 表达式里的实际读取集合、条件表达式中 condition 和选中分支的实际读取、debug index-window base expression（例如 `evaluate("zr[1..3]")` 的 indexed-window result 保留 `global zr`），以及普通 postfix safe-evaluate 里的成功索引/成员读取（例如 `evaluate("zr[1]")` 报告 `global zr, index access`，成功 member 读取追加 `member <name>`）。短路 RHS、未选中的 `?:` branch 和 index-window bounds 只做语法/数值消费，不会因为其中出现可解析 identifier 就填充引用摘要；它仍是 adapter-facing runtime metadata，不是 parser-owned reference fact，也不声明 member declaration resolution。

Debug semantic-summary walker 现在会先按 AST 节点直接查询 `ZR_SEMANTIC_REFERENCE_WRITE` 和 `ZR_SEMANTIC_REFERENCE_MEMBER_WRITE`，再进入普通 read/member-access/position 查询。这样 `globalSeed = 3`、`seed.value = 3`、`seed[index] = 4` 这类 assignment 摘要不会因为同一节点或子节点还有 read/member-access fact 而丢失 write 事实；若普通 position 查询因优先级返回 member-access，walker 只对 identifier 节点使用 `ZrParser_SemanticFacts_FindReferenceAtPositionByKind(..., ZR_SEMANTIC_REFERENCE_READ)` 恢复同位置的 read fact，例如 `seed[index]` 中索引 token 的 `reference read index`。协议层仍只展示 parser 已写入 `SZrSemanticContext` 的事实，不在 Debug 层推断成员声明或写入语义。

## Fact Families

- `expressionFacts`: 表达式节点、源码范围、推断类型、精确度、常量值、调用目标 token、成员 token 和失败提示。
- `referenceFacts`: 声明和使用之间的符号关系、声明范围、使用范围、引用类型和解析状态。
- `numericFacts`: 数值表达式的来源类型、目标类型、范围约束和潜在溢出状态。
- `reachabilityFacts`: 语句或表达式范围的可达状态，以及 return/throw/break/continue/常量条件等原因。
- `logicalFacts`: 常量布尔、数值常量比较、组合布尔表达式、恒真/恒假和短路推断结果。
- `ownershipFacts`: `%unique/%shared/%borrowed/%weak` 等所有权限定、生命周期区域、相关节点和违规状态。

## Lifecycle

`semantic.c` 仍然负责创建和销毁 `SZrSemanticContext`，但事实数组的细节由 `semantic_facts.c` 管理：

- `ZrParser_SemanticFacts_Init` 初始化六类 `SZrArray`。
- `ZrParser_SemanticFacts_Reset` 清空事实数组，并释放表达式事实里深拷贝的 `SZrInferredType`。
- `ZrParser_SemanticFacts_Free` 先 reset，再释放数组内存。

表达式事实追加时会深拷贝 `SZrInferredType`，并复制调用目标、成员名和诊断文本等字符串 payload。调用方可以释放自己的临时类型，事实层保留独立副本，避免 LSP 或 Debug 查询到悬空类型。

## Type Inference Emission

`type_inference_semantic_facts.c` 负责把 AST 节点和 `SZrInferredType` 转换为共享事实，避免继续扩大已经很大的 type inference orchestration 文件：

- `type_inference_constant_eval.c` 是同目录内的窄 helper，负责从 AST 证明整数字面量、浮点数值表达式、数值常量比较、logical-not 和组合 `&&` / `||` 的常量值。`type_inference_semantic_facts.c` 只消费这些结果来写 facts，避免把常量求值责任继续堆进事实发射文件。
- `type_inference_record_expression_fact` 写入表达式 kind、源码范围、精确推断类型、值类别、字面量常量、调用目标 payload 和成员 payload。
- `type_inference_record_numeric_fact` 只为数值结果写入来源类型、目标类型、范围约束和溢出状态。
- `type_inference_record_expression_and_numeric_facts` 是表达式入口的组合 helper，并带按节点去重保护。它也会为 deterministic boolean/logical expression 写入 parser-owned `logicalFacts`，并为可证明短路的 skipped operand 写入 parser-owned `reachabilityFacts`。布尔常量求值现在会递归穿过数值常量比较、logical-not、以及可证明结果的 `&&` / `||`，让 REPL 和后续 Debug 查询不必依赖 LSP semantic analyzer 才能解释常量布尔、短路和 skipped-branch 可达性。
- `type_inference_record_primary_call_reference_fact` 在 overload resolution 成功后为 primary function call 写入 resolved `ZR_SEMANTIC_REFERENCE_CALL`。它复用 expression payload 的 callee-token 定位逻辑，但绑定到刚解析成功的具体 call node；事实携带 callee range、declaration range、function symbol/type id 和函数名。
- `type_inference_record_identifier_write_reference_fact` 为赋值左侧已解析 identifier 写入 `ZR_SEMANTIC_REFERENCE_WRITE`。assignment inference 直接从 type environment 读取绑定类型，保留 left identifier expression/numeric facts，但不再让左侧 token 走普通 identifier read-reference 路径。
- `type_inference_record_member_access_reference_fact` 为成功推断的成员读取写入 unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS`。`seed.value` 的 fact range 对准 `value`；`seed[index]` 的 computed member-access fact 使用完整 member expression range，并在写入前 materialize index expression facts。reference position query 先按更窄 range 选择事实，因此光标在 `index` token 上仍返回索引变量 read，光标在 `[` 或 wider member expression 范围内返回 member-access fact。
- `type_inference_record_member_write_reference_fact` 为非 identifier 赋值左值中的最后一个 member/index token 写入 unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_WRITE`。`seed.value = 3` 的事实 range 对准 `value`，`seed[index] = 4` 的事实 range 对准 computed index 表达式；当前只声明 assignment-target token 分类，不声明成员声明解析。reference position query 会在同一 token 上优先返回 write/member-write，因此 `seed.value = 3` 不会被新增的 member-access fact 误报为读取。
- `type_inference_record_ownership_builtin_fact` 记录 `%borrow/%loan/%shared/%weak/%release/%detach` 这类 ownership builtin 的动作、结果 qualifier 和相关 operand 节点，让 REPL/LSP 可以读共享事实，而不是根据类型字符串重猜所有权语义。

当前已验证的发射范围：

- 整数字面量经 `ZrParser_ExpressionType_Infer` 后产生 `ZR_SEMANTIC_EXPRESSION_FACT_LITERAL` 和 `ZR_SEMANTIC_NUMERIC_FACT_LITERAL`。
- 已注册 identifier 经 `ZrParser_ExpressionType_Infer` 后产生 `ZR_SEMANTIC_EXPRESSION_FACT_IDENTIFIER`，并在类型带范围约束时写入 `ZR_SEMANTIC_NUMERIC_FACT_RANGE`。`seed` 注册为 exact `7..7` 后，identifier fact 与 numeric range fact 都会落在同一 token。
- 二元算术表达式经 `ZrParser_ExpressionType_Infer` 后产生 `ZR_SEMANTIC_EXPRESSION_FACT_BINARY` 和 `ZR_SEMANTIC_NUMERIC_FACT_PROMOTION`。
- 复合表达式 AST 范围现在由左右子表达式范围合并得到，二元、逻辑、条件和赋值表达式都会覆盖 operator gap。`true || false` 的逻辑表达式范围覆盖 `0..13`，因此 `||` token 位置可以稳定命中整个逻辑表达式，而不是漂移到右侧字面量。
- `ZR_AST_LOGICAL_EXPRESSION` 现在通过 type inference 推断为 `bool`，成功出口会记录 `ZR_SEMANTIC_EXPRESSION_FACT_BINARY`、`ZR_SEMANTIC_VALUE_KIND_BOOL` 和 exact `bool` inferred type。parser/type inference 还会为 `true`、`false`、左右均为布尔常量的逻辑表达式，以及 `true || rhs`、`false && rhs` 这类确定短路表达式写入 `logicalFacts`。短路分析同时在 skipped operand 范围写入 `ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT`，`causeNode` 指向证明短路的左操作数。非短路但结果已知的组合逻辑表达式，例如 `(1 < 2) && (3 < 4)`，会在完整 logical expression 范围写入 `ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE` 并让 expression fact 携带 `hasConstant=true`。logical fact 与 expression fact 使用完整逻辑表达式范围，reachability fact 使用被跳过的右操作数范围，便于 LSP 在 operator/RHS 位置拿到类型和控制流解释，也让 REPL `:type true || false` 可以直接报告逻辑值、短路流和 skipped operand 不可达原因。
- 常量数值比较二元表达式现在也会在 type inference 成功出口记录 exact bool 常量和 parser-owned logical fact。`1 < 2` 的 expression fact 是 `ZR_SEMANTIC_EXPRESSION_FACT_BINARY`、exact `bool`、`hasConstant=true`、`constantValue=true`；同一表达式范围写入 `ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE`。`3 <= 2` 对应 `ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE`。这些比较结果也可作为外层 logical-not 或非短路逻辑表达式的布尔常量来源：`!(1 < 2)` 记录 false 常量，`(1 < 2) && (3 < 4)` 记录 true 常量。LSP local query 在比较或 `&&` operator token 上会返回 expression fact 和 logical fact，REPL `:type 1 < 2`、`:type !(1 < 2)` 和 `:type (1 < 2) && (3 < 4)` 会从同一 `SZrSemanticContext` 输出对应 `Logical value`，不执行表达式。
- `ZR_AST_UNARY_EXPRESSION` 的 logical-not `!` 现在对布尔常量 operand 写入 exact logical fact。`!true` 和 `!(1 < 2)` 都记录 `ZR_SEMANTIC_EXPRESSION_FACT_UNARY`、exact `bool` inferred type、`hasConstant=true`，并把 `relatedNode` 指向 operand。unary expression range 由 operator token 和 operand range 合并，`!true` 覆盖 `0..5`，所以 LSP local query 在 `!` token 位置会 materialize unary expression fact，而不是漂移到 `true` 字面量。
- `ZrParser_ExpressionType_Infer` 的成功推断出口已集中调用 `type_inference_record_expression_and_numeric_facts`。因此函数调用/primary/member 这类非字面量、非二元表达式也会写入 expression fact；`pick(42)` 的 initializer 当前会以 parser 的 primary/member 表达式节点记录 exact `int64` fact。
- primary/member expression facts 现在会记录调用和成员 token payload。`pick(42)` 记录 `hasCallInfo=true`、`callTargetName=pick`、`callTargetRange` 和实参数量；`seed.value` 记录 `hasMemberInfo=true`、`memberName=value`、`memberRange` 和 computed 标记。成员调用会同时标记 `isMemberCall=true` 并保留成员 token。
- 已解析的 primary function call 现在也会记录 call reference fact。`pick(value: int): int { ... }` 通过 `ZrParser_TypeEnvironment_RegisterFunctionEx` 注册后，`pick(42)` 推断成功会在 `pick` call target token 上写入 `ZR_SEMANTIC_REFERENCE_CALL`，并指回函数声明名 token，即使声明从文件 offset 0 开始也会保留有效 declaration range。
- assignment expression inference 已验证会记录 `ZR_SEMANTIC_EXPRESSION_FACT_ASSIGNMENT`，同时保留 left identifier fact 和 right literal fact。`seed = 3` 的 assignment fact 使用 right-hand type 作为表达式结果，并通过 left-hand type 做兼容性检查。已解析 identifier 左值还会在左侧 `seed` token 写入 `ZR_SEMANTIC_REFERENCE_WRITE`，指回声明名 token，并复用同一 symbol/type id；该 token 不再被误分类为 read reference。
- ordinary dot-member reads 现在也会记录 member-access reference facts。`seed.value` 在 `value` token 上写入 unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS`；LSP local reference query 可以返回该 fact，REPL `:type seed.value` 会在 `Member: value` 之外显示 `Reference: member value`。
- member/index assignment target inference 现在也会记录 assignment-target reference facts。`seed.value = 3` 在 `value` token 上写入 unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_WRITE`，`seed[index] = 4` 在 `index` expression range 上写入同类事实；LSP local query 和 hover/rich hover 可以显示 `Reference: member write` 与 symbol token，REPL `:type seed.value = 3` 也会显示 `Reference: member write value`。这些 facts 的 declaration range 仍保持本 token，`isResolved=false`，因此 REPL 不会伪造成员声明位置。
- computed member read hover 现在也通过同一 reference/expression fact 桥接。`seed[index]` 在 `[` 位置的 local query 会用 wide `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS` 找回 member payload，plain hover 显示 `Reference: member access`、`Symbol: index`、`Member: index`，rich hover 暴露稳定 `reference`、`symbol`、`member` roles；`index` token 本身仍保留 resolved read-reference 查询合同。
- array literal inference 已验证会访问 element expressions，让 `[1 + 2]` 这类 nested element 也 materialize 自己的 binary expression fact 和 exact `3..3` numeric promotion fact，同时 array literal 本身记录 `ZR_SEMANTIC_EXPRESSION_FACT_ARRAY`。
- object literal inference 现在会 best-effort 访问每个 key/value pair 的 value expression，让 `{a: 1 + 2}` 这类嵌套 property value 也 materialize 自己的 `ZR_SEMANTIC_EXPRESSION_FACT_BINARY` 和 `ZR_SEMANTIC_NUMERIC_FACT_PROMOTION`。parser 在 `SZrKeyValuePair.keyIsComputed` 上记录 key 是否来自 `[expr]`；type inference 只对 computed key 做 best-effort key expression 推断，所以 `{[1 + 2]: 4}` 会 materialize key 上的 binary expression fact 和 exact `3..3` numeric promotion fact，而 `{a: ...}` 的 `a` 仍是 property name，不会被误当作 identifier use。object literal 本身仍按原合同推断为 `object`，property key/value 推断失败时不把整个 object literal 改成失败。
- object literal lowering、compile-time evaluation、compile-time binding metadata、class analysis 和 task-effect scanning 都复用 `keyIsComputed` 区分静态 property name 与 computed key expression。`{[key]: 1, name: 2}` 必须同时产生 indexed assignment 和 member assignment；plain identifier/string keys 保持字面 property name 合同。
- lambda expression inference 已验证会记录 `ZR_SEMANTIC_EXPRESSION_FACT_LAMBDA` 和 callable closure type，例如 `(x:int)->{ return x; }` 记录 `%func(int)->int`。
- ownership builtin expression inference 已验证会同时记录 expression fact 和 ownership fact。例如 `%borrow(owner)` 在 `owner` 为 `%unique int` 时记录 `ZR_SEMANTIC_EXPRESSION_FACT_OWNERSHIP_BUILTIN`、borrowed result qualifier，以及 `ZR_SEMANTIC_OWNERSHIP_FACT_BORROW`。
- 数值事实仍保持更窄边界：只有已明确标记为 literal、promotion 或 conversion 的数值结果会写入 `numericFacts`。函数调用/primary/member 表达式即使推断为数值返回类型，也不会伪造数值来源或范围事实。
- 整数常量数值事实现在会从 AST 补充 `hasRange/minValue/maxValue/mayOverflow`。整数/char literal 记录精确单点范围；左右都是整数常量的 `+`、`-`、`*`、`/` 会在无溢出且无除零时记录精确结果范围；`INT64_MAX + 1`、过大乘法和 `INT64_MIN / -1` 这类无法安全折叠的整数常量表达式会保留无范围状态并标记 `mayOverflow=true`。
- 非精确整数 interval 和 unsigned range payload 也在事实层保留：例如 `seed: int` 且 `seed` 范围为 `2..4` 时，`seed + 3` 写入 `5..7`；`uint8` 范围参与 `+ 3` 时同时保留 signed `3..258` 和 unsigned `3..258`，`uint64` 原始标注可以通过 `hasUnsignedRange` 表达 `UINT64_MAX`。
- ownership builtin expression 在 type inference 成功后写入 `ownershipFacts`。例如 `%borrow(owner)` 若 operand 是 `%unique int`，会产生 `ZR_SEMANTIC_OWNERSHIP_FACT_BORROW` 和 `ZR_OWNERSHIP_QUALIFIER_BORROWED`；REPL `:type` 通过 `ZrParser_SemanticFacts_FindOwnershipByNode` 读取同一个节点事实并显示 `Ownership: borrow %borrowed`。
- literal AST 节点现在在消费 token 前保存 `get_current_token_location` 的范围，并把 semantic fact range 对齐到真实 token。这个修正保证 `42` 的数值事实范围是 `0..2`，也避免 LSP 在二元表达式 operator 位置查询时误命中后续字面量。unary expression AST 节点也使用 operator token location 作为 range 起点，再与 operand range 合并；`-42` 覆盖 `0..3`，`!true` 覆盖 `0..5`。

helper 已能分类 identifier、unary、call、member/primary、assignment、conditional、array、object、lambda、type conversion 和 ownership builtin 节点。后续任务需要继续丰富每类 expression fact 的语义细节，例如 overload/callee resolution、完整成员链、conversion provenance 和跨过程事实传播，而不是新增平行事实模型。

## Reference Emission

LSP semantic analyzer 的引用收集继续写入既有 `SZrReferenceTracker`，同时把同一条信息复制到共享 `referenceFacts`：

- `semantic_analyzer_references.c` 的 `semantic_add_reference` 是 read/write/call 使用引用的窄入口。它保持原有 reference tracker 行为，并追加 `SZrSemanticReferenceFact`。
- `semantic_analyzer_support.c` 的 `ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForRange` 是声明引用入口。`AddDefinitionReferenceForSymbol` 和 `semantic_analyzer_symbols.c` 中 class property/accessor 的专用 range 都走这个入口。
- 引用事实记录 `range`、`declarationRange`、`kind`、`symbolId`、`typeId`、`name` 和 `isResolved`。当 AST 位置是零宽 identifier 位置时，引用入口会用符号名扩展成完整 token 范围。

这一步的目标是强化 token 追踪和声明/使用关系追踪。后续 LSP 局部查询可以直接查询 `referenceFacts`，不用重新扫描 AST 或重复实现 reference tracker 的范围修正逻辑。

parser/type inference 现在也开始在没有 LSP analyzer 参与时写入可复用的 reference facts：

- `ZrParser_TypeEnvironment_RegisterVariableEx` 会为带 declaration range 的变量注册 shared symbol/type id，并写入 declaration reference fact；identifier inference 成功后写入 read reference fact。
- `ZrParser_TypeEnvironment_RegisterFunctionEx` 会为带 source declaration node 的函数注册 shared symbol/type id，记录函数名 token declaration range，并写入 declaration reference fact。range 有效性允许 offset 0 的文件开头声明，避免把首个 top-level 函数误当成无范围声明。
- `ZrParser_PrimaryExpressionType_Infer` 在 runtime 或 compile-time function overload resolution 成功后调用 `type_inference_record_primary_call_reference_fact`，为 resolved root call 写入 `ZR_SEMANTIC_REFERENCE_CALL`。这个事实不执行调用、不伪造 numeric range，只保存 resolved callee token、declaration token、symbol/type id 和函数名，供 LSP/REPL/Debug 后续查询复用。
- `ZrParser_AssignmentType_Infer` 对已注册 identifier 左值使用 assignment 专用路径：它复制并规范化绑定类型、显式记录 left identifier expression/numeric facts，再通过 `type_inference_record_identifier_write_reference_fact` 写入 `ZR_SEMANTIC_REFERENCE_WRITE`。非 identifier 左值仍走既有 expression inference 路径。
- `ZrParser_PrimaryExpressionType_Infer` 对成功推断的成员读取追加 `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS`。非 computed property token 使用 property range；computed member read 使用完整 member expression range，并让 index expression 自己发射 read/numeric/expression facts。`ZrParser_SemanticFacts_FindReferenceAtPosition` 的更窄 range 优先规则保证 `seed[index]` 中的 `index` token 仍命中索引变量 read，而 `[` / member expression 范围命中 member access。
- `ZrParser_AssignmentType_Infer` 对非 identifier 左值成功推断后，会在最后一个 member/index target token 上追加 `ZR_SEMANTIC_REFERENCE_MEMBER_WRITE`。该事实是 token 分类事实，`isResolved=false`，`symbolId/typeId` 保持 invalid；它让 LSP/后续 REPL/Debug 查询知道光标位于 member assignment write，而不会把 member target 当作普通 expression payload 或 read reference。

## Reachability And Logical Emission

`semantic_analyzer_typecheck.c` 在既有诊断点旁边追加共享事实，不新增第二套控制流分析。条件是否为可证明布尔常量的判断由 `semantic_analyzer_constant_condition.c` 封装，当前只接受布尔字面量和同一可见作用域中 `var const name = true/false` 这类声明早于使用位置的绑定。loop body 的常量条件可达性由 `semantic_analyzer_reachability.c` 承担，避免继续扩大 typecheck orchestration 文件。parser/type inference 现在也会为当前表达式本身写入确定性 logical fact；semantic analyzer 继续负责需要语句/分支上下文的诊断和 reachability fact：

- 语句块扫描发现 `return` 或 `throw` 之后仍有语句时，继续发出 `unreachable_code` warning，同时写入 `ZR_SEMANTIC_REACHABILITY_UNREACHABLE`。`cause` 分别使用 `ZR_SEMANTIC_REACHABILITY_AFTER_RETURN` 或 `ZR_SEMANTIC_REACHABILITY_AFTER_THROW`，`causeNode` 指向导致后续语句不可达的退出节点。
- 常量 `if (true/false)` 和可证明的 `var const flag = false; if (flag) { ... }` 仍发出 `unreachable_branch` warning，同时为死分支写入 `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`，并为条件写入 `ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_TRUE` 或 `ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE`。普通可变变量、声明晚于使用位置的符号、非布尔 initializer 和无法解析的标识符保持 unknown，不会被误判为常量分支。
- 常量 `if` 的实际执行分支若必然退出，也会让后续同块语句写入 `ZR_SEMANTIC_REACHABILITY_AFTER_EXHAUSTIVE_BRANCH`。例如 `var const flag = true; if (flag) { return 1; } var after = 2;` 中 `after` 的局部 hover 会显示 `Logical value: true` 和 `Reachability: unreachable after exhaustive branch`。这个判断由 `semantic_analyzer_reachability.c` 统一负责，`semantic_analyzer_typecheck.c` 只负责在 block 扫描时委托退出判定并记录事实。
- 带 default 的 `switch` 若每个 case/default block 都必然退出，也会让后续同块语句写入 `ZR_SEMANTIC_REACHABILITY_AFTER_EXHAUSTIVE_BRANCH`，`causeNode` 指向 `ZR_AST_SWITCH_EXPRESSION`。缺少 default、case block 不是退出路径、或 default block 不是退出路径时保持可达，避免把非穷尽分支误判为死代码。
- 常量 `while (false)` / `for (...; false; ...)` 和可证明的 `var const keepGoing = false; while (keepGoing) { ... }` 会发出 `unreachable_loop_body` warning，同时为 loop body 写入 `ZR_SEMANTIC_REACHABILITY_CONDITION_FALSE`，并为条件写入 `ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE`。局部 hover/rich hover 可以在 loop body 内显示 `Logical value: false` 和 `Reachability: unreachable because the condition is false`。常量 `true` loop condition 只记录已知 logical fact，不把 body 标记为不可达。
- 常量 `while (true)` / `for (...; true; ...)` 若 loop body 的所有可执行路径都通过 `return` 或 `throw` 离开外层控制流，也会让后续同块语句写入 `ZR_SEMANTIC_REACHABILITY_AFTER_NON_FALLTHROUGH_LOOP`，`causeNode` 指向 loop 节点。传统 `for (;;) { ... }` 没有显式 condition token，但按语言规范的无限循环形式参与同一 non-fallthrough 证明；nested constant-true loop 若自身所有可执行路径离开外层控制流，也能证明外层 loop body 不会继续执行。local query 会从 `ZR_AST_WHILE_LOOP` / `ZR_AST_FOR_LOOP` cause node 回查显式 loop condition 的 logical fact，因此 hover 可以同时显示 `Logical value: true` 和 `Reachability: unreachable after non-fallthrough loop`；`for (;;)` 这类省略条件的 loop 只暴露 reachability cause。这个证明是保守的：只要 body 中可执行路径可能走到 loop-local `break` 或 `continue`，就不会把 loop 后语句标记为不可达；`break; return;` 这类 break 后不可达 return 也不能证明 loop 后语句不可达。
  - 确定性短路 `true || rhs` 和 `false && rhs` 仍发出 `short_circuit_unreachable` warning，同时写入 `ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT`。事实记录 `hasKnownValue=true`，`knownValue` 是整个逻辑表达式的确定结果，`relatedNode` 指向被跳过的右侧节点。
- 短路右侧也会写入 `ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT`。短路 logical fact 和 reachability fact 现在都可通过覆盖完整逻辑表达式的范围查询，`FindLogicalAtPosition` 会在 operator 或 operand 位置返回最窄命中的 logical fact，同时保留 RHS 节点为 `node`。

这些事实让后续 LSP hover、局部诊断、Debug 条件断点和 REPL 表达式执行可以直接解释“为什么这里不会执行”或“为什么右侧不会求值”，而不是只依赖 warning 文本。

Debug safe evaluate 现在已经对齐这条跳过求值合同的一部分：`true || rhs`、`false && rhs` 和 `condition ? selected : skipped` 在条件断点/`evaluate` 表达式里会消费被跳过表达式的语法，但不会解析被跳过路径上的局部变量、成员、索引或数值/比较计算。这样 Debug 表达式不会因为确定不可执行的 RHS 或 branch 上存在 `missingLocal` 这类当前帧缺失数据而失败；未短路/被选中的路径仍按只读求值路径正常访问调试快照。

Debug safe evaluate 的数值语义错误也开始走 richer diagnostic 文本：非数值 operand 参与数值运算、除零、取模零值或非整数取模、unary `-` 作用在非数值上时，错误会说明实际 operand 类型、具体原因和建议动作。成员/索引/括号/条件结构的语法错误也不再停留在 `expected member name`、`missing ']'`、`missing ')'` 或 generic `expected expression` 这类短句，`true.`、`true[0`、`(true || false`、缺少 `:` 的条件表达式，以及 `true ? : 2` / `true ? 1 :` 这类缺少 consequent 或 alternate branch 的条件表达式都会说明缺少的结构、错误原因和下一步修复建议。这仍然是调试时的只读安全求值器，不是新的完整编译期语义事实层；后续如果要把 Debug 与 parser shared local query 完整打通，应继续复用 `SZrSemanticContext` 的事实，而不是在 Debug 内部复制一套类型推断。当前 Debug 代码已经把 safe-evaluate diagnostics/right-operand helper 抽到 `debug_eval_diagnostics.c` / `debug_eval_internal.h`，后续应继续保持这种窄模块边界。

## REPL Type Query Consumer

`zr_vm_cli/src/zr_vm_cli/repl/repl.c` 的 `:type <expression>` 命令是 REPL 的第一条共享事实消费路径。它不会执行目标表达式，而是创建 fresh analysis state，将当前 REPL 会话中已成功提交的 declaration-style 源码和参数表达式一起解析，把 prior variable declarations 注册进 parser type environment，调用 `ZrParser_ExpressionType_Infer` 推断最后一个 expression statement，然后读取同一个 `compilerState.semanticContext`。

当前输出范围保持窄而可验证：

- 总是输出 parser type formatter 给出的 `Type: ...`。
- 若当前表达式或其子表达式有 numeric fact，则输出 exact range、unsigned range 或 `may overflow` 提示。`[1 + 2]` 这类 aggregate expression 会通过 expression walker 找到 element 上的 structural numeric fact 并输出 `Numeric range: 3..3`，同时在命中 structural fact 后停止下钻，避免再输出 literal `1..1` / `2..2` 这类叶子噪声。
- 若当前表达式或其子表达式有 logical fact，则遍历表达式树输出已知布尔值或短路说明。`true || false` 当前会输出 `Type: bool`、`Logical value: true` 和 `Logical flow: short-circuits right operand`；`true ? 1 : 2` 会把 constant condition 的 `Logical value: true` 和 skipped alternate 的 reachability fact 一起显示。这些信息来自 parser/type inference 写入的同一个 `SZrSemanticContext`。
- 若当前表达式或其子表达式范围有 reachability fact，则输出不可达原因。`true || false` 的 `false` operand 会通过 parser/type inference 写入 `ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT`，`:type true || false` 因此会额外输出 `Reachability: unreachable because short-circuit skips evaluation`。REPL 只遍历表达式树并按 range 查询 `SZrSemanticContext`，不复制短路判断。
- 若当前表达式或 aggregate 子表达式节点有 ownership fact，则输出 ownership action 和 qualifier，或显示 violation 文本。`%borrow(owner)` 当前会输出 `Type: %borrowed int` 和 `Ownership: borrow %borrowed`；`[%borrow(owner)]` 会在 array root 下继续显示 nested ownership builtin 的 `Ownership: borrow %borrowed`。这些信息来自 `infer_construct_expression_type` 成功路径记录的 ownership builtin fact。
- 若当前表达式、aggregate 子表达式节点或 call argument 节点有 expression fact，则输出共享 fact 里的 kind/exactness/constant/call/member payload。`[1 + 2]` 会显示 array root 的 `Expression: array exact`，以及 element expression 的 `Expression: binary exact` / `Constant: 3`，但不会下钻到 literal leaf constants。`pick(42)` 会显示 `Call: pick args=1`；同一会话先声明 `func pick(value: int): int { ... }` 后，`:type pick(1 + 2)` 会在 call payload 之后继续显示 argument 的 `Expression: binary exact` / `Constant: 3`，仍不会输出 leaf `Constant: 1` / `Constant: 2`。`seed.value` 会显示 `Member: value`。REPL 不解析调用文本来重建这些字段，而是读取 `SZrSemanticExpressionFact` payload。
- 若当前表达式或其子表达式范围有 reference fact，则输出引用分类。`seed.value` 会显示 `Reference: member value`，`seed.value = 3` 会显示 `Reference: member write value`；只有 `isResolved=true` 的 fact 才会输出 `Declared at:`。
- 若同一 REPL 会话先成功提交 `var seed = 2;`，后续 `:type seed + 3` 会通过 replayed declaration context 输出 `Type: int` 和 `Numeric range: 5..5`，并且不会执行 `seed + 3` 本身。

这条路径刻意只保存 declaration-style 源码上下文，不保存长期 VM runtime cell、不执行目标表达式、不复制 LSP 的局部查询实现。每次提交仍使用 fresh runtime 并重放声明上下文；`:reset` 会清空该上下文。后续若要做持久交互式对象/模块状态或更完整的本地事实显示，应继续复用 parser/LSP 已有事实层，而不是在 CLI 内新增第二套语义推断。

## Ownership And Rich Diagnostics

所有权违规现在通过 `diagnostic_builder.c` 先构造成结构化诊断，再转换成 language server 的 `SZrDiagnostic`。`SZrDiagnostic` 除了原有 severity、range、message、code 外，还保留 `cause` 和 `suggestion`，让 LSP 或后续 Debug/REPL 消费方能展示“具体为什么错”和“下一步怎么改”，而不是只给出 `type_mismatch`。

当前已覆盖的稳定语义诊断：

- `ownership_mismatch`: `%shared` 流入 `%unique` 等目标所有权限定不匹配的声明初始化、return、函数参数和方法参数。
- `owner_to_plain_escape`: `%unique/%shared` 隐式流入 plain GC value，建议显式 `%detach(...)`。
- `weak_value_requires_upgrade`: `%weak` 直接传给 `%borrowed`，建议先 `%upgrade(...)` 并处理升级失败路径。
- `borrow_escape`: `return %borrow(...)` 这类临时借用逃逸当前 owner 的 return path。
- `loan_escape`: 与 `borrow_escape` 同类的 `%loan(...)` return escape 入口，当前 builder、发射路径和专门回归测试已就绪。

每个成功发射的所有权结构化诊断都会同步写入 `ownershipFacts`。事实的 `range` 对准真正有问题的表达式或参数 token，例如变量初始化右侧、函数实参、方法实参或 `%borrow(...)` 所在行；诊断行仍保持在用户能看到的语句位置。事实保存 `ZR_SEMANTIC_OWNERSHIP_FACT_ERROR`、实际所有权 qualifier、`isViolation=true` 和诊断 message 的深拷贝，后续局部查询可以在光标停留于表达式时直接解释所有权问题。

## Syntax Diagnostics Through LSP

parser 现在保留旧 `TZrParserErrorCallback`，同时在 `SZrParserState` 上新增可选 `TZrParserStructuredErrorCallback`。旧回调仍接收 `range/message/token`，结构化回调接收 `SZrStructuredDiagnostic`，包含 stable code、message、cause、suggestion 和 severity。`report_structured_parser_error` 会先调用结构化回调，再调用旧字符串回调，保证旧调用方不需要迁移；severity 为 warning/info 的结构化诊断只记录诊断，不设置 parser hard-error state。

当前代表性语法诊断包括：

- `legacy_ownership_type_syntax`: 旧 `%unique/%shared/%weak/%borrow/%loan T` 所有权类型语法仍作为迁移兼容语法解析，但会产生 warning 级结构化诊断。message 说明 legacy ownership type syntax 已 deprecated，cause 指出所有权类型已经迁移为 intrinsic generic wrapper，suggestion 建议写成 `Unique<T>`、`Shared<T>`、`Weak<T>`、`Borrow<T>` 或 `Loan<T>`。该诊断不会阻断 AST 构建、source 解析或 LSP 语义分析。
- `missing_expression_after_assignment`: `var x = ;` 或 `var x = <eos>` 在 `=` 后直接遇到语句终止符时产生。message 是 `Missing expression after '='`，cause 说明初始化已开始但没有值表达式，suggestion 建议在 `;` 前添加表达式或移除 `=` initializer。
- `missing_right_operand`: 二元、逻辑或赋值操作符右侧直接遇到语句终止符或源码结束时产生，例如 `1 +`、`true &&`、`value = ;`。message 是 `Missing expression after '<op>'`，cause 说明该操作符需要右侧表达式但表达式提前结束，suggestion 建议补上右侧表达式或移除该操作符。
- `missing_member_name`: 点号成员访问在 `.` 后没有合法成员名时产生，例如 `value.`。message 是 `Missing member name after '.'`，cause 说明 member-access operator 已写出但缺少属性、方法或字段名，suggestion 建议补上成员名或移除该成员访问。
- `missing_index_close`: computed member/index access 在 `[` 后已经读到 index 表达式但没有闭合 `]` 时产生，例如 `value[0;`。message 是 `Missing closing ']' in index access`，cause 说明 computed member access 已开始但 closing bracket 缺失，suggestion 建议在继续 member access 前补上 `]`。
- `missing_call_close`: function call 在 `(` 后已经读到参数但没有闭合 `)` 时产生，例如 `pick(1 + 2;`。message 是 `Missing closing ')' in function call`，cause 说明 argument list 已开始但 closing parenthesis 缺失，suggestion 建议在继续源码前补上 `)`。
- `missing_parameter_list_close`: function/method/meta/accessor/delegate declaration parameter list 在 `(` 后已经读到参数但没有闭合 `)` 时产生，例如 top-level `func pick(value: int: int { ... }`、class method `class Box { func read(value: int: int { ... } }`、class setter `class Sized { set length(value: int { ... } }`、class meta function `class Callable { @call(value: int: int { ... } }`、interface method signature `interface Readable { read(value: int: int; }`、interface meta signature `interface Callable { @call(value: int: int; }`、extern function `%extern("fixture") { NativeAdd(value: int: int; }` 或 extern delegate `%extern("fixture") { delegate Callback(value: int: int; }`。message 是 `Missing closing ')' in function declaration parameters`，cause 说明 declaration parameter list 已开始但 closing parenthesis 缺失，suggestion 建议在继续源码前补上 `)`。该诊断和 function call 的 `missing_call_close`、grouped expression 的 `missing_group_close` 分开，方便编辑器区分声明参数列表和表达式括号问题。
- `missing_group_close`: grouped expression 在 `(` 后已经读到表达式但没有闭合 `)` 时产生，例如 `return (1 + 2`。message 是 `Missing closing ')' in grouped expression`，cause 说明 grouped expression 已开始但 closing parenthesis 缺失，suggestion 建议在继续源码前补上 `)`。
- `missing_array_close`: array literal 在 `[` 后已经读到元素但没有闭合 `]` 时产生，例如 `return [1, 2`。message 是 `Missing closing ']' in array literal`，cause 说明 array literal 已开始但 closing bracket 缺失，suggestion 建议在继续源码前补上 `]`。
- `missing_array_element_separator`: array literal 中一个元素后紧跟另一个元素表达式、但没有 `,` 或 `;` 分隔时产生，例如 `return [1 2];`。message 是 `Missing separator between array elements`，cause 说明前一个元素后立即出现新的元素表达式，suggestion 建议在元素之间插入 `,` 或 `;`。
- `missing_object_close`: object literal 在 `{` 后已经读到属性但没有闭合 `}` 时产生，例如 `return {a: 1`。message 是 `Missing closing '}' in object literal`，cause 说明 object literal 已开始但 closing brace 缺失，suggestion 建议在继续源码前补上 `}`。
- `missing_object_computed_key_close`: object literal computed key 在 `[` 后已经读到 key expression 但没有闭合 `]` 时产生，例如 `return {[seed: 1}`。message 是 `Missing closing ']' in computed object key`，cause 说明 computed object key 已开始但 key expression 没有闭合，suggestion 建议在 `:` 前补上 `]`。
- `missing_object_property_colon`: object literal property key 后缺少 `:` 时产生，例如 `{a 1}`。message 是 `Missing ':' after object property key`，cause 说明 object literal property 需要用 `:` 分隔 key 和 value expression，suggestion 建议在属性 key 和 value 之间插入 `:`。
- `missing_object_property_separator`: object literal 中一个属性 value 后紧跟另一个属性 key、但没有 `,` 或 `;` 分隔时产生，例如 `{a: 1 b: 2}`。message 是 `Missing separator between object properties`，cause 说明前一个属性 value 后立即出现新的属性 key，suggestion 建议在属性之间插入 `,` 或 `;`。
- `missing_declaration_body_open`: declaration header 后缺少 body-opening `{` 时产生，例如 `class Box`、`interface Sized`、`func pick(): int`、`enum Tone`、`%extern("fixture")` 或 `%test("smoke")`。message 按 declaration kind 区分，例如 `Missing '{' to start class declaration body`、`Missing '{' to start interface declaration body`、`Missing '{' to start function declaration body`、`Missing '{' to start enum declaration body`、`Missing '{' to start extern block body` 或 `Missing '{' to start test declaration body`，cause 说明 declaration/block header 已解析完成但 body-opening `{` 缺失，suggestion 建议在对应 declaration、extern block 或 test declaration header 后插入 `{` 或补全 declaration body。该诊断把 declaration body opener 缺失和普通 expected-token fallback 分开，方便 LSP/pull diagnostics 给出具体修复建议。
- `missing_declaration_body_close`: declaration body 已经用 `{` 开始但输入结束前没有闭合 `}` 时产生，例如 `class Box { var id: int;`、`interface Sized { get length: int;`、`enum Tone { warm,`、`%extern("fixture") { NativeAdd(value: int): int;`、`func pick(): int { return 1;` 或 `%test("smoke") { return 1;`。message 按 declaration kind 区分，例如 `Missing closing '}' for class declaration body`、`Missing closing '}' for interface declaration body`、`Missing closing '}' for enum declaration body`、`Missing closing '}' for extern block body`、`Missing closing '}' for function declaration body` 或 `Missing closing '}' for test declaration body`，cause 说明 declaration body 已开始但 parser 到达输入末尾仍未看到 closing brace，suggestion 建议插入 `}` 关闭对应 declaration body 再继续。该诊断只在 parser 已确认 body-opening `{` 后触发，避免 `class Box` / `interface Sized` / `enum Tone` / `%extern("fixture")` / `func pick(): int` / `%test("smoke")` 这类 opener 缺失场景同时报告 close 问题。
- `missing_extern_spec_close`: `%extern(...)` block spec 在 `(` 后已经读到 library spec，但进入 extern block body 前没有闭合 `)` 时产生，例如 `%extern("fixture" { NativeAdd(value: int): int; }`。message 是 `Missing closing ')' in extern block spec`，cause 说明 extern block spec 已开始但 parser 到达 extern block body 前仍未看到 closing parenthesis，suggestion 建议在 extern block body 前插入 `)`。该诊断把 extern block header 的 spec-close 问题和 generic `expected ')'` fallback 分开，方便 LSP 在局部编辑 extern block 时给出可执行修复建议。
- `missing_test_name_close`: `%test(...)` declaration name 在 `(` 后已经读到测试名，但进入 test body 前没有闭合 `)` 时产生，例如 `%test("smoke" { return 1; }`。message 是 `Missing closing ')' in test declaration name`，cause 说明 test declaration name 已开始但 parser 到达 test body 前仍未看到 closing parenthesis，suggestion 建议在 test body 前插入 `)`。该诊断把 test declaration header 的 name-close 问题和 generic `expected ')'` fallback 分开，方便 LSP 在局部编辑 `%test` declaration 时给出可执行修复建议。
- `missing_statement_body_open`: control statement 或 branch/header 后缺少 body-opening `{` 时产生，例如 `if (ready)\nreturn 1;`、`while (ready)\nreturn 1;`、`for (;;)\nreturn 1;`、`for (var item in items)\nreturn item;`、`switch (choice)\nreturn 1;`、`switch (choice) { (1)\nreturn 1; }`、`switch (choice) { ()\nreturn 1; }`、`if (ready) { return 1; } else\nreturn 2;`、`try\nreturn 1;`、`try { throw 1; } catch (error)\nreturn 2;`、`try { return 1; } finally\nreturn 2;` 或 `%using (resource)\nreturn resource;`。message 按 statement kind 区分，例如 `Missing '{' to start if statement body`、`Missing '{' to start while statement body`、`Missing '{' to start for statement body`、`Missing '{' to start foreach statement body`、`Missing '{' to start switch statement body`、`Missing '{' to start switch case body`、`Missing '{' to start switch default body`、`Missing '{' to start else statement body`、`Missing '{' to start try statement body`、`Missing '{' to start catch statement body`、`Missing '{' to start finally statement body` 或 `Missing '{' to start using statement body`，cause 说明 statement/branch header 已解析完成但 body-opening `{` 缺失，suggestion 建议在对应 statement、switch case/default、foreach、else/catch/finally/using header 后插入 `{` 或用 braces 包住 statement body。该诊断把控制语句/分支缺 block opener 和普通 expected-token fallback 分开，方便 LSP/pull diagnostics 直接给出修复建议。
- `missing_block_close`: block 已经用 `{` 开始但输入结束前没有闭合 `}` 时产生，例如 `if (ready) { return 1;`。message 是 `Missing closing '}' for block`，cause 说明 block 已开始但 parser 到达输入末尾仍未看到 closing brace，suggestion 建议插入 `}` 关闭 block 再继续。该诊断覆盖普通 statement/declaration body block 的未闭合局部编辑场景，避免 `parse_block` 退回 generic expected-token fallback。
- `missing_catch_pattern_close`: `catch` clause pattern 在 `(` 后已经读到 pattern/parameter，但进入 catch body 前没有闭合 `)` 时产生，例如 `try { throw 1; } catch (error { return 2; }`。message 是 `Missing closing ')' in catch pattern`，cause 说明 catch pattern 已开始但 parser 到达 catch body 前仍未看到 closing parenthesis，suggestion 建议在 catch body 前插入 `)`。该诊断把 catch header 的 pattern-close 问题和 generic `expected ')'` fallback 分开，方便 LSP 在局部编辑 catch clause 时给出可执行修复建议。
- `missing_using_resource_close`: block-scoped `%using` resource expression 在 `(` 后已经读到 resource，但进入 using body 前没有闭合 `)` 时产生，例如 `%using (resource { return resource; }`。message 是 `Missing closing ')' in using resource`，cause 说明 using resource expression 已开始但 parser 到达 using body 前仍未看到 closing parenthesis，suggestion 建议在 using body 前插入 `)`。该诊断把 using header 的 resource-close 问题和 generic `expected ')'` fallback 分开，方便 LSP 在局部编辑 using statement 时给出可执行修复建议。
- `missing_for_header_close`: traditional `for` header 在 `(` 后已经读到 header 片段，但进入 loop body 前没有闭合 `)` 时产生，例如 `for (; ready; ready = false { return 1; }`。message 是 `Missing closing ')' in for header`，cause 说明 for header 已开始但 parser 到达 loop body 前仍未看到 closing parenthesis，suggestion 建议在 loop body 前插入 `)`。该诊断把 traditional for header close 问题和 generic `expected ')'` fallback 分开。
- `missing_for_header_separator`: traditional `for` header 的 initializer、condition、step clause 之间缺少 `;` 时产生，例如 `for (i = 0 i < 3; i = i + 1) { out i; }`、`for (i = 0; i < 3 i = i + 1) { out i; }` 或 `for (var i = 0 i < 3; i = i + 1) { out i; }`。message 是 `Missing ';' between for header clauses`，cause 说明 traditional for header 需要用 `;` 分隔 clauses，suggestion 建议在 clauses 之间插入 `;`。该诊断把 for header 内部的分隔符缺失和普通 statement semicolon / generic expected-token fallback 分开，顶层和 block 内 `for (var ...)` 分流都会先判断 `=` / `;` / `in`，避免把 malformed traditional for 误走 foreach 诊断。
- `missing_foreach_header_close`: `foreach` header 在 `(` 后已经读到 iterable expression，但进入 loop body 前没有闭合 `)` 时产生，例如 `for (var item in items { return item; }`。message 是 `Missing closing ')' in foreach header`，cause 说明 foreach header 已开始但 parser 到达 loop body 前仍未看到 closing parenthesis，suggestion 建议在 iterable expression 后、loop body 前插入 `)`。该诊断把 foreach header close 问题和 generic `expected ')'` fallback 分开，方便 LSP 在局部编辑 foreach loop 时给出可执行修复建议。
- `missing_foreach_in_keyword`: `foreach` header 已经读到 pattern，但 pattern 和 iterable expression 之间缺少 `in` keyword 时产生，例如 `for (var item items) { return item; }`。message 是 `Missing 'in' in foreach header`，cause 说明 foreach header 有 pattern 但 parser 没有在 iterable expression 前找到 `in`，suggestion 建议在 pattern 和 iterable expression 之间插入 `in`。该诊断把 foreach separator 缺失和 generic `expected 'in'` fallback 分开，方便 LSP 在局部编辑 foreach loop 时给出可执行修复建议。
- `missing_switch_case_header_close`: `switch` 普通 case header 在 `(` 后已经读到 case expression，但进入 case body 前没有闭合 `)` 时产生，例如 `switch (choice) { (1 { return 1; } }`。message 是 `Missing closing ')' in switch case header`，cause 说明 switch case header 已开始但 parser 到达 case body 前仍未看到 closing parenthesis，suggestion 建议在 case body 前插入 `)`。该诊断把 switch case header 的 close 问题和 generic `expected ')'` fallback 分开，方便 LSP 在局部编辑 switch case 时给出可执行修复建议。
- `missing_switch_body_close`: `switch` body 已经用 `{` 开始但输入结束前没有闭合最终 `}` 时产生，例如 `switch (choice) { (1) { return 1; }`。message 是 `Missing closing '}' for switch body`，cause 说明 switch body 已开始但 parser 到达输入末尾仍未看到 closing brace，suggestion 建议插入 `}` 关闭 switch body 再继续。该诊断把 switch body close 问题和 generic `expected '}'` fallback 分开，方便 LSP 在局部编辑 switch statement 时给出可执行修复建议。

LSP incremental parser 设置结构化回调并把它转换成 `SZrDiagnostic`。同一错误随后触发的旧字符串回调会被 LSP collector 跳过一次，避免用户看到一条新结构化诊断和一条旧 `parser_syntax_error` 重复诊断。warning-only parser diagnostics 不会触发 fallback AST；例如 legacy ownership type warning 会发布为 LSP severity 2，同时保留当前 AST/analyzer 结果。最终 LSP wire struct 不新增字段；`lsp_interface_support.c` 会在 `message` 内追加：

```text
Cause: ...
Suggestion: ...
```

这让 VSCode 端在不改协议结构的前提下展示具体错误问题和建议。parser 内部的试探性解析路径在临时关闭旧错误回调时，也同步关闭并恢复结构化回调，避免未来更多结构化语法错误在 lookahead/probe 分支中误报。`ZrParser_Source_Compile` 也会捕获 parser 已报告的错误；即使恢复性解析返回了 AST，source compile 仍会拒绝生成可执行 function，避免不完整表达式继续进入 runtime 后触发断言或崩溃。

## Query Contract

追加函数在空上下文、空事实或未初始化数组时返回 `ZR_FALSE`。查询函数在无法命中时返回 `ZR_NULL`，上层必须把它视为显式 unknown，而不是崩溃或回退到误导性类型。

位置查询只在事实范围和查询位置双方都有 offset 时使用 offset；任一侧缺少 offset 时回退到 line/column。这样 LSP 可以用带 offset 的位置查询 AST 仍只带行列信息的事实，不会因为一侧 offset 为零而误判未命中。`FindExpressionAtPosition` 和 `FindLogicalAtPosition` 都返回包含该位置的最窄事实范围，用于 hover、局部类型推断、逻辑短路解释和嵌套表达式查询。

## LSP Local Semantic Query

`lsp_local_semantic_query.c` 是 language server 的局部语义事实入口。它把 VSCode/LSP 的局部位置查询收敛成三种稳定结果：

- `ZR_LSP_LOCAL_SEMANTIC_QUERY_FACT`: 在当前位置命中表达式、数值、引用、不可达、逻辑或所有权事实。
- `ZR_LSP_LOCAL_SEMANTIC_QUERY_DIAGNOSTIC_FAILURE`: 当前位置被 parser diagnostic 阻断，例如 `var x = ;` 的 `missing_expression_after_assignment`。
- `ZR_LSP_LOCAL_SEMANTIC_QUERY_UNKNOWN`: 当前没有可用 analyzer、semantic context 或可命中的事实。调用方必须把它当成“没有足够事实”，不能伪造弱类型或崩溃。

`ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt` 会先把 LSP position 转换成文件 range，再检查覆盖该位置的 parser diagnostic。若语法错误已经说明当前位置无法形成合法表达式，查询直接返回 diagnostic-backed failure，并保留诊断对象给 hover、局部推断或后续工具展示具体原因。若没有阻断诊断，它会使用当前 URI 的 analyzer 和 `SZrSemanticContext` 查询 expression/numeric/reference/reachability/logical/ownership facts。`ZrLanguageServer_Lsp_GetHover` 在进入旧 hover fallback 前会先检查这个结果；当当前位置已经被 parser diagnostic 阻断时，hover 返回 no result，而不是继续搜索旧语义路径并生成误导性内容。

局部数值查询通过 `SZrLspLocalSemanticQueryResult.numericFact` 暴露，不再要求 VSCode 插件或后续 hover 重新推断表达式范围。`ExpressionAt` 先定位当前位置的结构化表达式节点，再按同一 AST 节点回查 numeric fact；因此光标位于 `1 + 2` 的 `+` token 时会返回二元表达式的 promotion fact，`hasRange=true` 且 `minValue=maxValue=3`。对于 `9223372036854775807 + 1`，同一查询返回 `mayOverflow=true` 且 `hasRange=false`，调用方可以展示“潜在溢出”而不是伪造结果范围。

局部逻辑查询通过 `SZrLspLocalSemanticQueryResult.logicalFact` 暴露。`ExpressionAt` 会按当前位置查询 logical facts，并与 expression/reachability facts 一起返回；因此光标位于 `true || false` 的 `||` token 时会返回 exact `bool` expression fact、`ZR_SEMANTIC_LOGICAL_FACT_SHORT_CIRCUIT` 和 `ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT`，调用方可以展示“整个表达式已知为 true，右侧不会求值”的具体语义，而不是只给出普通 bool 类型。unary logical-not 也走同一路径：`!true` 的 `!` token 位置返回 unary expression fact 和 `ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE`，事实来自 parser/type inference，不由 LSP 重算。若当前位置只命中 `reachabilityFact`，local query 会沿 `causeNode` 回查 logical fact；当原因节点是 `if` 时，会转到 `if.condition`，所以常量分支退出后的后续语句也能展示导致它不可达的布尔事实。

`ZrLanguageServer_LspLocalSemanticQuery_ReferenceAt` 查询同一事实层的 `referenceFacts`，并保留 usage fact 到 declaration token range 的关系。LSP semantic analyzer 对 identifier 赋值左值使用 write-reference 路径，所以 `seed = 3;` 的左侧 `seed` 会返回 `ZR_SEMANTIC_REFERENCE_WRITE`，而不是落回普通 read reference。当前 definition/reference/highlight 的主路径仍由既有 `ZrLanguageServer_LspSemanticQuery_*` 负责；局部 reference fact 入口已经可供后续 VSCode 插件局部查询、hover 解释或导航短路复用。

`ZrLanguageServer_LspLocalSemanticQuery_AppendFactsToHover` 把当前 query 命中的共享事实追加到已有 hover markdown 上。这个 API 让传统 symbol hover 保留原本的变量/类型/文档内容，同时展示局部 reference fact，例如 `Reference: read` / `Reference: write` 和声明 token 的 `Declared at: line:column`。局部 reachability fact 也会携带具体原因，例如 `Reachability: unreachable after return`、`unreachable after throw` 或 `because short-circuit logic skips it`，而不是只展示一个不可达布尔状态。ownership 违规事实会把状态和诊断 message 保持在同一 `Ownership:` 行，例如 `Ownership: violation - Loaned value cannot escape its owner`，这样 plain hover 可读，rich hover 也能把完整所有权原因放进稳定 `ownership` role。expression fact 本身会显示 `Expression: binary exact` 与 `Constant: 3` 这类行，并在 rich hover 中映射为 `expression` / `constant` roles；字符串 constant 使用 `lsp_local_semantic_expression_text.c` 的 byte-length escaped writer，因此内嵌 quote、backslash、newline、tab 和低控制字节不会把 hover markdown 或 rich-hover section 拆成不可读的多行 raw payload。调用/成员 payload 也在同一 formatter 中追加：`pick(42)` 的 call target token 会显示 `Call: pick args=1`，rich hover role 为 `call`；`seed.value` 的 member token 会显示 `Member: value`，rich hover role 为 `member`。事实格式化留在 `lsp_local_semantic_query.c` 和 expression text helper，`lsp_interface.c` 只负责在 hover 编排路径上调用它们和映射结构化 section role，避免把本地事实展示规则继续堆进已经很大的接口文件。

`lsp_inlay_hints.c` 现在单独拥有 LSP inlay hint 生成，不再继续把 hint 逻辑堆在 `lsp_interface.c` 中。它保持原来的触发边界：只为已有的未显式注解 local、field 或无返回注解函数生成类型 hint，不会给每个子表达式额外插入提示。对于 unannotated local，hint 会查看 initializer 对应的共享 facts；如果 initializer 已有 numeric/logical fact，label 会在类型后追加紧凑语义，例如包含 `: int` 和 `range 3..3`，并在同一 label 中保留已有 unsigned range 或短路/逻辑值提示。这样 VSCode 可以在最常见的局部推断位置直接看到 parser/type inference 已证明的事实，同时仍复用 `SZrSemanticContext`，不在 LSP 里复制数值或逻辑推断。

`lsp_completion_semantic_facts.c` 现在单独拥有 completion item 的共享事实追加逻辑。`ZrLanguageServer_Lsp_EnrichCompletionItemMetadata` 仍负责原来的 symbol detail、documentation 和 resolved-type 补充；找到本地变量 symbol 后，它委托 `ZrLanguageServer_Lsp_EnrichCompletionItemSemanticFacts` 查看变量 initializer 的 parser facts。若 initializer 还没有 materialized expression fact，该 helper 通过 `ZrLanguageServer_SemanticAnalyzer_InferExactExpressionType` 触发同一条 type-inference/fact emission 路径，然后读取同一个 `SZrSemanticContext` 的 expression/numeric/logical/ownership facts。当前已验证的 completion detail 追加包括 `Semantic facts: expression binary exact, constant 3, range 3..3`、`Semantic facts: expression literal exact, constant "a\"b\\c\n\t"`、`Semantic facts: logical true, short-circuits`，以及 `Semantic facts: ownership violation: Owned value cannot flow into a plain GC value implicitly`。字符串常量使用 byte-length escaped writer，避免 completion detail 被 decoded newline/tab 或 raw quote 拆坏。这让 VSCode 补全面板也能消费局部表达式事实、局部推断事实和所有权诊断原因，同时避免把事实格式化继续堆进已经很大的 `lsp_interface_support.c`。

`lsp_signature_semantic_facts.c` 现在单独拥有 signature help 参数文档的共享事实格式化。`signature help` 保留原有声明参数 label，但会在对应参数 documentation 上追加 `Argument semantic facts: ...`，内容来自实际 argument 的 numeric/logical/ownership facts。若 overload compatibility 因语义违规 argument 被拒绝，`lsp_signature_help.c` 会回退到唯一同名且同 arity 的声明候选，让 VSCode 仍能显示声明签名和局部 ownership/numeric/logical facts，而不是返回 no signature help。这个回退只服务 LSP 展示健壮性；编译器/semantic analyzer 的诊断仍按原规则产生，不会把非法调用视为类型兼容。

`stdio_inline_value.c` 现在也通过 `ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt` 消费同一局部事实层。`textDocument/inlineValue` 保留原来的 `InlineValueVariableLookup`，用于调试器按变量名取运行时值；如果 local initializer 在当前 `SZrSemanticContext` 中有 numeric/logical fact，则额外在变量名范围上返回 `InlineValueText`。initializer 可以和 `=` 同行，也可以从下一行开始；两种情况下 semantic text 都挂在变量名范围上，例如 `var sum =\n 1 + 2;` 对 `sum` 同时返回 variable lookup 和 `range 3..3`。`return` 表达式和缩进后位于行首的简单 expression statement 现在也走同一查询入口：stdio 只定位表达式范围并优先查询 operator 或 payload token，事实仍由 parser/type inference 和 local semantic query 提供，例如 `return 1 + 2;`、`return 1 +\n 2;`、`return\n 1 + 2;` 或 `1 + 2;` 在表达式范围上返回 `range 3..3`，`true || false;` 在完整 logical expression 范围上返回 `logical true, short-circuits`，`var seed = 2; seed + 3;` 会在 `seed + 3` 范围上返回 `range 5..5`。当 editor 只请求 operator-split expression statement 的 continuation line，例如 request range 从 `1 +\n 2;` 的第二行开始时，stdio 会回溯到 owner expression statement，仍返回覆盖完整表达式的 fact-backed inline value，而 request range 同时包含 owner line 时不重复发射。Unary 起始的 expression statement 也复用这条路径：`!true;` 在 unary expression 范围上返回 `logical false`，`-42;` 在 unary numeric expression 范围上返回 `range -42..-42`。Call/member 起始的 expression statement 只格式化已有 expression payload：`pick(42);` 在 call expression 范围上返回 `call pick args=1`，`seed.value;` 在 member expression 范围上返回 `member value`，`seed[index];` 可追加 `reference member access`。Array literal aggregate 起始的 expression statement 只借用现有 nested element/operator fact：`[1 + 2];` 在 aggregate range 上返回 `range 3..3`，`[true || false];` 返回 `logical true, short-circuits`，不在 stdio 内重建 array 推断。Object aggregate 起始也复用同一查询边界：`{[1 + 2]: 4};` 命中 computed key 的共享 numeric fact 并把 `range 3..3` 锚定在 object expression range；`{a: 1 + 2};` 和 `{\n a: 1 + 2\n};` 也能把 key/value 形状识别为 object-literal-looking statement，并通过内部 `1 + 2` operator query 返回同一 `range 3..3`。这仍不是完整 parser block/object 消歧；任意 block-like `{ ... }` 仍不会被 stdio 当作表达式。数值 fact 目前格式化为 `range min..max`、unsigned range 或 `may overflow`；逻辑 fact 格式化为 `logical true/false` 和 `short-circuits`；call/member payload 格式化为 `call <target> args=N` 和 `member <name>`。逻辑 initializer、return expression 和 supported expression statement 会查询 `&&` / `||` operator 位置来命中已有的结构化 logical expression fact，member statement 会查询 `.` 后的成员 token，call statement 使用表达式起点的 call target token，aggregate statement 会把 `[` / `{` 起始范围交给 `stdio_inline_value_scan.c/.h` 的 query-offset selector 命中内部 operator，避免在 stdio 层复制短路、unary、数值、array/object element 或 call/member 推断。当前 expression-statement 扫描保持保守：只覆盖缩进后行首、分号结束的数字/括号/boolean literal、identifier、`!`、`-`、`[` 或 object-literal-looking `{` 起始形式；continuation owner 回溯只覆盖前一行以 expression continuation operator 结尾的简单 statement；call/member/aggregate inline text 必须来自共享 semantic fact，stdio 不会自己解析源码来合成 payload。

局部表达式查询会优先检查 expression fact 的调用/成员 payload token 范围，再进入普通 AST 嵌套节点查询。这样光标停在 `pick(42)` 的 `pick` 或 `seed.value` 的 `value` 上时，`ExpressionAt` 返回承载调用目标或成员名 payload 的 enclosing primary expression fact，而不是只返回一个缺少调用/成员上下文的裸 identifier fact。

表达式事实查询由 `semantic_analyzer_expression_query.c` 集中实现，避免继续扩大 `semantic_analyzer.c`。查询顺序是：

- 先在 `lsp_local_semantic_query.c` 中命中 call/member payload token 范围。
- 再用 `ZrParser_SemanticFacts_FindExpressionAtPosition` 做精确位置命中。
- 再把位置映射到 AST 表达式节点，并用 `ZrParser_SemanticFacts_FindExpressionByNode` 回查事实。
- 最后在同一可见行范围内选择附近的精确 expression fact，用于修正二元表达式 operator token 和 AST/fact offset 轻微漂移的问题。

`ZrLanguageServer_SemanticAnalyzer_ResolveTypeAtPosition` 也复用同一个 expression fact helper。hover 在 `1 + 2` 的 `+` 上可以拿到 exact `int` 表达式事实，而不是落回不精确类型提示；不完整语法则返回诊断失败或 unknown，不再返回误导性的弱对象推断。

## Validation

`zr_vm_parser_test` 覆盖：

- `ZrParser_Source_Compile` 在表达式已经报告 `missing_right_operand` 时返回 `NULL`，不会继续生成 function 并进入 runtime。

`zr_vm_semantic_facts_test` 覆盖：

- 语义上下文会初始化六类事实数组。
- 表达式事实可以按 AST 节点和源码位置回查。
- 逻辑事实可以按 AST 节点和源码位置回查，`FindLogicalAtPosition` 会命中覆盖 query token 的短路事实。
- `ZrParser_SemanticContext_Reset` 会清空事实数组。

`zr_vm_type_inference_test` 覆盖：

- 字面量表达式推断写入表达式事实、int64 常量和数值事实。
- 二元算术表达式推断写入 binary 表达式事实和数值提升事实。

`zr_vm_expression_fact_emission_test` 覆盖：

- `Function Call Inference Records Expression Fact` 验证 `var result = pick(42);` 的 initializer 在函数返回类型推断成功后写入 exact expression fact，kind 为当前 parser AST 形态对应的 `ZR_SEMANTIC_EXPRESSION_FACT_MEMBER`，推断类型为 `int64`，且不会为函数调用伪造 numeric fact。
- `Function Call Expression Fact Records Call Target Payload` 验证 `pick(42)` 的 expression fact 记录 `callTargetName=pick`、目标 token range、实参数量和 named-argument 标记。
- `Member Expression Fact Records Member Payload` 验证 `seed.value` 的 expression fact 记录 `memberName=value`、成员 token range 和 computed 标记。
- `Integer Literal Numeric Fact Records Exact Range` 验证 `42` 的 numeric fact 写入 `hasRange=true`、`minValue=42`、`maxValue=42` 和 `mayOverflow=false`。
- `Binary Integer Numeric Fact Records Exact Range` 与 `Binary Integer Multiply Numeric Fact Records Exact Range` 验证 `1 + 2`、`3 * 4` 的 promotion fact 写入精确结果范围。
- `Binary Integer Numeric Fact Records Exact Range` 同时验证 `1 + 2` 的 binary AST 和 numeric fact 范围覆盖完整表达式 `0..5`，保证 operator 位置不会被排除在事实范围外。

`zr_vm_reference_fact_emission_test` 覆盖：

- `Assignment Identifier Records Write Reference Fact` 验证已注册 identifier assignment target 写入 resolved `ZR_SEMANTIC_REFERENCE_WRITE`，并指回声明 token。
- `Member Access Records Member Reference Fact` 验证 `seed.value` 在 `value` token 上记录 unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS`，并保留 invalid symbol/type ids。
- `Assignment Member Targets Record Member Write Reference Facts` 验证 `seed.value = 3` 和 `seed[index] = 4` 分别在 property token 与 computed index expression range 上记录 unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_WRITE`。
- `Resolved Function Call Records Call Reference Fact` 验证 `pick(42)` 写入 resolved `ZR_SEMANTIC_REFERENCE_CALL`，并指回函数声明名 token。
- `Logical Expression Range Covers Operator Gap` 验证 `true || false` 的 logical AST 范围覆盖完整表达式 `0..13`。
- `Logical Expression Inference Records Bool Fact` 验证 `true || false` 通过 type inference 得到 exact `bool` expression fact，且 fact range 与 logical AST range 一致。
- `Unary Logical Not Records Exact Logical Fact` 验证 `!true` 的 unary expression AST/fact range 覆盖 `0..5`，type inference 写入 exact `bool` expression fact、`ZR_SEMANTIC_LOGICAL_FACT_ALWAYS_FALSE`、`knownValue=false`，并把 logical fact 的 `relatedNode` 指向 operand。
- `Binary Integer Numeric Fact Marks Overflow` 与 `Binary Integer Multiply Numeric Fact Marks Overflow` 验证 `9223372036854775807 + 1` 和 `3037000500 * 3037000500` 不伪造范围，并标记 `mayOverflow=true`。
- `Integer Literal Numeric Fact Records Exact Range` 同时验证 literal AST range 和 numeric fact range 都对齐到真实 token `0..2`，防止 token 消费后的位置漂移破坏局部查询。
- `Object Literal Value Expression Records Nested Facts` 验证 `{a: 1 + 2}` 的 static property key 保持 `keyIsComputed=false`，并且 nested value expression 写入 binary expression fact 和 exact `3..3` numeric promotion fact。
- `Object Literal Computed Key Expression Records Nested Facts` 验证 `{[1 + 2]: 4}` 的 property key 记录 `keyIsComputed=true`，object literal 本身写入 object expression fact，computed key 写入 binary expression fact 和 exact `3..3` numeric promotion fact。

`zr_vm_object_literal_key_lowering_test` 覆盖：

- `Computed Identifier Object Key Emits Index Set` 验证 `{[key]: 1, name: 2}` 编译后正好包含一个 `SET_BY_INDEX` 和一个 `SET_MEMBER`，防止 bracketed identifier key 被错误降级为 plain property name。

`zr_vm_cli_repl_e2e_test` 覆盖：

- `:type 1 + 2` 通过 parser/type inference 输出 `Type: int` 和 `Numeric range: 3..3`。
- 同一 `:type` 查询不会执行表达式，因此不会打印普通 REPL expression execution 的结果 `3`。
- `var seed = 2;` 后的 `:type seed + 3` 通过同一会话的 declaration context 输出 `Numeric range: 5..5`，并且不会执行表达式打印 `5`。

`tests/cli/repl_type_call_member_smoke.js` 覆盖：

- `:type pick(42)` 通过 parser expression fact 输出 `Call: pick args=1`。
- `:type seed.value` 通过 parser expression fact 输出 `Member: value`，并通过 parser reference fact 输出 `Reference: member value`。
- `:type seed.value = 3` 通过 parser reference fact 输出 `Reference: member write value`，不会对 unresolved member target 输出 fake declaration location。
- 同一查询不会执行函数调用，也不会报 `failed to infer expression type`。

`tests/cli/repl_type_call_reference_smoke.js` 覆盖：

- 同一 REPL 会话先声明 `func pick(value: int): int { ... }` 后，`:type pick(1 + 2)` 通过 prior function replay 推断 `Type: int`。
- 同一查询通过 parser expression fact 输出 `Call: pick args=1`，并继续下钻到 call argument 的 `Expression: binary exact` / `Constant: 3`。
- 同一查询通过 parser reference fact 输出 resolved `Reference: call pick` 和 `Declared at:`，同时避免输出 literal leaf constants `1` / `2`。

`zr_vm_language_server_local_semantic_hover_test` 覆盖：

- `LSP Local Expression Hover Surfaces Assignment Write Reference Fact` 验证 `var seed = 1; seed = 3;` 的 assignment target token 通过 `ExpressionAt` 返回 `ZR_SEMANTIC_REFERENCE_WRITE`，plain hover 输出 `Reference: write` / `Symbol: seed` / `Declared at: 1:5`，rich hover 同步暴露 `reference`、`symbol` 和 `declaration` roles。
- `LSP Local Hover Surfaces Call/Member Payloads` 先确认 `ExpressionAt` 在 `pick(42)` 的 call target token 和 `seed.value` 的 member token 上命中带 payload 的 expression fact，再验证 plain hover 输出 `Call: pick args=1` / `Member: value`，rich hover 暴露 `call` / `member` roles。

`zr_vm_language_server_expression_fact_hover_test` 覆盖：

- `LSP Hover Surfaces Expression Fact Kind And Constant` 验证 `1 + 2` 的 operator-position local query 命中 binary exact expression fact 和 constant `3`，plain/public hover 输出 `Expression: binary exact` / `Constant: 3`，rich hover 同步暴露 `expression` / `constant` roles。
- `LSP Hover Escapes String Constant Payload` 验证 string literal 的 decoded payload 含有 quote、backslash、newline 和 tab 时，plain/public hover 与 rich-hover `constant` role 都显示 `Constant: "a\"b\\c\n\t"`，不会输出 raw embedded quote 或 decoded control characters。

`zr_vm_language_server_local_semantic_query_test` 覆盖：

- `LSP Local Expression Query Returns Numeric Range Fact` 验证 `1 + 2` 的 `+` operator 位置命中二元表达式 fact，并返回 exact numeric promotion range `3..3`。
- `LSP Local Expression Query Returns Numeric Overflow Fact` 验证 `9223372036854775807 + 1` 的 `+` operator 位置返回 overflow numeric fact，不伪造 range。
- `LSP Local Expression Query Returns Logical Short Circuit Fact` 验证 `true || false` 的 `||` operator 位置返回 exact `bool` expression fact、short-circuit logical fact 和 short-circuit reachability fact。
- `LSP Local Expression Query Returns Conditional Branch Fact` 验证 `true ? 1 : 2` 的 skipped alternate branch 位置返回 conditional expression fact、selected numeric range `1..1`、constant-condition logical fact 和 `ZR_SEMANTIC_REACHABILITY_CONSTANT_BRANCH`。
- `LSP Local Reference Query Returns Member Access Fact` 验证 `seed.value` 的 `value` token 位置返回 unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS`，而 assignment target 上的 `value` 仍由 member-write fact 优先。

`zr_vm_language_server_inlay_semantic_facts_test` 覆盖：

- `LSP Inlay Hint Uses Initializer Numeric Fact` 先以 RED 确认原 inlay label 只有 `: int`，随后验证 `var sum = 1 + 2;` 的未注解 local hint 会复用 initializer numeric fact，同一 label 至少包含 `: int` 和 `range 3..3`，并允许当前 numeric fact 追加 unsigned range。
- `LSP Completion Detail Uses Initializer Numeric Fact` 先以 RED 确认 `sum` completion detail 只有 `private int`，随后验证补全 detail 复用 initializer numeric fact 并包含 `range 3..3`。
- `LSP Completion Detail Uses Initializer Expression Fact` 先以 RED 确认 `sum` completion detail 只有 numeric facts，随后验证补全 detail 同时包含 `expression binary exact`、`constant 3` 和 `range 3..3`。
- `LSP Completion Detail Escapes Initializer String Expression Fact` 先以 RED 确认 `label` completion detail 只有 `expression literal exact`，随后验证字符串常量 `a"b\c` 加 newline/tab 会显示为一行 escaped `constant "a\"b\\c\n\t"`，且不会出现 raw `constant "a"b`。
- `LSP Completion Detail Uses Initializer Logical Fact` 先以 RED 确认 `flag` completion detail 只有 `private bool`，随后验证补全 detail 复用 initializer logical fact 并包含 `logical true` 与 `short-circuits`。
- `LSP Completion Detail Uses Initializer Ownership Fact` 先以 RED 确认 `plainFromUnique` completion detail 只有 `private Resource`，随后验证补全 detail 复用 initializer ownership fact 并包含 `ownership violation` 与 `Owned value cannot flow into a plain GC value implicitly`。
- `LSP Signature Help Parameter Docs Use Argument Semantic Facts` 验证 `pick(1 + 2, true || false)` 的 signature parameter docs 复用实参 facts，并包含 `range 3..3`、`logical true` 和 `short-circuits`。
- `LSP Signature Help Parameter Docs Use Argument Ownership Fact` 先以 RED 确认 ownership-invalid `observe(resource)` 返回 no signature help，随后验证 signature help 仍显示声明参数文档，并包含 `ownership violation` 与 `Weak value must be upgraded`。

`tests/language_server/stdio_smoke.js` 覆盖：

- `textDocument/inlineValue` 对 local initializer 继续返回变量 lookup，并在变量名范围上追加 `range 20..20` 和 `logical true, short-circuits`。
- `textDocument/inlineValue` 对 `return 1 + 2;` 的非 local 表达式返回 `InlineValueText`，在 `1 + 2` 表达式范围上显示 `range 3..3`，证明 stdio 层通过 local semantic query 消费 parser numeric fact，而不是只支持变量 initializer。
- `textDocument/inlineValue` 对缩进后行首的 `1 + 2;` 和 `true || false;` expression statement 返回 `InlineValueText`，分别在表达式范围上显示 `range 3..3` 和 `logical true, short-circuits`。这条路径只做范围扫描和 query offset 选择，不新增 stdio 自己的 numeric/logical 推断。

`tests/language_server/stdio_inline_value_semantic_smoke.js` 覆盖：

- `textDocument/inlineValue` 对缩进后行首的 identifier-led expression statement `seed + 3;` 返回 `InlineValueText`，在 `seed + 3` 表达式范围上显示 `range 5..5`。下层 `zr_vm_language_server_local_semantic_query_test` 同时覆盖 `ZR_AST_EXPRESSION_STATEMENT` 的 root expression 会在局部变量绑定仍可见时 materialize numeric fact。
- `textDocument/inlineValue` 对 `return` 后换行开始的表达式 `return\n 1 + 2;` 只返回一个 `range 3..3` fact-backed inline text，并把 LSP range 锚定在实际表达式 `1 + 2` 上，不再同时返回覆盖换行空白的 return fact 和 continuation expression fact。
- `textDocument/inlineValue` 对缩进后行首的 unary expression statement `!true;` 和 `-42;` 返回 `InlineValueText`，分别在完整 unary expression 范围上显示 `logical false` 和 `range -42..-42`。stdio 层只把 `!` / `-` 识别为可查询的 statement start，具体 logical/numeric fact 仍来自 parser/type inference 和 local semantic query。
- `textDocument/inlineValue` 对缩进后行首的 call/member expression statement `pick(42);` 和 `seed.value;` 返回 `InlineValueText`，分别显示 `call pick args=1` 和 `member value`。stdio 只选择 call target/member token 作为 query position，payload 仍来自 `SZrSemanticExpressionFact`。
- `textDocument/inlineValue` 对缩进后行首的 array literal aggregate expression statement `[1 + 2];` 和 `[true || false];` 返回 `InlineValueText`，分别在 aggregate expression 范围上显示 nested fact 对应的 `range 3..3` 和 `logical true, short-circuits`。stdio 只把 `[` 识别为保守 statement start，query 和 fact text 仍来自共享 local semantic query。
- `textDocument/inlineValue` 当 request range 从 operator-split expression statement 的 continuation line 开始时，仍回溯到 owner statement 并返回完整表达式范围上的 semantic fact，例如 `1 +\n 2;` 的第二行请求返回 `range 3..3`，range 覆盖 `1 +` 到 `2`。

`tests/cli/repl_type_reachability_smoke.js` 覆盖：

- `:type true || false` 输出 `Type: bool`、`Logical flow: short-circuits right operand` 和 `Reachability: unreachable because short-circuit skips evaluation`。
- 该 smoke 不执行目标表达式，证明 REPL 通过 parser/type inference 写入的 shared logical/reachability facts 显示短路语义，而不是把 `true` 作为运行时结果打印。

`tests/cli/repl_type_conditional_branch_smoke.js` 覆盖：

- `:type true ? 1 : 2` 输出 `Type: int`、`Numeric range: 1..1`、`Expression: conditional exact`、constant condition 的 `Logical value: true`，以及 skipped alternate 的 `Reachability: unreachable because a constant branch skips evaluation`。
- 该 smoke 不执行目标表达式，证明 REPL 通过 parser/type inference 写入的 shared conditional/logical/reachability facts 显示分支语义，而不是把选中分支作为运行时结果打印。

`tests/cli/repl_type_nested_numeric_smoke.js` 覆盖：

- `:type [1 + 2]` 输出 `Type: int[1]<int>`、`Expression: array exact` 和 nested element expression 的 `Numeric range: 3..3`。
- 该 smoke 不执行目标 aggregate expression，证明 REPL 通过 parser/type inference 已写入的 nested numeric facts 显示 aggregate 子表达式语义，而不是只读取 array root expression fact。

`tests/cli/repl_type_nested_expression_fact_smoke.js` 覆盖：

- `:type [1 + 2]` 输出 aggregate root 的 `Expression: array exact`，以及 nested element expression 的 `Expression: binary exact` 和 `Constant: 3`。
- 该 smoke 明确拒绝 leaf `Constant: 1` / `Constant: 2`，证明 REPL expression display 会显示结构性 nested facts，但不会把所有 literal leaf facts 都倾倒出来。

`tests/cli/repl_type_nested_ownership_smoke.js` 覆盖：

- `var owner: %unique int;` 后的 `:type [%borrow(owner)]` 输出 aggregate borrowed type、array root expression fact、nested ownership builtin expression fact，以及 nested `Ownership: borrow %borrowed`。
- 该 smoke 证明 REPL 通过 parser/type inference 已写入的 nested ownership facts 显示 aggregate 子表达式所有权语义，而不是只读取 array root 或 ownership builtin expression fact。

`tests/cli/repl_expression_aggregate_smoke.js` 覆盖：

- 普通 REPL 提交 `[1 + 2][0]` 会被识别为完整裸表达式，包装执行后打印 `3`。
- 同一 smoke 拒绝 parser 把行首 `[` 当作 statement/block 起始后报 `Expected ';'` / `期望 ';'`，也拒绝走 `:type` 分析路径的 `failed to infer expression type`。
- 同一 smoke 拒绝数组字面量元素初始化时出现 `SET_BY_INDEX` runtime failure，证明 REPL 执行路径下的 aggregate expression 真的完成 array literal 构造和 index read。

`tests/cli/repl_expression_array_display_smoke.js` 覆盖：

- 普通 REPL 提交 `[1 + 2]` 会被识别为完整裸表达式，包装执行后通过 shared value conversion 打印 `[3]`。
- 同一 smoke 拒绝 `ZrCore_Value_ConvertToString` 在 array result display 上触发断言退出。
- 同一 smoke 拒绝走 `:type` 分析路径或数组字面量元素初始化的 `SET_BY_INDEX` runtime failure，证明这是正常执行结果展示路径。

`tests/cli/repl_expression_object_smoke.js` 覆盖：

- 普通 REPL 提交 `{a: 1 + 2}.a` 会被识别为完整裸表达式，包装执行后打印 `3`。
- 普通 REPL 提交 `{[1 + 2]: 4}[3]` 会保留 parser/compiler 的计算键对象字面量路径，包装执行后打印 `4`。
- 同一 smoke 拒绝旧的行首 `{` 语句路径错误 `Expected ';'` / `期望 ';'`，也拒绝 `:type` 分析路径和对象初始化 opcode failure。

`tests/parser/test_instruction_execution.c` 中的 `CREATE_ARRAY With Elements Execution` 覆盖：

- `[1, 2, 3]` 编译执行后返回 array value。
- array 的 `length` 成员为 `3`，索引 `0/1/2` 分别保存 `1/2/3`。
- 该测试锁定 compiler/runtime 低层数组字面量元素写入路径；它不是 REPL 显示层测试。

`zr_vm_language_server_call_member_semantic_query_test` 覆盖：

- `LSP Local Expression Query Returns Call Target Payload` 验证 `pick(42)` 的 `pick` token 位置返回 call payload expression fact。
- `LSP Local Expression Query Returns Member Payload` 验证 `seed.value` 的 `value` token 位置返回 member payload expression fact，而不是缺少成员上下文的普通 identifier fact。

`zr_vm_language_server_semantic_analyzer_test` 覆盖：

- `Semantic Analyzer Records Reference Facts With Precise Ranges` 验证同一个 `value` 的声明 token 和使用 token 都能从 `referenceFacts` 查到。
- 声明事实保留 `ZR_SEMANTIC_REFERENCE_DECLARATION`、非零语义 symbol id 和完整 token 范围。
- 使用事实保留 `ZR_SEMANTIC_REFERENCE_READ`、同一声明 range、非零语义 symbol id 和完整 token 范围。
- `Semantic Analyzer Records Reachability Facts For Unreachable Statements` 验证 `return/throw` 之后的不可达语句可按位置查到 reachability fact，并保留 `AFTER_RETURN/AFTER_THROW` cause 和 cause node。
- `Semantic Analyzer Records Short Circuit Logical Facts` 验证 `true || rhs` 与 `false && rhs` 会记录已知逻辑结果，并能按 RHS token 查到 `ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT` 事实。
- `Semantic Analyzer Reports Declared Ownership Initializer Mismatch` 验证 `%shared` 初始化 `%unique` 会产生 `ownership_mismatch`，并在初始化表达式上写入 ownership fact。
- `Semantic Analyzer Reports Owner To Plain Initializer Escape` 验证 `%unique/%shared` 到 plain GC declaration 会产生 `owner_to_plain_escape`，并建议 `%detach(...)`。
- `Semantic Analyzer Reports Return Ownership Mismatch` 验证 return path 的所有权限定不匹配保留 cause/suggestion 和 ownership fact。
- `Semantic Analyzer Reports Borrowed Return Escape` 验证 `return %borrow(resource);` 不再被类型兼容性误放行，而是产生 `borrow_escape` 和对应 fact。
- `Semantic Analyzer Reports Function Argument Ownership Mismatch`、`Semantic Analyzer Reports Weak Argument Requires Upgrade` 和 `Semantic Analyzer Reports Method Argument Ownership Mismatch` 验证函数/方法调用参数的所有权诊断、cause/suggestion 和 fact。

`zr_vm_language_server_ownership_diagnostics_test` 覆盖：

- `Semantic Analyzer Reports Loaned Return Escape` 验证 `return %loan(resource);` 在 `%loaned Resource` return path 上产生 `loan_escape`，诊断包含 message/cause/suggestion，并在 `%loan(resource)` ownership builtin 位置写入 `ZR_SEMANTIC_OWNERSHIP_FACT_ERROR`、`ZR_OWNERSHIP_QUALIFIER_LOANED` 和 `isViolation=true`。

`zr_vm_language_server_lsp_interface_test` 覆盖：

- `LSP Get Parser Diagnostics` 验证 `var x = ;` 不再只报告 `Expected primary expression`，而是暴露 `missing_expression_after_assignment`、`Missing expression after '='` 和 `Add an expression before ';'`，并保持错误范围落在分号 token 上。
- `LSP Get Missing Right Operand Parser Diagnostic` 验证 `1 +` 暴露 `missing_right_operand`、`Missing expression after '+'` 和补右侧表达式的 suggestion。
- `LSP Semantic Query Unifies Local Symbol Navigation And Hover` 同时验证 `ReferenceAt` 能在 `return result;` 的使用位置返回 `ZR_SEMANTIC_REFERENCE_READ` fact，并指回 `var result` 的声明 token。
- `LSP Hover Includes Local Reference Fact Payload` 验证同一 `return result;` 使用位置的 hover 会保留 symbol hover 的 `result`/`int` 内容，并追加共享 reference fact payload：`Reference: read` 和 `Declared at: 2:9`。
- `LSP Local Semantic Query Returns Expression Fact` 验证 `return 1 + 2;` 的 `+` 位置会返回 exact expression fact，推断类型为 `int`，hover 也复用该事实而不是提示 `cannot infer exact type`。
- `LSP Local Semantic Query Reports Diagnostic Failure For Incomplete Expression` 验证 `var x = ;` 的分号位置返回 `ZR_LSP_LOCAL_SEMANTIC_QUERY_DIAGNOSTIC_FAILURE`，并携带 `missing_expression_after_assignment` 诊断；同一位置的 hover 不继续走 fallback 语义推断。

`zr_vm_language_server_parser_diagnostics_test` 覆盖：

- `LSP Missing Index Close Parser Diagnostic` 验证 `value[0;` 暴露 `missing_index_close`、`Missing closing ']' in index access` 和补 `]` 的 suggestion。
- `LSP Missing Member Name Parser Diagnostic` 验证 `value.` 暴露 `missing_member_name`、`Missing member name after '.'` 和补成员名或移除成员访问的 suggestion。该覆盖位于 focused parser-diagnostics target 中，避免每个语法诊断都继续扩大已经很大的 `test_lsp_interface.c`。
- `LSP Missing Call Close Parser Diagnostic` 验证 `pick(1 + 2;` 暴露 `missing_call_close`、`Missing closing ')' in function call` 和补 `)` 的 suggestion。
- `LSP Missing Group Close Parser Diagnostic` 验证 `return (1 + 2` 暴露 `missing_group_close`、`Missing closing ')' in grouped expression` 和补 `)` 的 suggestion。该诊断与 function call 的 `missing_call_close` 分开，方便编辑器区分“括号分组缺闭合”和“调用参数列表缺闭合”。
- `LSP Missing Array Close Parser Diagnostic` 验证 `return [1, 2` 暴露 `missing_array_close`、`Missing closing ']' in array literal` 和补 `]` 的 suggestion。该诊断与 computed index 的 `missing_index_close` 分开，方便编辑器区分“字面量缺闭合”和“成员/index access 缺闭合”。
- `LSP Missing Array Element Separator Parser Diagnostic` 验证 `return [1 2];` 暴露 `missing_array_element_separator`、`Missing separator between array elements` 和插入 `,` 或 `;` 的 suggestion。parser 会在后一个元素 token 处报告真实的分隔符问题，而不是退化成 array literal 缺 `]`。
- `LSP Missing Object Close Parser Diagnostic` 验证 `return {a: 1` 暴露 `missing_object_close`、`Missing closing '}' in object literal` 和补 `}` 的 suggestion。该诊断覆盖局部编辑中常见的 object literal 未闭合场景，避免退回 generic expected-token fallback。
- `LSP Missing Object Computed Key Close Parser Diagnostic` 验证 `return {[seed: 1}` 暴露 `missing_object_computed_key_close`、`Missing closing ']' in computed object key` 和在 `:` 前补 `]` 的 suggestion。该诊断与 computed member/index 的 `missing_index_close` 分开，方便编辑器区分“object key 语法缺闭合”和“成员/index access 缺闭合”。
- `LSP Missing Object Property Colon Parser Diagnostic` 验证 `{a 1}` 暴露 `missing_object_property_colon`、`Missing ':' after object property key` 和补 `:` 的 suggestion。parser 在报告结构化诊断后继续从当前 value token 做恢复解析，避免只给出 generic expected-token fallback。
- `LSP Missing Object Property Separator Parser Diagnostic` 验证 `{a: 1 b: 2}` 暴露 `missing_object_property_separator`、`Missing separator between object properties` 和插入 `,` 或 `;` 的 suggestion。parser 会在后一个 property key token 处报告真实的分隔符问题，而不是退化成 object literal 缺 `}`。
- `LSP Missing Condition Parser Diagnostics` 验证空 `if ()` / `while ()` / `switch ()` 暴露 `missing_condition`，并验证 `if (ready { ... }`、`while (ready { ... }` 和 `switch (choice { ... }` 暴露 `missing_condition_close`。`switch` 现在和 `if` / `while` 一样，在 `(` 后立即识别空条件并报告 `Missing condition inside 'switch'`，也会在控制条件表达式解析完成后报告 `Missing ')' after 'switch' condition` 和补 `)` 的 suggestion，而不是只给出 generic expected-token fallback。
- `LSP Missing Statement Semicolon Parser Diagnostics` 验证 `return 1\nvar next = 2;`、`1 + 2\nvar next = 3;`、`var seed = 1\nvar next = 2;`、`%module "main"\nvar next = 2;`、loop body 中的 `break\nvar next = 2;` 和 `continue\nvar next = 2;`，以及 `throw 1\nvar next = 2;`、`out 1\nvar next = 2;` 和 `%using resource\nvar next = 2;` 暴露 `missing_statement_semicolon`，并分别给出 `Missing ';' after return statement`、`Missing ';' after expression statement`、`Missing ';' after variable declaration statement`、`Missing ';' after module declaration statement`、`Missing ';' after break statement`、`Missing ';' after continue statement`、`Missing ';' after throw statement`、`Missing ';' after out statement` 或 `Missing ';' after using statement` 以及补 `;` 的 suggestion。parser 在已成功解析 statement body 后检查 terminator；变量声明不再把下一条 `var` declaration 当作隐式可省略分隔，而是在下一条声明 token 处报告 statement-specific terminator 诊断；module declaration 同样在解析 module path 后用结构化 terminator reporter，避免 `%module` 局部编辑只暴露 generic expected-token fallback；`break` / `continue` 还会先识别下一行开始的新 statement，避免把下一条语句的第一个 token 当作 loop-control expression 后只呈现 generic expected-token fallback；`throw` / `out` 也在表达式 body 解析完成后复用同一结构化 terminator reporter，而不是退回 generic expected-token fallback；单表达式 `%using` declaration 同样在 resource 表达式解析完成后报告 statement-specific terminator 诊断。
- 同一 `LSP Missing Statement Semicolon Parser Diagnostics` suite 也验证 `interface Readable { read(value: int): int }` 暴露 `missing_statement_semicolon`，问题文本为 `Missing ';' after interface method signature statement`，suggestion 为 `Insert ';' after the interface method signature statement`。`parse_interface_method_signature` 在参数、返回类型和 where clause 解析完成后复用 statement terminator reporter，避免 interface 局部编辑退回 generic expected-token fallback。
- 同一 suite 还验证 `interface Callable { @call(value: int): int }` 暴露 `missing_statement_semicolon`，问题文本为 `Missing ';' after interface meta signature statement`，suggestion 为 `Insert ';' after the interface meta signature statement`。`parse_interface_meta_signature` 在参数列表和返回类型解析完成后复用 statement terminator reporter，避免 meta-signature 局部编辑只呈现 generic expected-token fallback。
- 同一 suite 还验证 `interface Sized { get length: int }` 暴露 `missing_statement_semicolon`，问题文本为 `Missing ';' after interface property signature statement`，suggestion 为 `Insert ';' after the interface property signature statement`。interface member dispatch 现在也会把 line-start `get` / `set` 直接路由到 `parse_interface_property_signature`，所以无访问修饰符的 property signature 和带 `pub` / `pri` / `pro` 的 property signature 一样能进入结构化 terminator reporter。
- 同一 suite 还验证 `interface Entity { var id: int }` 暴露 `missing_statement_semicolon`，问题文本为 `Missing ';' after interface field declaration statement`，suggestion 为 `Insert ';' after the interface field declaration statement`。`parse_interface_field_declaration` 在字段名和可选类型注解解析完成后复用 statement terminator reporter，使 interface field / method / property / meta 四类 member terminator 都返回稳定 code、problem text 和 repair suggestion。
- 同一 suite 还验证 `class Entity { var id: int }` 暴露 `missing_statement_semicolon`，问题文本为 `Missing ';' after class field declaration statement`，suggestion 为 `Insert ';' after the class field declaration statement`。`parse_class_field` 在字段名、可选类型注解、默认值和 where clause 解析完成后复用 statement terminator reporter，避免 class field 局部编辑退回 generic expected-token fallback。
- 同一 suite 还验证 `class Sized { get length: int }` 和 `class Sized { set length(value: int) }` 暴露 `missing_statement_semicolon`，问题文本分别为 `Missing ';' after class getter statement` 和 `Missing ';' after class setter statement`，suggestion 分别为 `Insert ';' after the class getter statement` 和 `Insert ';' after the class setter statement`。class member dispatch 现在会把 line-start `get` / `set` 路由到 property parser；`parse_property_get` / `parse_property_set` 在 accessor signature 后区分合法 `;`、合法 `{ ... }` body，以及缺失 terminator/body 的局部编辑场景，避免 class accessor 退回 generic block/expected-token fallback。
- 同一 suite 还验证 `class Box { func read(value: int): int }` 和 `class Callable { @call(value: int): int }` 暴露 `missing_statement_semicolon`，问题文本分别为 `Missing ';' after class method statement` 和 `Missing ';' after class meta function statement`，suggestion 分别为 `Insert ';' after the class method statement` 和 `Insert ';' after the class meta function statement`。`parse_class_method` / `parse_class_meta_function` 在 signature 后同样区分合法 `;`、合法 `{ ... }` body，以及缺失 terminator/body 的局部编辑场景，使 class field / accessor / method / meta 四类 member terminator 都返回稳定 code、problem text 和 repair suggestion。
- `LSP Missing Class Member Parameter List Close Parser Diagnostics` 覆盖 `class Sized { set length(value: int { return value; } }` 和 `class Callable { @call(value: int: int { return value; } }`，验证 class setter accessor 与 class meta function 在参数列表已经开始但缺少 `)` 时暴露 `missing_parameter_list_close`、`Missing closing ')' in function declaration parameters` 和补 `)` 的 suggestion，而不是退回 raw expected-token fallback。该覆盖放在新的 focused class-member parser diagnostics target 中，避免继续扩大已经接近大文件阈值的 `test_lsp_parser_diagnostics.c`。

`zr_vm_language_server_declaration_parser_diagnostics_test` 覆盖：

- `LSP Missing Declaration Body Open Parser Diagnostics` 验证 `class Box`、`interface Sized`、`func pick(): int`、`enum Tone`、`%extern("fixture")` 和 `%test("smoke")` 暴露 `missing_declaration_body_open`，分别给出 class/interface/function/enum/test declaration body 或 extern block body 的问题文本，并建议在对应 declaration/header 后插入 `{`。该覆盖放在新的 focused declaration parser diagnostics target 中，避免继续扩大已经接近大文件阈值的 `test_lsp_parser_diagnostics.c`。
- `LSP Missing Extern Spec Close Parser Diagnostic` 验证 `%extern("fixture" { NativeAdd(value: int): int; }` 暴露 `missing_extern_spec_close`、`Missing closing ')' in extern block spec` 和在 extern block body 前补 `)` 的 suggestion。该覆盖复用 focused declaration parser diagnostics target，验证 `parse_extern_block` 不再在 extern block spec 的 `)` 缺失场景只暴露 expected-token fallback。
- `LSP Missing Test Name Close Parser Diagnostic` 验证 `%test("smoke" { return 1; }` 暴露 `missing_test_name_close`、`Missing closing ')' in test declaration name` 和在 test body 前补 `)` 的 suggestion。该覆盖复用 focused declaration parser diagnostics target，验证 `parse_test_declaration` 不再在 test declaration name 的 `)` 缺失场景只暴露 expected-token fallback。
- `LSP Missing Declaration Body Close Parser Diagnostics` 验证 `class Box { var id: int;`、`interface Sized { get length: int;`、`enum Tone { warm,`、`%extern("fixture") { NativeAdd(value: int): int;`、`func pick(): int { return 1;` 和 `%test("smoke") { return 1;` 暴露 `missing_declaration_body_close`，分别给出 class/interface/enum/extern/function/test declaration body 的具体缺 `}` 问题文本，并建议插入 `}` 关闭对应 declaration body。该覆盖复用 focused declaration parser diagnostics target，验证这些 body 已打开但输入结束的局部编辑场景不再退回 generic expected-token fallback 或过宽的 generic block-close 语义。

`zr_vm_language_server_statement_parser_diagnostics_test` 覆盖：

- `LSP Missing Statement Body Open Parser Diagnostics` 验证 `if (ready)\nreturn 1;`、`while (ready)\nreturn 1;`、`for (;;)\nreturn 1;`、`for (var item in items)\nreturn item;`、`switch (choice)\nreturn 1;`、`switch (choice) { (1)\nreturn 1; }`、`switch (choice) { ()\nreturn 1; }`、`if (ready) { return 1; } else\nreturn 2;`、`try\nreturn 1;`、`try { throw 1; } catch (error)\nreturn 2;`、`try { return 1; } finally\nreturn 2;` 和 `%using (resource)\nreturn resource;` 暴露 `missing_statement_body_open`，分别给出 if/while/for/foreach/switch/switch-case/switch-default/else/try/catch/finally/using statement body 的问题文本，并建议在对应 statement header 后插入 `{`。该覆盖放在新的 focused statement parser diagnostics target 中，避免继续扩大已经接近大文件阈值的 `test_lsp_parser_diagnostics.c`。
- `LSP Missing Block Close Parser Diagnostic` 验证 `if (ready) { return 1;` 暴露 `missing_block_close`、`Missing closing '}' for block` 和补 `}` 的 suggestion。该覆盖复用 focused statement parser diagnostics target，验证通用 `parse_block` 不再只暴露 expected-token fallback。
- `LSP Missing Catch Pattern Close Parser Diagnostic` 验证 `try { throw 1; } catch (error { return 2; }` 暴露 `missing_catch_pattern_close`、`Missing closing ')' in catch pattern` 和在 catch body 前补 `)` 的 suggestion。该覆盖复用 focused statement parser diagnostics target，验证 `parse_try_catch_finally_statement` 不再在 catch header 的 `)` 缺失场景只暴露 expected-token fallback。
- `LSP Missing Using Resource Close Parser Diagnostic` 验证 `%using (resource { return resource; }` 暴露 `missing_using_resource_close`、`Missing closing ')' in using resource` 和在 using body 前补 `)` 的 suggestion。该覆盖复用 focused statement parser diagnostics target，验证 `parse_using_statement_body` 不再在 block-scoped using header 的 `)` 缺失场景只暴露 expected-token fallback。
- `LSP Missing For Header Close Parser Diagnostic` 验证 `for (; ready; ready = false { return 1; }` 暴露 `missing_for_header_close`、`Missing closing ')' in for header` 和在 loop body 前补 `)` 的 suggestion。该覆盖复用 focused statement parser diagnostics target，验证 traditional `parse_for_loop` 不再在 for header 的 `)` 缺失场景只暴露 expected-token fallback。
- `LSP Missing For Header Separator Parser Diagnostic` 验证 `for (i = 0 i < 3; i = i + 1) { out i; }`、`for (i = 0; i < 3 i = i + 1) { out i; }` 和 `for (var i = 0 i < 3; i = i + 1) { out i; }` 暴露 `missing_for_header_separator`、`Missing ';' between for header clauses` 和在 clauses 之间补 `;` 的 suggestion。该覆盖复用 focused statement parser diagnostics target，验证 expression initializer、condition clause 和 variable initializer 三类 traditional `for` header 都不会退回普通 statement semicolon 或 foreach `in` keyword 诊断。
- `LSP Missing Foreach Header Close Parser Diagnostic` 验证 `for (var item in items { return item; }` 暴露 `missing_foreach_header_close`、`Missing closing ')' in foreach header` 和在 loop body 前补 `)` 的 suggestion。该覆盖复用 focused statement parser diagnostics target，验证 `parse_foreach_loop` 不再在 foreach header 的 `)` 缺失场景只暴露 expected-token fallback。
- `LSP Missing Foreach In Keyword Parser Diagnostic` 验证 `for (var item items) { return item; }` 暴露 `missing_foreach_in_keyword`、`Missing 'in' in foreach header` 和在 pattern 与 iterable expression 之间补 `in` 的 suggestion。该覆盖复用 focused statement parser diagnostics target，验证 `parse_foreach_loop` 不再在 foreach header 缺 `in` 场景只暴露 expected-token fallback。
- `LSP Missing Switch Case Header Close Parser Diagnostic` 验证 `switch (choice) { (1 { return 1; } }` 暴露 `missing_switch_case_header_close`、`Missing closing ')' in switch case header` 和在 case body 前补 `)` 的 suggestion。该覆盖复用 focused statement parser diagnostics target，验证 `parse_switch_expression` 不再在普通 switch case header 的 `)` 缺失场景只暴露 expected-token fallback。
- `LSP Missing Switch Body Close Parser Diagnostic` 验证 `switch (choice) { (1) { return 1; }` 暴露 `missing_switch_body_close`、`Missing closing '}' for switch body` 和补 `}` 的 suggestion。该覆盖复用 focused statement parser diagnostics target，验证 `parse_switch_expression` 不再在 switch body 最终 `}` 缺失场景只暴露 expected-token fallback。

`zr_vm_language_server_local_semantic_hover_test` 覆盖：

- `LSP Local Expression Hover Surfaces Assignment Write Reference Fact` 验证 `var seed = 1; seed = 3;` 的 assignment target 局部 hover 显示 `Reference: write`、`Symbol: seed` 和 `Declared at: 1:5`，rich hover 同步暴露 `reference` / `symbol` / `declaration` roles。
- `LSP Local Hover Surfaces Constant Boolean Branch Cause` 验证 `var const flag = false; if (flag) { ... }` 的死分支局部 hover 返回 `Logical value: false` 和 `Reachability: unreachable because a constant branch excludes it`，rich hover 同步暴露 `logicalValue` 与 `reachability` role。
- `LSP Local Hover Surfaces Constant False Loop Body Cause` 验证 `var const keepGoing = false; while (keepGoing) { ... }` 的 loop body 局部 hover 返回 `Logical value: false` 和 `Reachability: unreachable because the condition is false`，rich hover 同步暴露 `logicalValue` 与 `reachability` role。
- `LSP Local Hover Surfaces Ownership Violation Message` 验证 `%loan(resource)` 的局部 hover 显示 `Ownership: violation` 和 `Loaned value cannot escape`，rich hover 的 `ownership` role 同时保留 violation 状态与 loan escape message，而不是把 message 降级到 generic docs section。

`tests/language_server/stdio_smoke.js` 覆盖：

- `textDocument/inlineValue` 保留已有 variable lookup 行为。
- `textDocument/inlineValue` 对 `var x = 20;` 的 local initializer 追加 fact-backed `InlineValueText`，内容包含 `range 20..20`。
- `textDocument/inlineValue` 对 `var flag = true || false;` 的 deterministic logical initializer 追加 fact-backed `InlineValueText`，内容包含 `logical true, short-circuits`。

2026-06-03 的 WSL gcc Debug 验证命令：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_semantic_facts_test zr_vm_type_inference_test zr_vm_language_server_semantic_analyzer_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_semantic_facts_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_type_inference_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test"
```

结果：`zr_vm_semantic_facts_test` 3 tests, 0 failures；`zr_vm_type_inference_test` 通过；`zr_vm_language_server_semantic_analyzer_test` 中新的 reference fact、reachability fact、short-circuit logical fact、ownership fact 和 rich semantic diagnostic 测试通过，既有 unreachable/short-circuit warning 测试仍通过。`Semantic Analyzer Reports Borrowed Return Escape` 已由 `borrow_escape` 诊断覆盖，不再作为既有失败保留。构建仍有既有 warning：`type_inference.c` 中 `ZrCore_String_Create` 的 const qualifier warning、`test_type_inference.c` 中 `test_allocator` unused warning、`semantic_analyzer_symbols.c` 的未覆盖 AST switch warning，以及 `lsp_signature_help.c` 中未使用 helper warning。

2026-06-03 Task 5 额外聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_semantic_facts_test zr_vm_language_server_semantic_analyzer_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_semantic_facts_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test > build/codex-wsl-gcc-debug/zr_semantic_analyzer_task5.out; tail -n 28 build/codex-wsl-gcc-debug/zr_semantic_analyzer_task5.out; grep -E 'Fail -' build/codex-wsl-gcc-debug/zr_semantic_analyzer_task5.out || true"
```

结果：`zr_vm_semantic_facts_test` 3 tests, 0 failures；`zr_vm_language_server_semantic_analyzer_test` 输出 `All Semantic Analyzer Tests Completed`，`grep -E 'Fail -'` 无匹配。所有权相关测试包括 initializer mismatch、owner-to-plain escape、return mismatch、borrowed return escape、function argument mismatch、weak argument upgrade 和 method argument mismatch 均通过。

2026-06-03 Task 6 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_parser_test zr_vm_semantic_facts_test zr_vm_type_inference_test zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_incremental_parser_test zr_vm_language_server_lsp_interface_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_parser_test > build/codex-wsl-gcc-debug/zr_parser_task6.out; ./build/codex-wsl-gcc-debug/bin/zr_vm_semantic_facts_test > build/codex-wsl-gcc-debug/zr_semantic_facts_task6.out; ./build/codex-wsl-gcc-debug/bin/zr_vm_type_inference_test > build/codex-wsl-gcc-debug/zr_type_inference_task6.out; ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test > build/codex-wsl-gcc-debug/zr_semantic_analyzer_task6.out; ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_incremental_parser_test > build/codex-wsl-gcc-debug/zr_incremental_parser_task6.out; ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test > build/codex-wsl-gcc-debug/zr_lsp_interface_task6.out"
```

结果：`zr_vm_parser_test` 66 tests, 0 failures；`zr_vm_semantic_facts_test` 3 tests, 0 failures；`zr_vm_type_inference_test` 103 tests, 0 failures；`zr_vm_language_server_semantic_analyzer_test` 输出 `All Semantic Analyzer Tests Completed`；`zr_vm_language_server_incremental_parser_test` 输出 `All Incremental Parser Tests Completed`；`zr_vm_language_server_lsp_interface_test` 输出 `All LSP Interface Tests Completed`。`LSP Get Parser Diagnostics` 的 RED 阶段先失败于缺少 `missing_expression_after_assignment`，GREEN 阶段通过。

2026-06-03 Task 7 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ > build/codex-wsl-gcc-debug/zr_task7_reconfigure.out 2>&1 && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_lsp_interface_test -j 8 > build/codex-wsl-gcc-debug/zr_lsp_local_query_task7_build9.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test > build/codex-wsl-gcc-debug/zr_lsp_local_query_task7_run8.out 2>&1"
```

这次需要重新 configure，因为新增了 `semantic_analyzer_expression_query.c` 和 `lsp_local_semantic_query.c`。结果：新源文件被纳入构建，`LSP Local Semantic Query Returns Expression Fact` 与 `LSP Local Semantic Query Reports Diagnostic Failure For Incomplete Expression` 通过。

Task 7 四目标回归：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_lsp_interface_test zr_vm_language_server_semantic_analyzer_test zr_vm_semantic_facts_test zr_vm_type_inference_test -j 8 > build/codex-wsl-gcc-debug/zr_task7_final_focus_build2.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test > build/codex-wsl-gcc-debug/zr_task7_final_lsp2.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test > build/codex-wsl-gcc-debug/zr_task7_final_semantic_analyzer2.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_semantic_facts_test > build/codex-wsl-gcc-debug/zr_task7_final_semantic_facts2.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_type_inference_test > build/codex-wsl-gcc-debug/zr_task7_final_type_inference2.out 2>&1"
```

结果：`zr_vm_language_server_lsp_interface_test` 输出 `All LSP Interface Tests Completed`；`zr_vm_language_server_semantic_analyzer_test` 输出 `All Semantic Analyzer Tests Completed`；`zr_vm_semantic_facts_test` 3 tests, 0 failures；`zr_vm_type_inference_test` 103 tests, 0 failures。四个输出文件均无 `Fail -` 标记。

2026-06-03 Task 8 stdio diagnostic smoke:

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 8 > build/codex-wsl-gcc-debug/zr_task8_stdio_build.out 2>&1 && ctest --test-dir build/codex-wsl-gcc-debug -R language_server_stdio_smoke --output-on-failure > build/codex-wsl-gcc-debug/zr_task8_stdio_ctest.out 2>&1"
```

结果：`language_server_stdio_smoke` 通过。`stdio_smoke.js` 现在打开 `var x = ;` 文档并断言 JSON diagnostics 中包含 `missing_expression_after_assignment`、`Missing expression after '='` 和 `Add an expression before ';'`，证明结构化 parser 诊断已经能到达 VSCode 插件消费的 stdio 通道。

2026-06-03 Task 8 聚焦一阶段验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_semantic_facts_test zr_vm_type_inference_test zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_lsp_interface_test zr_vm_language_server_stdio -j 8 > build/codex-wsl-gcc-debug/zr_task8_focus_build.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_semantic_facts_test > build/codex-wsl-gcc-debug/zr_task8_semantic_facts.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_type_inference_test > build/codex-wsl-gcc-debug/zr_task8_type_inference.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test > build/codex-wsl-gcc-debug/zr_task8_semantic_analyzer.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test > build/codex-wsl-gcc-debug/zr_task8_lsp_interface.out 2>&1 && ctest --test-dir build/codex-wsl-gcc-debug -R language_server_stdio_smoke --output-on-failure > build/codex-wsl-gcc-debug/zr_task8_stdio_smoke.out 2>&1"
```

结果：`zr_vm_semantic_facts_test` 3 tests, 0 failures；`zr_vm_type_inference_test` 103 tests, 0 failures；`zr_vm_language_server_semantic_analyzer_test` 输出 `All Semantic Analyzer Tests Completed`；`zr_vm_language_server_lsp_interface_test` 输出 `All LSP Interface Tests Completed`；`language_server_stdio_smoke` 通过。`type_inference` 输出中的 `Error Message`/`Status: FAILED` 文本来自负向诊断用例，最终 Unity 汇总仍为 0 failures。

2026-06-03 validation matrix:

```powershell
.\.codex\skills\zr-vm-dev\scripts\validate-matrix.ps1 -Jobs 12
```

结果摘要：WSL gcc configure/build 通过，WSL gcc `hello_world` smoke 通过；WSL clang configure/build 通过，WSL clang `hello_world` smoke 通过；MSVC configure/build 通过，Windows `hello_world` smoke 通过。WSL gcc 和 WSL clang 的全量 `ctest` 均剩余同一个 `core_runtime` 失败：`zr_vm_execution_member_access_fast_paths_test::test_member_get_cached_property_getter_stack_slot_skips_anchor_restore_stack_lookup_when_stack_unchanged`，错误为 `Expected 0 Was 1`。该失败属于运行时 member-access fast path，未触及本阶段 parser semantic fact、diagnostic builder、LSP local semantic query 或 stdio diagnostic path。当前工作树因此不能声明全仓库绿色；一阶段语义/LSP相关验证已经通过。

2026-06-04 LSP completion semantic fact 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_inlay_semantic_facts_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_lsp_interface_test zr_vm_language_server_semantic_analyzer_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_inlay_semantic_facts_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test >/tmp/zr_lsp_semantic_analyzer_completion_fact_gcc.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_lsp_interface_completion_fact_gcc.out 2>&1"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_inlay_semantic_facts_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_lsp_interface_test zr_vm_language_server_semantic_analyzer_test -j 8 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_inlay_semantic_facts_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_semantic_analyzer_test >/tmp/zr_lsp_semantic_analyzer_completion_fact_clang.out 2>&1 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_lsp_interface_completion_fact_clang.out 2>&1"
```

结果：WSL gcc 与 WSL clang 的 `zr_vm_language_server_inlay_semantic_facts_test`、`zr_vm_language_server_local_semantic_query_test`、`zr_vm_language_server_local_semantic_hover_test`、`zr_vm_language_server_semantic_analyzer_test` 和 `zr_vm_language_server_lsp_interface_test` 均通过。`zr_vm_language_server_inlay_semantic_facts_test` 覆盖 completion detail 的 numeric/logical initializer facts；local query、hover 和 interface focused targets 确认新增 completion fact consumer 没有破坏既有 LSP 局部查询、hover/rich hover 和补全行为。

2026-06-04 Windows MSVC compatibility smoke：

```powershell
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake -S . -B build\codex-msvc-lsp-smoke -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF; cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_shared zr_vm_language_server_stdio --parallel 8; cmake -S . -B build\codex-msvc-cli-debug -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF; cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8; .\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

结果：MSVC 使用 `E:\Visual Studio\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe`；`build\codex-msvc-lsp-smoke` 重新配置并构建 `zr_vm_language_server_shared`、`zr_vm_language_server_stdio` 通过，构建日志显示 `lsp_completion_semantic_facts.c` 已纳入编译；`build\codex-msvc-cli-debug` 构建 `zr_vm_cli_executable` 通过，`hello_world.zrp` 输出 `hello world`。

2026-06-04 LSP completion ownership semantic fact RED/GREEN 验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_inlay_semantic_facts_test -j 1 && timeout 60s ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_inlay_semantic_facts_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_inlay_semantic_facts_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_lsp_interface_test -j 1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_inlay_semantic_facts_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test >/tmp/zr_completion_ownership_semantic_analyzer_gcc.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_completion_ownership_lsp_interface_gcc.out 2>&1"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_inlay_semantic_facts_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_lsp_interface_test -j 1 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_inlay_semantic_facts_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_semantic_analyzer_test >/tmp/zr_completion_ownership_semantic_analyzer_clang.out 2>&1 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_completion_ownership_lsp_interface_clang.out 2>&1"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-wsl-gcc-debug -R '^(language_server)$' --output-on-failure"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_shared zr_vm_language_server_stdio --parallel 8; cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8; .\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-wsl-gcc-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp; status=$?; printf "\nEXIT:%s\n" "$status"'
```

结果：RED 阶段新增 `LSP Completion Detail Uses Initializer Ownership Fact` 后，WSL gcc focused test 失败，`plainFromUnique` 的 completion detail 仍为 `private Resource`，缺少 `ownership violation` 和 `Owned value cannot flow into a plain GC value implicitly`。GREEN 后，WSL gcc 与 WSL clang 的 `zr_vm_language_server_inlay_semantic_facts_test`、`zr_vm_language_server_local_semantic_query_test`、`zr_vm_language_server_local_semantic_hover_test`、`zr_vm_language_server_semantic_analyzer_test` 和 `zr_vm_language_server_lsp_interface_test` 均通过；WSL gcc `language_server` CTest 通过。MSVC 重新构建 `zr_vm_language_server_shared`、`zr_vm_language_server_stdio` 和 `zr_vm_cli_executable` 通过，且构建日志显示 `lsp_completion_semantic_facts.c` 已编译；但当前 dirty checkout 的 Windows `hello_world.zrp` CLI smoke 输出 `null`，同一 fixture 在 WSL gcc CLI 也输出 `null` 并以 0 退出。该 smoke 不作为本轮 LSP ownership completion slice 的绿色信号，需继续按 active core/value-type return-path baseline 协调。

2026-06-04 LSP signature-help argument semantic fact RED/GREEN 验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_inlay_semantic_facts_test -j 1 && timeout 60s ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_inlay_semantic_facts_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-gcc-debug && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_inlay_semantic_facts_test -j 1 && timeout 60s ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_inlay_semantic_facts_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_inlay_semantic_facts_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_lsp_interface_test -j 1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_inlay_semantic_facts_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test >/tmp/zr_signature_ownership_semantic_analyzer_gcc.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_signature_ownership_lsp_interface_gcc.out 2>&1"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_inlay_semantic_facts_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_lsp_interface_test -j 1 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_inlay_semantic_facts_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_semantic_analyzer_test >/tmp/zr_signature_ownership_semantic_analyzer_clang.out 2>&1 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_signature_ownership_lsp_interface_clang.out 2>&1"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_semantic_analyzer_test >/tmp/zr_signature_ownership_semantic_analyzer_clang.out 2>&1 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_signature_ownership_lsp_interface_clang.out 2>&1"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-wsl-gcc-debug -R '^(language_server)$' --output-on-failure"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_shared zr_vm_language_server_stdio --parallel 8; cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8; .\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-wsl-gcc-debug/bin/zr_vm_cli ./tests/fixtures/projects/hello_world/hello_world.zrp; status=$?; printf "\nEXIT:%s\n" "$status"'
```

结果：RED 阶段新增 `LSP Signature Help Parameter Docs Use Argument Ownership Fact` 后，WSL gcc focused test 失败于 `Signature help request failed`，说明 `%weak Resource` 实参传入 `%borrowed Resource` 时 overload compatibility 拒绝了调用并让 LSP signature help 消失。GREEN 后，`lsp_signature_semantic_facts.c` 负责 parameter documentation fact formatting，`lsp_signature_help.c` 对唯一同名同 arity 声明做展示回退；WSL gcc 的 focused test 通过，包含 numeric/logical argument docs 和 ownership violation docs。WSL gcc 的五个 focused LSP targets 通过，WSL gcc `language_server` CTest 通过。WSL clang 完成构建并通过 inlay/query/hover；第一次长命令在继续执行 semantic analyzer/interface 时超时，随后单独重跑 `zr_vm_language_server_semantic_analyzer_test` 和 `zr_vm_language_server_lsp_interface_test` 通过。MSVC 重新构建 `zr_vm_language_server_shared`、`zr_vm_language_server_stdio` 和 CLI 通过；第一次 MSVC 构建因 CMake glob 重新生成同一 project pass 后未编译新源而 link failed，重跑后 project 已包含 `lsp_signature_semantic_facts.c` 并链接通过。当前 dirty checkout 的 Windows 和 WSL gcc `hello_world.zrp` CLI smoke 都输出 `null` 并以 0 退出，因此该输出按 active core/value-type return-path baseline 记录，不作为本轮 LSP signature-help slice 的绿色信号。

2026-06-04 LSP stdio inline-value semantic fact RED/GREEN 验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio -j 8 && ctest --test-dir build/codex-wsl-gcc-debug -R language_server_stdio_smoke --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio -j 8 && ctest --test-dir build/codex-wsl-gcc-debug -R language_server_stdio_smoke --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio -j 8 && ctest --test-dir build/codex-wsl-clang-debug -R language_server_stdio_smoke --output-on-failure"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake -S . -B build\codex-msvc-lsp-smoke -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF; cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_shared zr_vm_language_server_stdio --parallel 8; cmake -S . -B build\codex-msvc-cli-debug -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF; cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8; .\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

结果：第一条 RED 在新增断言后失败，错误为 `textDocument/inlineValue must expose semantic numeric facts for local initializers`，证明原 stdio inline value 仍只是 scanner/variable lookup。GREEN 后，WSL gcc 的 `build/codex-wsl-gcc-debug` 和 WSL clang 的 `build/codex-wsl-clang-debug` 均重建 `zr_vm_language_server_stdio`，`language_server_stdio_smoke` 通过，并覆盖 `range 20..20` 与 `logical true, short-circuits` 两个 fact-backed inline text。MSVC 使用 `E:\Visual Studio\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe`；`build\codex-msvc-lsp-smoke` 构建 `zr_vm_language_server_shared`、`zr_vm_language_server_stdio` 通过，`build\codex-msvc-cli-debug` 构建 CLI 通过，`hello_world.zrp` 输出 `hello world`。clang/MSVC 构建日志仍有既有 unrelated warnings；全仓库 runtime baseline 仍不能声明绿色。

2026-06-04 LSP completion resolve protocol coverage and Debug grouped-expression diagnostic 验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && node tests/language_server/stdio_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio build/codex-wsl-clang-debug/bin/zr_vm_cli"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_debug_expression_diagnostics_test -j 2 && ./build/codex-wsl-clang-debug/bin/zr_vm_debug_expression_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_debug_expression_diagnostics_test -j 1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_debug_expression_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 4 && node tests/language_server/stdio_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio build/codex-wsl-clang-debug/bin/zr_vm_cli"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-msvc-debug --config Debug --target zr_vm_debug_expression_diagnostics_test --parallel 4; .\build\codex-msvc-debug\bin\Debug\zr_vm_debug_expression_diagnostics_test.exe
```

结果：stdio smoke 通过，新增协议断言覆盖 `completionItem/resolve` 保留 generic completion 的 `Resolved Type: Derived<Item, 4>`，并保留 local `x` 的 `Semantic facts: range 20..20`、local `flag` 的 `logical true` / `short-circuits` 以及 `labelDetails`。Debug grouped-expression RED 阶段曾在 `(true || false` 上只返回 `missing ')' in debug evaluate expression`；GREEN 后缺少 grouped expression 右括号会返回 `Missing closing ')' in grouped expression`，并包含 `Cause:`、`conditional breakpoint` 和 `Suggestion:`。测试已改用 exported `ZrDebug_Evaluate`，因此 shared DLL 的 MSVC 构建不依赖未导出的内部 `zr_debug_evaluate_expression` symbol。2026-06-04 12:26 +08:00 重新验证后，标准 WSL gcc、WSL clang 与 Windows MSVC CMake target rebuild 均能到达并运行 `zr_vm_debug_expression_diagnostics_test`，各自报告 11 tests, 0 failures；WSL clang 还重建 `zr_vm_language_server_stdio`、`zr_vm_cli_executable` 并再次通过 stdio smoke。MSVC 构建使用 `E:\Visual Studio\VC\Tools\MSVC\14.44.35207\bin\Hostx64\x64\cl.exe`。当前仍不能声明全仓库绿色，因为 active core/value-type runtime work 仍在 dirty checkout 中推进，且 `hello_world.zrp` 在相关 baseline 下可能输出 `null`。

2026-06-04 Debug unterminated string diagnostic RED/GREEN 验证：

```powershell
wsl cmake --build build/codex-wsl-gcc-debug --target zr_vm_debug_expression_diagnostics_test -j 8
wsl ./build/codex-wsl-gcc-debug/bin/zr_vm_debug_expression_diagnostics_test
wsl cmake --build build/codex-wsl-clang-debug --target zr_vm_debug_expression_diagnostics_test -j 8
wsl ./build/codex-wsl-clang-debug/bin/zr_vm_debug_expression_diagnostics_test
cmake --build build\codex-msvc-debug --config Debug --target zr_vm_debug_expression_diagnostics_test --parallel 8
.\build\codex-msvc-debug\bin\Debug\zr_vm_debug_expression_diagnostics_test.exe
```

RED：新增 `"open` 的 safe-evaluate smoke 后，WSL gcc 失败为 `expected expression in debug evaluate`，证明 `zr_debug_eval_parse_string` 已经识别到 bad string literal，但 `zr_debug_eval_parse_primary` 随后覆盖了具体错误。GREEN：unterminated string literal 现在返回 `Unterminated string literal in debug evaluate`，并包含 `Cause:`、`debug evaluate`、`Suggestion:` 和 closing quote 建议；primary-expression fallback 只在还没有更具体错误时写入 generic expected-expression。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_debug_expression_diagnostics_test` 均通过 12 tests, 0 failures。MSVC 仍输出当前 dirty checkout 中已有 core/runtime warning；本 slice 没有触碰 parser/type inference、LSP 或 core runtime。

2026-06-04 Debug unsupported string escape diagnostic RED/GREEN 验证：

```powershell
wsl cmake --build build/codex-wsl-gcc-debug --target zr_vm_debug_expression_diagnostics_test -j 8
wsl ./build/codex-wsl-gcc-debug/bin/zr_vm_debug_expression_diagnostics_test
wsl cmake --build build/codex-wsl-clang-debug --target zr_vm_debug_expression_diagnostics_test -j 8
wsl ./build/codex-wsl-clang-debug/bin/zr_vm_debug_expression_diagnostics_test
cmake --build build\codex-msvc-debug --config Debug --target zr_vm_debug_expression_diagnostics_test --parallel 8
.\build\codex-msvc-debug\bin\Debug\zr_vm_debug_expression_diagnostics_test.exe
```

RED：新增 `"bad\q"` 的 safe-evaluate smoke 后，WSL gcc 失败为旧的 `unsupported string escape in debug evaluate` 短文本，没有具体 escape、原因或建议。GREEN：unsupported escape 现在返回 `Unsupported string escape '\q' in debug evaluate`，并包含 `Cause:`、`\q`、`Suggestion:` 和 supported escape 提示；message 缩短后 GCC 不再报告 `snprintf` truncation warning。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_debug_expression_diagnostics_test` 均通过 13 tests, 0 failures。该 slice 没有触碰 parser/type inference、LSP 或 core runtime。

2026-06-04 LSP ownership rich-hover message RED/GREEN 验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_local_semantic_hover_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_lsp_interface_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test && ctest --test-dir build/codex-wsl-gcc-debug -R '^(language_server)$' --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_lsp_interface_test -j 8 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_shared zr_vm_language_server_stdio --parallel 8; cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8; .\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

RED：`LSP Local Hover Surfaces Ownership Violation Message` 先失败；plain hover 已显示 `Ownership: violation` 和 `Loaned value cannot escape its owner`，但 rich hover 的 `ownership` role 只有 `violation`，诊断 message 落入 generic docs。GREEN：`lsp_local_semantic_query.c` 把 ownership 状态和 message 格式化在同一 `Ownership:` 行。WSL gcc 的 local semantic query、LSP interface 和 `language_server` CTest 通过；WSL clang 的 local semantic hover/query/interface 三个目标通过。MSVC 重新构建 `zr_vm_language_server_shared`、`zr_vm_language_server_stdio` 和 `zr_vm_cli_executable` 通过，`hello_world.zrp` 输出 `hello world`。clang/MSVC 仍有既有 core/runtime warning；本轮没有运行全仓库 ctest，不能声明全仓库绿色。

2026-06-04 LSP exhaustive switch reachability query RED/GREEN 验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_reachability_semantic_query_test -j 1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_reachability_semantic_query_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_reachability_semantic_query_test zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_lsp_interface_test -j 1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_reachability_semantic_query_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_reachability_semantic_query_test zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_lsp_interface_test -j 1 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_reachability_semantic_query_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_semantic_analyzer_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-wsl-gcc-debug -R '^(language_server)$' --output-on-failure"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_shared zr_vm_language_server_stdio --parallel 8; cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8; .\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

RED：新增 `LSP Local Query Returns Exhaustive Switch Reachability Fact` 后先失败，`deadAfterSwitch` 的 local query 返回 fact 状态但 `reachabilityFact` 为 `NULL`；第一次修复只让 switch 被识别为 terminating statement，但 cause 仍是 `UNKNOWN`。GREEN：`semantic_analyzer_reachability.c` 现在要求 switch 有 default 且每个 case/default block 都必然退出，并把 `ZR_AST_SWITCH_EXPRESSION` 映射到 `ZR_SEMANTIC_REACHABILITY_AFTER_EXHAUSTIVE_BRANCH`；WSL gcc/clang 的 reachability query、semantic analyzer、local semantic query、local hover 和 LSP interface focused targets 通过，WSL gcc `language_server` CTest 通过。MSVC 重新构建 `zr_vm_language_server_shared`、`zr_vm_language_server_stdio` 和 CLI 通过，`hello_world.zrp` 输出 `hello world`。gcc 首次并行 RED 验证曾遇到 stale/corrupt shared-library artifact 导致 `libzr_vm_core.so: file not recognized`，重查文件为有效 ELF 后以 `-j 1` 重跑得到行为 RED；clang/MSVC 仍有既有 core/runtime warnings，本轮没有运行全仓库 ctest。

2026-06-04 LSP constant-true loop exit reachability query RED/GREEN 验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_reachability_semantic_query_test -j 1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_reachability_semantic_query_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_reachability_semantic_query_test zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_lsp_interface_test -j 1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_reachability_semantic_query_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_reachability_semantic_query_test zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_lsp_interface_test -j 1 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_reachability_semantic_query_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_semantic_analyzer_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-wsl-gcc-debug -R '^(language_server)$' --output-on-failure"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_shared zr_vm_language_server_stdio --parallel 8; cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8; .\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

RED：新增 `LSP Local Query Returns Constant True Loop Exit Reachability Fact` 后先失败，`deadAfterLoop` 的 local query 返回 fact 状态但 `reachabilityFact` 和 `logicalFact` 均为空；第一次修复只记录了 `ZR_SEMANTIC_REACHABILITY_AFTER_NON_FALLTHROUGH_LOOP`，local query 还没有从 loop cause node 回查 condition logical fact。随后把负向用例加强为 `break; return;`，确认 loop-local break 后的不可达 return 不能证明 loop 后语句不可达。GREEN：`semantic_analyzer_reachability.c` 现在将 loop-local break/continue 和外层 return/throw 分开跟踪，只有所有可执行 body path 都离开外层控制流时，常量 true loop 才会让后续语句写入 `ZR_SEMANTIC_REACHABILITY_AFTER_NON_FALLTHROUGH_LOOP`；`lsp_local_semantic_query.c` 会从 while/for cause node 回查条件 logical fact。WSL gcc/clang 的 reachability query、semantic analyzer、local semantic query、local hover 和 LSP interface focused targets 通过，WSL gcc `language_server` CTest 通过。MSVC 重新构建 `zr_vm_language_server_shared`、`zr_vm_language_server_stdio` 和 CLI 通过，`hello_world.zrp` 输出 `hello world`。验证期间共享 WSL build tree 曾被并发 relink 污染，出现 `libzr_vm_core.so: file not recognized`，等待构建进程清空并重跑后通过；clang/MSVC 仍有既有 core/runtime warnings，本轮没有运行全仓库 ctest。

2026-06-03 REPL/parser missing-right-operand 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build .codex/debug-expression-diagnostics-build --target zr_vm_parser_test zr_vm_cli_executable zr_vm_cli_repl_e2e_test zr_vm_language_server_lsp_interface_test zr_vm_debug_expression_diagnostics_test -j 12"
wsl bash -lc "cd /mnt/e/Git/zr_vm && .codex/debug-expression-diagnostics-build/bin/zr_vm_parser_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && .codex/debug-expression-diagnostics-build/bin/zr_vm_cli_repl_e2e_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_lsp_interface_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && .codex/debug-expression-diagnostics-build/bin/zr_vm_semantic_facts_test && .codex/debug-expression-diagnostics-build/bin/zr_vm_type_inference_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir .codex/debug-expression-diagnostics-build -R '^(cli_repl_e2e|debug_expression_diagnostics)$' --output-on-failure"
```

结果：`zr_vm_parser_test` 67 tests, 0 failures；`zr_vm_cli_repl_e2e_test` 通过，覆盖 `1 + 2` 输出 `3` 和 `1 +` 只显示具体缺右操作数诊断、不泄露内部 REPL 包装；`zr_vm_language_server_lsp_interface_test` 输出 `All LSP Interface Tests Completed`，并覆盖 `missing_right_operand`；`zr_vm_semantic_facts_test` 3 tests, 0 failures；`zr_vm_type_inference_test` 通过；`cli_repl_e2e` 与 `debug_expression_diagnostics` 的 ctest 聚焦回归通过。

2026-06-03 ownership diagnostics 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B .codex/debug-expression-diagnostics-build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_STATIC_LIB=ON -DBUILD_SHARED_LIB=OFF && cmake --build .codex/debug-expression-diagnostics-build --target zr_vm_language_server_ownership_diagnostics_test -j 12 && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_ownership_diagnostics_test"
```

结果：`zr_vm_language_server_ownership_diagnostics_test` 通过，输出 `All Ownership Diagnostics Tests Completed`，覆盖 `loan_escape` 诊断和 ownership fact。

2026-06-03 expression fact emission 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build .codex/debug-expression-diagnostics-build --target zr_vm_expression_fact_emission_test zr_vm_type_inference_test zr_vm_semantic_facts_test -j 12 && .codex/debug-expression-diagnostics-build/bin/zr_vm_expression_fact_emission_test && .codex/debug-expression-diagnostics-build/bin/zr_vm_semantic_facts_test && .codex/debug-expression-diagnostics-build/bin/zr_vm_type_inference_test >/tmp/zr_type_after_expression_fact.out 2>&1; status=$?; tail -12 /tmp/zr_type_after_expression_fact.out; exit $status"
```

结果：`zr_vm_expression_fact_emission_test` 1 test, 0 failures；`zr_vm_semantic_facts_test` 3 tests, 0 failures；`zr_vm_type_inference_test` 103 tests, 0 failures。

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build .codex/debug-expression-diagnostics-build --target zr_vm_language_server_lsp_interface_test zr_vm_language_server_semantic_analyzer_test zr_vm_parser_test -j 12 && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_lsp_after_expression_fact.out 2>&1; status=$?; grep -E 'All LSP Interface Tests Completed|FAIL|Fail|Expression Fact|Hover' /tmp/zr_lsp_after_expression_fact.out | tail -20; exit $status"
```

结果：`zr_vm_language_server_lsp_interface_test` 输出 `All LSP Interface Tests Completed`，并保留 `LSP Local Semantic Query Returns Expression Fact` 覆盖。

2026-06-03 numeric range and overflow fact 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build .codex/debug-expression-diagnostics-build --target zr_vm_expression_fact_emission_test zr_vm_semantic_facts_test zr_vm_type_inference_test zr_vm_language_server_lsp_interface_test -j 12 && .codex/debug-expression-diagnostics-build/bin/zr_vm_expression_fact_emission_test && .codex/debug-expression-diagnostics-build/bin/zr_vm_semantic_facts_test && .codex/debug-expression-diagnostics-build/bin/zr_vm_type_inference_test >/tmp/zr_type_numeric_fact2.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_lsp_numeric_fact2.out 2>&1; status=$?; tail -12 /tmp/zr_type_numeric_fact2.out; grep -E 'All LSP Interface Tests Completed|FAIL|Fail|Expression Fact|Hover' /tmp/zr_lsp_numeric_fact2.out | tail -20; exit $status"
```

结果：`zr_vm_expression_fact_emission_test` 6 tests, 0 failures；`zr_vm_semantic_facts_test` 3 tests, 0 failures；`zr_vm_type_inference_test` 103 tests, 0 failures；`zr_vm_language_server_lsp_interface_test` 输出 `All LSP Interface Tests Completed`。

2026-06-03 LSP local numeric query 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build .codex/debug-expression-diagnostics-build --target zr_vm_parser_test zr_vm_expression_fact_emission_test zr_vm_semantic_facts_test zr_vm_type_inference_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_lsp_interface_test -j 12 && .codex/debug-expression-diagnostics-build/bin/zr_vm_parser_test >/tmp/zr_parser_local_numeric.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_expression_fact_emission_test >/tmp/zr_expr_local_numeric.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_semantic_facts_test >/tmp/zr_semantic_facts_local_numeric.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_type_inference_test >/tmp/zr_type_local_numeric.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_local_semantic_query_test >/tmp/zr_lsp_local_numeric.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_lsp_interface_local_numeric.out 2>&1; status=$?; echo '--- parser ---'; tail -8 /tmp/zr_parser_local_numeric.out; echo '--- expression ---'; tail -8 /tmp/zr_expr_local_numeric.out; echo '--- semantic facts ---'; tail -8 /tmp/zr_semantic_facts_local_numeric.out; echo '--- type inference ---'; tail -8 /tmp/zr_type_local_numeric.out; echo '--- lsp local ---'; tail -12 /tmp/zr_lsp_local_numeric.out; echo '--- lsp interface ---'; grep -E 'All LSP Interface Tests Completed|FAIL|Fail|Local Semantic Query|Expression Fact|Hover' /tmp/zr_lsp_interface_local_numeric.out | tail -24; exit $status"
```

结果：`zr_vm_parser_test` 67 tests, 0 failures；`zr_vm_expression_fact_emission_test` 6 tests, 0 failures；`zr_vm_semantic_facts_test` 3 tests, 0 failures；`zr_vm_type_inference_test` 103 tests, 0 failures；`zr_vm_language_server_local_semantic_query_test` 2 tests, 0 failures；`zr_vm_language_server_lsp_interface_test` 输出 `All LSP Interface Tests Completed`。

2026-06-03 LSP local logical short-circuit query 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build .codex/debug-expression-diagnostics-build --target zr_vm_expression_fact_emission_test zr_vm_semantic_facts_test zr_vm_language_server_local_semantic_query_test -j 4 && .codex/debug-expression-diagnostics-build/bin/zr_vm_expression_fact_emission_test && .codex/debug-expression-diagnostics-build/bin/zr_vm_semantic_facts_test && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_local_semantic_query_test"
```

结果：`zr_vm_expression_fact_emission_test` 8 tests, 0 failures；`zr_vm_semantic_facts_test` 4 tests, 0 failures；`zr_vm_language_server_local_semantic_query_test` 3 tests, 0 failures。

2026-06-04 LSP local unary logical-not query RED/GREEN 验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_expression_fact_emission_test zr_vm_language_server_local_semantic_query_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_expression_fact_emission_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_expression_fact_emission_test zr_vm_language_server_local_semantic_query_test -j 8 && ./build/codex-wsl-clang-debug/bin/zr_vm_expression_fact_emission_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test"
cmake --build build\codex-msvc-debug --config Debug --target zr_vm_expression_fact_emission_test zr_vm_language_server_local_semantic_query_test --parallel 8; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; .\build\codex-msvc-debug\bin\Debug\zr_vm_expression_fact_emission_test.exe; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; .\build\codex-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe
```

RED：新增 `test_unary_logical_not_records_exact_logical_fact` 后，parser fact emission 先失败，因为 `!true` 没有 parser-owned logical fact。新增 LSP local query 用例后，`ExpressionAt` 在 `!` 位置先返回 `true` 字面量 expression fact，`logicalFact` 为空。GREEN：type inference 现在为 unary logical-not 写入 exact `ALWAYS_TRUE/ALWAYS_FALSE` logical fact，parser unary expression range 覆盖 operator token 到 operand，LSP expression query 在 operator 位置返回 unary node，local query materialize 后可通过 expression node 找到 logical fact。WSL gcc、WSL clang 和 Windows MSVC focused target pair 均通过；当前结果为 `zr_vm_expression_fact_emission_test` 27 tests, 0 failures，`zr_vm_language_server_local_semantic_query_test` 全部通过。

2026-06-03 LSP local logical short-circuit query 扩展聚焦回归：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build .codex/debug-expression-diagnostics-build --target zr_vm_parser_test zr_vm_expression_fact_emission_test zr_vm_semantic_facts_test zr_vm_type_inference_test zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_lsp_interface_test -j 4 && .codex/debug-expression-diagnostics-build/bin/zr_vm_parser_test >/tmp/zr_parser_phase1.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_expression_fact_emission_test >/tmp/zr_expr_facts_phase1.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_semantic_facts_test >/tmp/zr_semantic_facts_phase1.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_type_inference_test >/tmp/zr_type_inference_phase1.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_semantic_analyzer_test >/tmp/zr_lsp_semantic_analyzer_phase1.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_local_semantic_query_test >/tmp/zr_lsp_local_query_phase1.out 2>&1 && .codex/debug-expression-diagnostics-build/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_lsp_interface_phase1.out 2>&1"
```

结果：`zr_vm_parser_test` 67 tests, 0 failures；`zr_vm_expression_fact_emission_test` 8 tests, 0 failures；`zr_vm_semantic_facts_test` 4 tests, 0 failures；`zr_vm_type_inference_test` 103 tests, 0 failures；`zr_vm_language_server_semantic_analyzer_test` 输出 `All Semantic Analyzer Tests Completed`；`zr_vm_language_server_local_semantic_query_test` 3 tests, 0 failures；`zr_vm_language_server_lsp_interface_test` 输出 `All LSP Interface Tests Completed`。构建仍有既有 warning：`tests/parser/test_type_inference.c` 中 `test_allocator` defined but not used。

2026-06-04 LSP hover reference fact payload RED/GREEN 验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_lsp_interface_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_lsp_interface_test -j 8 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test"
```

RED：`LSP Hover Includes Local Reference Fact Payload` 先失败，实际 hover 只包含 `**variable**: result`、`Resolved Type: int` 和 `Access: private`，没有 reference fact payload。GREEN：WSL gcc 和 WSL clang 的 `zr_vm_language_server_lsp_interface_test` 均输出 `All LSP Interface Tests Completed`，新用例通过并验证 symbol hover 追加 `Reference: read` 与 `Declared at: 2:9`。

2026-06-04 LSP assignment write reference hover 覆盖验证：

```powershell
wsl cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_local_semantic_query_test -j 8
wsl ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test
wsl ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test
wsl cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_local_semantic_query_test -j 8
wsl ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test
wsl ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test
& "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1" cmake --build build\codex-msvc-debug --config Debug --target zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_local_semantic_query_test --parallel 8
.\build\codex-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_hover_test.exe
.\build\codex-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe
```

结果：新增 `LSP Local Expression Hover Surfaces Assignment Write Reference Fact` 后，WSL gcc、WSL clang 和 Windows MSVC 的 `zr_vm_language_server_local_semantic_hover_test` 都通过，并验证 `var seed = 1; seed = 3;` 的 assignment target hover 显示 `Reference: write`、`Symbol: seed` 和 `Declared at: 1:5`，rich hover 同步暴露 `reference`、`symbol` 和 `declaration` roles。`zr_vm_language_server_local_semantic_query_test` 同时在三套构建中继续通过既有 `LSP Local Reference Query Returns Write Fact`。该轮发现 LSP semantic analyzer 的 identifier assignment-target write path 已在当前工作区存在，因此本 slice 只补足 hover/rich-hover 端到端覆盖和文档；property/index write references 仍不在本轮声明范围内。

2026-06-04 LSP constant-boolean branch reachability hover RED/GREEN 验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_local_semantic_hover_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_lsp_interface_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test >/tmp/zr_const_branch_semantic_gcc.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test >/tmp/zr_const_branch_query_gcc.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test >/tmp/zr_const_branch_hover_gcc.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_const_branch_interface_gcc.out 2>&1"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_lsp_interface_test -j 8 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_semantic_analyzer_test >/tmp/zr_const_branch_semantic_clang.out 2>&1 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test >/tmp/zr_const_branch_query_clang.out 2>&1 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test >/tmp/zr_const_branch_hover_clang.out 2>&1 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_const_branch_interface_clang.out 2>&1"
```

RED：`LSP Local Hover Surfaces Constant Boolean Branch Cause` 先失败，`ExpressionAt` 在 `deadThen` 位置返回 `UNKNOWN`，`reachabilityFact` 和 `logicalFact` 均为空。GREEN：新增 `semantic_analyzer_constant_condition.c` 后，WSL gcc 与 WSL clang 的 `zr_vm_language_server_semantic_analyzer_test`、`zr_vm_language_server_local_semantic_query_test`、`zr_vm_language_server_local_semantic_hover_test`、`zr_vm_language_server_lsp_interface_test` 均通过。新用例验证 `var const flag = false; if (flag) { ... }` 的死分支 hover 展示 `Logical value: false` 与 `Reachability: unreachable because a constant branch excludes it`，rich hover 同步输出 `logicalValue` 和 `reachability` roles。

2026-06-04 LSP constant-true branch exit hover RED/GREEN 验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_local_semantic_hover_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_lsp_interface_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_semantic_analyzer_test >/tmp/zr_const_true_branch_semantic_gcc.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test >/tmp/zr_const_true_branch_query_gcc.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test >/tmp/zr_const_true_branch_hover_gcc.out 2>&1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_const_true_branch_lsp_gcc.out 2>&1"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_semantic_analyzer_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_lsp_interface_test -j 8 >/tmp/zr_const_true_branch_clang_build.out 2>&1 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_semantic_analyzer_test >/tmp/zr_const_true_branch_semantic_clang.out 2>&1 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test >/tmp/zr_const_true_branch_query_clang.out 2>&1 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test >/tmp/zr_const_true_branch_hover_clang.out 2>&1 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test >/tmp/zr_const_true_branch_lsp_clang.out 2>&1"
```

RED：`LSP Local Hover Surfaces Constant True Branch Exit Cause` 先失败，`var const flag = true; if (flag) { return 1; } var afterBranch = 2;` 中 `afterBranch` 的 `ExpressionAt` 返回 fact 状态但没有 `reachabilityFact` 或 `logicalFact`。GREEN：`semantic_analyzer_reachability.c` 现在拥有 `StatementDefinitelyExits`，会用已有常量条件推断只检查实际执行分支；block 扫描记录 `ZR_SEMANTIC_REACHABILITY_AFTER_EXHAUSTIVE_BRANCH`，local query 从 `if` 原因节点回查 `if.condition` logical fact。WSL gcc 与 WSL clang 的 `zr_vm_language_server_semantic_analyzer_test`、`zr_vm_language_server_local_semantic_query_test`、`zr_vm_language_server_local_semantic_hover_test`、`zr_vm_language_server_lsp_interface_test` 均通过，新 hover 用例显示 `Logical value: true` 和 `Reachability: unreachable after exhaustive branch`。

2026-06-04 LSP call/member hover payload RED/GREEN 验证：

```powershell
wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_call_member_semantic_query_test -j 1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_call_member_semantic_query_test'
wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_call_member_semantic_query_test -j 1 && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-wsl-clang-debug/bin/zr_vm_language_server_call_member_semantic_query_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-msvc-debug --target zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_call_member_semantic_query_test --config Debug --parallel 4; .\build\codex-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_hover_test.exe; .\build\codex-msvc-debug\bin\Debug\zr_vm_language_server_call_member_semantic_query_test.exe
```

RED：新增 `LSP Local Hover Surfaces Call/Member Payloads` 后，WSL gcc 先失败，因为 `ExpressionAt` 已经返回 `hasCallInfo=true` 的 `pick(42)` expression fact，但 hover/rich-hover 只显示 type/reference 内容，没有 `Call:` 行或 `call` role。GREEN：`lsp_local_semantic_query.c` 在共享 hover formatter 中追加 expression payload，`lsp_interface.c` 把 `Call` / `Member` markdown label 映射到 `call` / `member` rich-hover role。WSL gcc、WSL clang 和 Windows MSVC 的 `zr_vm_language_server_local_semantic_hover_test` 与 `zr_vm_language_server_call_member_semantic_query_test` 均通过。WSL clang 和 MSVC 仍报告当前 dirty checkout 里的既有 warning，本轮没有运行全仓库 ctest。

2026-06-04 object literal property-value expression fact RED/GREEN 验证：

```powershell
wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_expression_fact_emission_test zr_vm_type_inference_test -j 1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_expression_fact_emission_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_type_inference_test'
wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_expression_fact_emission_test zr_vm_type_inference_test -j 1 && ./build/codex-wsl-clang-debug/bin/zr_vm_expression_fact_emission_test && ./build/codex-wsl-clang-debug/bin/zr_vm_type_inference_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-msvc-debug --target zr_vm_expression_fact_emission_test zr_vm_type_inference_test --config Debug --parallel 4; .\build\codex-msvc-debug\bin\Debug\zr_vm_expression_fact_emission_test.exe; .\build\codex-msvc-debug\bin\Debug\zr_vm_type_inference_test.exe
```

RED：新增 `test_object_literal_value_expression_records_nested_facts` 后，WSL gcc 先失败在 `Expected Non-NULL`，证明 `{a: 1 + 2}` 的 object literal 自身有 expression fact，但 property value 的 binary expression 没有 expression/numeric facts。GREEN：`ZrParser_ObjectLiteralType_Infer` 现在 best-effort 推断 key/value pair 的 value expression，保留 object literal 原有 `object` 返回合同，同时让 nested value materialize `binary` expression fact 和 exact `3..3` numeric promotion fact。WSL gcc 的 build-and-run 通过；WSL clang 和 Windows MSVC 均显示 `zr_vm_expression_fact_emission_test` 20 tests, 0 failures，`zr_vm_type_inference_test` 103 tests, 0 failures。后续 direct GCC summary rerun 遇到当前 dirty build tree 缺失 `libzr_vm_lib_system.so` 的 shared-library loader artifact，没有作为失败接受；本轮仍不声明全仓库绿色。

2026-06-04 array literal element expression fact characterization 验证：

```powershell
wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_expression_fact_emission_test -j 1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_expression_fact_emission_test'
```

结果：新增 `test_array_literal_element_expression_records_nested_facts` 后，WSL gcc 直接通过并显示 `zr_vm_expression_fact_emission_test` 21 tests, 0 failures。该用例证明当前 `ZrParser_ArrayLiteralType_Infer` 已经访问 element expression：`[1 + 2]` 的 array literal 记录 `ZR_SEMANTIC_EXPRESSION_FACT_ARRAY`，nested `1 + 2` 记录 binary expression fact 和 exact `3..3` numeric promotion fact。该 slice 没有 production code 变更，只补足 coverage；WSL clang/MSVC focused validation 跟随最终 parser fact set 记录。

2026-06-04 expression fact remaining-kind characterization 验证：

```powershell
wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_expression_fact_emission_test -j 1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_expression_fact_emission_test'
```

结果：新增 identifier、assignment、lambda 和 ownership builtin focused tests 后，WSL gcc 直接通过并显示 `zr_vm_expression_fact_emission_test` 25 tests, 0 failures。该 characterization 证明当前 centralized `ZrParser_ExpressionType_Infer` 成功出口和 `type_inference_expression_fact_kind` 已覆盖这些 expression kinds：registered identifier、`seed = 3`、`(x:int)->{ return x; }`、`%borrow(owner)` 都写入对应 expression fact；ownership builtin 还写入共享 ownership fact。没有 production code 变更。

2026-06-04 object literal computed-key expression fact 与 lowering RED/GREEN 验证：

```powershell
wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_expression_fact_emission_test -j 1 && ./build/codex-wsl-gcc-debug/bin/zr_vm_expression_fact_emission_test'
wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && ninja -C build/codex-wsl-gcc-debug zr_vm_parser/CMakeFiles/zr_vm_parser_shared.dir/src/zr_vm_parser/compiler/compile_expression_values.c.o && cd build/codex-wsl-gcc-debug && ninja -t commands lib/libzr_vm_parser.so | tail -1 | bash && ./bin/zr_vm_object_literal_key_lowering_test'
wsl -e bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-clang-debug -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIB=ON && cmake --build build/codex-wsl-clang-debug --target zr_vm_expression_fact_emission_test zr_vm_object_literal_key_lowering_test -j 1 && ./build/codex-wsl-clang-debug/bin/zr_vm_expression_fact_emission_test && ./build/codex-wsl-clang-debug/bin/zr_vm_object_literal_key_lowering_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake -S . -B build\codex-msvc-debug -DBUILD_SHARED_LIB=ON; cmake --build build\codex-msvc-debug --target zr_vm_expression_fact_emission_test zr_vm_object_literal_key_lowering_test --config Debug --parallel 4; .\build\codex-msvc-debug\bin\Debug\zr_vm_expression_fact_emission_test.exe; .\build\codex-msvc-debug\bin\Debug\zr_vm_object_literal_key_lowering_test.exe
```

RED：新增 `test_object_literal_computed_key_expression_records_nested_facts` 后，WSL gcc 先失败在 `Expected Non-NULL`，证明 `{[1 + 2]: 4}` 的 computed key expression 没有 materialize expression/numeric facts。新增 isolated lowering test 后，用旧 lowering 条件 `kv->key->type == ZR_AST_IDENTIFIER_LITERAL` 直接重建 parser object/shared library 并运行 `zr_vm_object_literal_key_lowering_test`，失败为 `Expected 1 Was 0`，证明 `{[key]: 1}` 没有产生 `SET_BY_INDEX`。

GREEN：parser 现在在 `SZrKeyValuePair.keyIsComputed` 记录 bracketed key，`ZrParser_ObjectLiteralType_Infer` 只对 computed key 做 best-effort key inference，同时继续访问 property values；compiler/static/compile-time consumers 复用该 marker。WSL gcc 通过直接重建 affected parser object、直接 relink `libzr_vm_parser.so` 与 focused binaries 验证：`zr_vm_expression_fact_emission_test` 26 tests, 0 failures；`zr_vm_object_literal_key_lowering_test` 1 test, 0 failures。WSL clang 正常 CMake target rebuild/run 通过同一两个 target：26 tests/0 failures 和 1 test/0 failures。Windows MSVC 正常 target rebuild/run 通过同一两个 target：26 tests/0 failures 和 1 test/0 failures。WSL clang/MSVC 仍输出当前 dirty checkout 中已有 core/runtime warning；正常 WSL gcc CMake target rebuild 曾被 active value-type runtime/core compile state 拖入无关路径，因此本段 GCC 证据使用 direct object/shared-library relink，不声明全仓库绿色。

2026-06-04 LSP stdio multi-line return inline-value semantic fact RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio -j 8'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_language_server_stdio'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-wsl-gcc-debug -R "^language_server_stdio(_inline_value_semantic)?_smoke$" --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio -j 8'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-wsl-clang-debug -R "^language_server_stdio(_inline_value_semantic)?_smoke$" --output-on-failure'
cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_stdio --parallel 8
node tests\language_server\stdio_inline_value_semantic_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe
node tests\language_server\stdio_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe
```

RED：新增 `return 1 +` / `2;` 的 stdio inline-value smoke 后，WSL gcc 先返回两个单行 inline text：第一行 `range 3..3` 范围只覆盖 `1 +`，第二行又把 continuation `2` 当成独立 expression statement。GREEN：`stdio_inline_value.c` 现在把 simple return/expression statement 的 source offsets 转为 LSP multi-line range/query position；`return 1 +\n 2;` 只产生一个覆盖两行表达式的 fact-backed inline value。Stdio 仍只负责定位 conservative source range 和 operator query offset，数值/逻辑事实继续来自 `ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt`。WSL gcc 和 WSL clang 的 registered stdio smoke 均通过；Windows MSVC `zr_vm_language_server_stdio` build、dedicated inline-value smoke 和 broader stdio smoke 均通过。构建仍输出当前 dirty checkout 中已有 core/runtime/parser warning；本轮不声明全仓库绿色。

2026-06-04 LSP stdio multi-line initializer inline-value semantic fact RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_language_server_stdio'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio -j 8 && ctest --test-dir build/codex-wsl-gcc-debug -R "^language_server_stdio(_inline_value_semantic)?_smoke$" --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio -j 8 && ctest --test-dir build/codex-wsl-clang-debug -R "^language_server_stdio(_inline_value_semantic)?_smoke$" --output-on-failure'
cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_stdio --parallel 8
node tests\language_server\stdio_inline_value_semantic_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe
node tests\language_server\stdio_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe
```

RED：新增 `var sum =\n 1 + 2;` 的 focused stdio smoke 后，WSL gcc 返回 `sum` 的 runtime variable lookup，但 semantic text 出现在 continuation expression range `2:8..2:13`，没有挂回变量名。随后强化 RED，确认不允许 continuation expression 再产生重复 `range 3..3` inline text。GREEN：initializer scanner 现在跨行跳过 `=` 后空白并查找到 statement terminator，用 continuation 行的 operator/query position 询问 shared local semantic query，同时把 `InlineValueText` 的 LSP range 保持在变量名上；上一行以 `=` 结束时，continuation 行不会再作为独立 expression statement 扫描。WSL gcc 与 WSL clang registered stdio smokes 均通过；Windows MSVC stdio target build、dedicated inline-value smoke 和 broader stdio smoke 均通过。该 slice 没有修改 parser/type inference 或 core runtime。

2026-06-04 LSP stdio return-next-line inline-value semantic fact RED/GREEN 验证：

```powershell
node tests\language_server\stdio_inline_value_semantic_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio -j 8 && ctest --test-dir build/codex-wsl-gcc-debug -R "^language_server_stdio(_inline_value_semantic)?_smoke$" --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio -j 8 && ctest --test-dir build/codex-wsl-clang-debug -R "^language_server_stdio(_inline_value_semantic)?_smoke$" --output-on-failure'
cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_stdio --parallel 8
node tests\language_server\stdio_inline_value_semantic_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe
node tests\language_server\stdio_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe
```

RED：新增 `return\n 1 + 2;` focused stdio smoke 后，Windows probe 和 RED run 返回两个 `range 3..3` inline text：一个从 `return` 行末空白跨到表达式末尾，另一个锚在 continuation expression。GREEN：return scanner 现在跨行跳过 return 后的 leading whitespace，直接用实际 expression start/end 构造 LSP range；continuation 检测也识别前一个 significant token 是 `return`，避免下一行再作为独立 expression statement 扫描。WSL gcc 与 WSL clang registered stdio smokes 均通过；Windows MSVC stdio target build、dedicated inline-value smoke 和 broader stdio smoke 均通过。该 slice 没有修改 parser/type inference 或 core runtime。

2026-06-04 LSP stdio unary expression-statement inline-value semantic fact RED/GREEN 验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio -j 8 && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_language_server_stdio"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-wsl-gcc-debug -R '^language_server_stdio(_inline_value_semantic)?_smoke$' --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio -j 8 && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-wsl-clang-debug -R '^language_server_stdio(_inline_value_semantic)?_smoke$' --output-on-failure"
cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_stdio --parallel 8; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; node tests\language_server\stdio_inline_value_semantic_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe
ctest --test-dir build\codex-msvc-lsp-smoke -C Debug -R "^language_server_stdio(_inline_value_semantic)?_smoke$" --output-on-failure
```

RED：新增 `!true;` / `-42;` focused stdio smoke 后，WSL gcc 先失败为 `textDocument/inlineValue must expose logical facts for unary expression statements; values=[]`，证明原 scanner 没有把 unary 起始行当成 expression statement 查询。GREEN：`stdio_inline_value.c` 只把 `!` 和 `-` 加入 line-start expression statement 的保守起始集合，后续 range、query position 和事实展示仍复用 `ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt`。WSL gcc 与 WSL clang registered stdio smokes 均通过；Windows MSVC `zr_vm_language_server_stdio` build 和 dedicated inline-value smoke 通过。`build\codex-msvc-lsp-smoke` 当前没有注册对应 CTest entry，`ctest` 返回 `No tests were found`，因此 MSVC 证据按 direct node smoke 记录。本 slice 没有修改 parser/type inference 或 core runtime。

2026-06-04 LSP stdio call/member expression-statement inline-value semantic payload RED/GREEN 验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio -j 8 && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_language_server_stdio"
wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-wsl-gcc-debug -R '^language_server_stdio(_inline_value_semantic)?_smoke$' --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio -j 8 && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio && ctest --test-dir build/codex-wsl-clang-debug -R '^language_server_stdio(_inline_value_semantic)?_smoke$' --output-on-failure"
cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_stdio --parallel 8; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; node tests\language_server\stdio_inline_value_semantic_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe
```

RED：新增 `pick(42);` / `seed.value;` focused stdio smoke 后，WSL gcc 先失败为 `textDocument/inlineValue must expose call payload facts for call expression statements; values=[]`，证明原 inline-value formatter 只输出 numeric/logical fact，不会展示已有 call/member expression payload。GREEN：`stdio_inline_value.c` 现在把 `SZrSemanticExpressionFact.hasCallInfo/hasMemberInfo` 格式化为 `call pick args=1` 和 `member value`；call statement 查询表达式起点的 call target token，member statement 查询 `.` 后的 member token，并且只把后接 identifier-start 的 `.` 当作 member query。stdio 仍只负责 source range/query token 选择和已存在 payload 的展示，不在 stdio 层推断调用或成员语义。本 slice 没有修改 parser/type inference 或 core runtime。WSL gcc 与 WSL clang 的 direct focused smoke 和 registered stdio CTest filter 均通过；Windows MSVC `zr_vm_language_server_stdio` build、dedicated inline-value smoke 和 broader stdio smoke 均通过。构建仍输出当前 dirty checkout 中已有 core/runtime/parser/LSP warning；本轮不声明全仓库绿色。

2026-06-04 LSP stdio continuation-range expression-statement inline-value RED/GREEN 验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_language_server_stdio"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio -j 8 && ctest --test-dir build/codex-wsl-gcc-debug -R '^language_server_stdio(_inline_value_semantic)?_smoke$' --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio -j 8 && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio && ctest --test-dir build/codex-wsl-clang-debug -R '^language_server_stdio(_inline_value_semantic)?_smoke$' --output-on-failure"
cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_stdio --parallel 8
node tests\language_server\stdio_inline_value_semantic_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe
node tests\language_server\stdio_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe
```

RED：新增 request range 只覆盖 `1 +\n 2;` continuation line 的 focused stdio smoke 后，WSL gcc 返回 `values=[]`，证明 scanner 为避免重复而跳过 continuation line 时没有 owner-line fallback。GREEN：`stdio_inline_value.c` 现在在 request 起点是 continuation line 且 owner line 不在 request range 内时，回溯前面的简单 expression statement，复用完整 expression range 和 shared local semantic query 返回 `range 3..3`。这只扩展 stdio 的 range/query 定位，不新增 parser/type inference 或 core runtime 逻辑。WSL gcc 与 WSL clang direct focused smoke 和 registered stdio CTest filter 均通过；Windows MSVC stdio target build、dedicated inline-value smoke 和 broader stdio smoke 均通过。构建仍可能输出当前 dirty checkout 中已有 core/runtime warning；本轮不声明全仓库绿色。

2026-06-04 REPL short-circuit reachability fact RED/GREEN 验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_cli_executable -j 8 && node tests/cli/repl_type_reachability_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_cli"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_expression_fact_emission_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_expression_fact_emission_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-wsl-gcc-debug --target zr_vm_expression_fact_emission_test zr_vm_cli_executable -j 8 && ctest --test-dir build/codex-wsl-gcc-debug -R '^cli_repl_type_(ownership|call_member|reachability)_smoke$|^cli_repl_e2e$' --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_expression_fact_emission_test zr_vm_cli_executable -j 8 && ./build/codex-wsl-clang-debug/bin/zr_vm_expression_fact_emission_test && ctest --test-dir build/codex-wsl-clang-debug -R '^cli_repl_type_(ownership|call_member|reachability)_smoke$' --output-on-failure"
cmake --build build\codex-msvc-debug --config Debug --target zr_vm_expression_fact_emission_test zr_vm_cli_executable --parallel 8; .\build\codex-msvc-debug\bin\Debug\zr_vm_expression_fact_emission_test.exe; node tests\cli\repl_type_reachability_smoke.js build\codex-msvc-debug\bin\Debug\zr_vm_cli.exe
node tests\cli\repl_type_ownership_smoke.js build\codex-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_call_member_smoke.js build\codex-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_reachability_smoke.js build\codex-msvc-debug\bin\Debug\zr_vm_cli.exe
```

RED：新增 `tests/cli/repl_type_reachability_smoke.js` 后，WSL gcc 直接 smoke 失败为 `:type should print the shared reachability fact for the skipped operand`，输出只有 `Type: bool`、`Logical value: true` 和 `Logical flow: short-circuits right operand`。随后 parser fact RED 也失败在 `test_logical_expression_inference_records_short_circuit_fact` 的 `Expected Non-NULL`，证明 shared reachability fact 本身尚未由 type inference 写入。GREEN：`type_inference_record_logical_short_circuit_fact` 现在在 skipped RHS range 写入 `ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT`，`causeNode` 指向左操作数；`repl_semantic_facts.c` 遍历 `:type` 表达式子树并通过 `ZrParser_SemanticFacts_FindReachabilityAtPosition` 显示不可达原因。WSL gcc parser fact target 27 tests/0 failures，registered `cli_repl_type_*` smokes 和 `cli_repl_e2e` 通过。WSL clang parser fact target 27 tests/0 failures，registered `cli_repl_type_ownership_smoke`、`cli_repl_type_call_member_smoke`、`cli_repl_type_reachability_smoke` 通过；同一 Clang build 的 broader `cli_repl_e2e` 仍在 current dirty checkout 的 `ZrCore_Closure_CloseStackValue` assertion 失败，属于已记录的 core/closure runtime path，不作为本 REPL fact display slice 的绿色信号。Windows MSVC parser fact target 27 tests/0 failures，三个 direct `:type` node smokes 通过；MSVC 仍报告既有 CLI runtime const qualifier warnings。

2026-06-04 parser/LSP/REPL constant comparison logical fact RED/GREEN 验证：

```powershell
wsl cmake --build build/codex-wsl-gcc-debug --target zr_vm_logical_fact_emission_test zr_vm_expression_fact_emission_test zr_vm_language_server_logical_semantic_query_test zr_vm_language_server_local_semantic_query_test zr_vm_cli_executable -j 8
wsl build/codex-wsl-gcc-debug/bin/zr_vm_logical_fact_emission_test
wsl build/codex-wsl-gcc-debug/bin/zr_vm_expression_fact_emission_test
wsl build/codex-wsl-gcc-debug/bin/zr_vm_language_server_logical_semantic_query_test
wsl build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test
wsl node tests/cli/repl_type_logical_comparison_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_cli
wsl node tests/cli/repl_type_reachability_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_cli
wsl cmake --build build/codex-wsl-clang-debug --target zr_vm_logical_fact_emission_test zr_vm_expression_fact_emission_test zr_vm_language_server_logical_semantic_query_test zr_vm_language_server_local_semantic_query_test zr_vm_cli_executable -j 8
wsl build/codex-wsl-clang-debug/bin/zr_vm_logical_fact_emission_test
wsl build/codex-wsl-clang-debug/bin/zr_vm_expression_fact_emission_test
wsl build/codex-wsl-clang-debug/bin/zr_vm_language_server_logical_semantic_query_test
wsl build/codex-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test
wsl node tests/cli/repl_type_logical_comparison_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_cli
wsl node tests/cli/repl_type_reachability_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_cli
& "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1" cmake --build build\codex-msvc-debug --config Debug --target zr_vm_logical_fact_emission_test zr_vm_expression_fact_emission_test zr_vm_language_server_logical_semantic_query_test zr_vm_language_server_local_semantic_query_test zr_vm_cli_executable --parallel 8
.\build\codex-msvc-debug\bin\Debug\zr_vm_logical_fact_emission_test.exe
.\build\codex-msvc-debug\bin\Debug\zr_vm_expression_fact_emission_test.exe
.\build\codex-msvc-debug\bin\Debug\zr_vm_language_server_logical_semantic_query_test.exe
.\build\codex-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe
node tests\cli\repl_type_logical_comparison_smoke.js build\codex-msvc-debug\bin\Debug\zr_vm_cli.exe
node tests\cli\repl_type_reachability_smoke.js build\codex-msvc-debug\bin\Debug\zr_vm_cli.exe
```

RED：新增 `tests/parser/test_logical_fact_emission.c`、`tests/language_server/test_lsp_logical_semantic_query.c` 和 `tests/cli/repl_type_logical_comparison_smoke.js` 后，WSL gcc 显示比较表达式已有 `Type: bool`，但 expression fact `hasConstant=0`、LSP `logicalFact=(nil)`，REPL 只打印 `Type: bool` 而没有 `Logical value: true`。GREEN：`type_inference_node_bool_value` 现在能对常量数值比较求 bool 值，expression fact 保留 bool 常量；`type_inference_record_constant_comparison_logical_fact` 为 comparison AST range 写入 exact `ALWAYS_TRUE/ALWAYS_FALSE` logical fact。WSL gcc、WSL clang 和 Windows MSVC 的 focused parser/LSP/REPL 测试均通过；clang/GCC 首次重链期间各遇到一次 active dirty build 目录的 shared-library transient，重跑同一目标后通过。现有 core/runtime/parser warning 仍是 dirty checkout 基线，本轮不声明全仓库绿色。

2026-06-04 parser/LSP/REPL composed boolean logical fact RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_logical_fact_emission_test zr_vm_language_server_logical_semantic_query_test zr_vm_cli_executable -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_logical_fact_emission_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_logical_semantic_query_test && node tests/cli/repl_type_logical_comparison_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_cli'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_expression_fact_emission_test zr_vm_language_server_local_semantic_query_test -j 8 && ./build/codex-wsl-gcc-debug/bin/zr_vm_expression_fact_emission_test && ./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test'
```

RED：新增 `!(1 < 2)` 和 `(1 < 2) && (3 < 4)` parser/LSP/REPL 覆盖后，parser fact tests 先失败在 `Expected TRUE Was FALSE`，证明 unary/logical expression fact 没有继续携带 bool 常量；LSP `&&` operator query 返回 `exprType=bool` 但 `hasConst=0`、`logical=(nil)`；REPL 对组合比较只输出 `Type: bool`。GREEN：`type_inference_node_bool_value` 现在递归处理 logical-not 和 `&&` / `||`，并保留短路已知值；`type_inference_record_logical_expression_constant_fact` 只为没有短路但结果可证明的 logical expression 写入 exact `ALWAYS_TRUE/ALWAYS_FALSE`，因此既增强 `(1 < 2) && (3 < 4)`，又保持 `true || false` 的 existing short-circuit fact 作为 `FindLogicalByNode` 的结果。WSL gcc、WSL clang 和 Windows MSVC focused parser/LSP/REPL tests 以及相邻 `zr_vm_expression_fact_emission_test`、`zr_vm_language_server_local_semantic_query_test` 均通过；现有 core/runtime/parser/LSP warning 仍属于 dirty checkout 基线，本轮不声明全仓库绿色。

2026-06-05 parser/type-inference constant evaluator modularization 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_logical_fact_emission_test zr_vm_expression_fact_emission_test zr_vm_language_server_logical_semantic_query_test zr_vm_language_server_local_semantic_query_test zr_vm_cli_executable -j 6'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_logical_fact_emission_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_expression_fact_emission_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_logical_semantic_query_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && node tests/cli/repl_type_logical_comparison_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_reachability_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_logical_fact_emission_test zr_vm_expression_fact_emission_test zr_vm_language_server_logical_semantic_query_test zr_vm_language_server_local_semantic_query_test zr_vm_cli_executable -j 6'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_logical_fact_emission_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_expression_fact_emission_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_logical_semantic_query_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test && node tests/cli/repl_type_logical_comparison_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_reachability_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake -S . -B build\codex-semantic-msvc-debug -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=ON -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF
cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_logical_fact_emission_test zr_vm_expression_fact_emission_test zr_vm_language_server_logical_semantic_query_test zr_vm_language_server_local_semantic_query_test zr_vm_cli_executable --parallel 6
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_logical_fact_emission_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_expression_fact_emission_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_logical_semantic_query_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe
node tests\cli\repl_type_logical_comparison_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe
node tests\cli\repl_type_reachability_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe
```

结果：`type_inference_node_*_value` 常量求值 helper 已抽到 `type_inference_constant_eval.c/.h`，`type_inference_semantic_facts.c` 降到 867 行。为避免与另一个活跃 WSL dirty build 目录竞争，本轮使用 isolated `build/codex-semantic-wsl-gcc-debug`、`build/codex-semantic-wsl-clang-debug` 和 `build\codex-semantic-msvc-debug`。三套构建中的 `zr_vm_logical_fact_emission_test` 均为 4 tests/0 failures，`zr_vm_expression_fact_emission_test` 均为 27 tests/0 failures，两个 LSP focused query targets 与两个 direct REPL smokes 均通过。构建仍可能输出当前 dirty checkout 里的既有 core/runtime/parser/library warning；本轮仍不声明全仓库绿色。

2026-06-05 parser/LSP member write reference fact RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_reference_fact_emission_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_reference_fact_emission_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_reference_fact_emission_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test -j 6 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_reference_fact_emission_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_reference_fact_emission_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test --parallel 6
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_reference_fact_emission_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_hover_test.exe
```

RED：新增 `tests/parser/test_reference_fact_emission.c` 的 member/index assignment 覆盖后，WSL gcc focused target 先因 `ZR_SEMANTIC_REFERENCE_MEMBER_WRITE` 不存在而编译失败。GREEN：`EZrSemanticReferenceKind` 新增 `MEMBER_WRITE`，assignment inference 对非 identifier 左值成功推断后在最后一个 member/index target token 写入 unresolved member-write reference fact；LSP local query/hover 把它格式化为 `Reference: member write` 和 `Symbol: value`。WSL gcc、WSL clang 和 Windows MSVC isolated focused parser/query/hover targets 均通过；clang helper prototype warning 已修正，剩余 const-qualifier/MSVC warnings 属于当前 dirty checkout 既有基线。本轮不声明 member declaration resolution、member-read facts、完整 Debug/REPL reference display 或全仓库绿色。

2026-06-05 REPL member-write reference display RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_cli_executable -j 6 && node tests/cli/repl_type_call_member_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_reference_fact_emission_test zr_vm_cli_executable -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_reference_fact_emission_test && node tests/cli/repl_type_call_member_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_reference_fact_emission_test zr_vm_cli_executable -j 6 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_reference_fact_emission_test && node tests/cli/repl_type_call_member_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_reference_fact_emission_test zr_vm_cli_executable --parallel 6
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_reference_fact_emission_test.exe
node tests\cli\repl_type_call_member_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe
```

RED：扩展 `tests/cli/repl_type_call_member_smoke.js` 查询 `:type seed.value = 3` 后，WSL gcc CLI smoke 失败为缺少 `Reference: member write value`；同一输出仍包含 `Type: int`，证明 parser/type inference 已能推断该 assignment expression，缺口只在 REPL reference fact 消费。GREEN：`repl_semantic_facts.c` 现在把 `ZR_SEMANTIC_REFERENCE_MEMBER_WRITE` 格式化为 `member write`，对 member expression 的非 computed property 也查询 reference fact，并且只有 `isResolved=true` 时才打印 `Declared at:`。WSL gcc、WSL clang 和 Windows MSVC isolated focused parser producer test 与 REPL consumer smoke 均通过；既有 CLI runtime const-qualifier warnings 仍属于当前 dirty checkout 基线。本轮不声明 member-read facts、member declaration resolution、完整 Debug reference display 或全仓库绿色。

2026-06-05 parser/LSP/REPL member-access reference fact RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_reference_fact_emission_test -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_reference_fact_emission_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_local_semantic_query_test zr_vm_cli -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && node tests/cli/repl_type_call_member_smoke.js ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_reference_fact_emission_test zr_vm_language_server_local_semantic_query_test zr_vm_cli -j 6 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_reference_fact_emission_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test && node tests/cli/repl_type_call_member_smoke.js ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build\codex-semantic-msvc-debug --target zr_vm_reference_fact_emission_test zr_vm_language_server_local_semantic_query_test --parallel 6
cmake --build build\codex-semantic-msvc-debug --target zr_vm_cli_executable --parallel 6
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_reference_fact_emission_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe
node .\tests\cli\repl_type_call_member_smoke.js .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe
```

RED：新增 `test_member_access_records_member_reference_fact` 后，WSL gcc focused parser target 失败在 `Expected Non-NULL`，证明 `seed.value` 的 `value` token 还没有 parser-owned reference fact。GREEN：`type_inference_record_member_access_reference_fact` 现在为成功推断的非 computed 点号成员读取写入 unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS`；`ZrParser_SemanticFacts_FindReferenceAtPosition` 在同一 token/range 上按引用种类优先级选择更具体的 write/member-write fact，保持 `seed.value = 3` 查询为 `MEMBER_WRITE`。LSP local reference query 覆盖 `seed.value` 的 member-access fact，REPL `:type seed.value` 显示 `Reference: member value`。WSL gcc、WSL clang 和 Windows MSVC focused parser/LSP/REPL tests 均通过；MSVC 第一次 build 使用了错误 target 名 `zr_vm_cli`，改用实际 target `zr_vm_cli_executable` 后通过。该 slice 当时仍不声明 computed member-read facts、member declaration resolution、完整 Debug reference display 或全仓库绿色；下一节覆盖 computed member-access 分类。

2026-06-05 parser/LSP/REPL computed member-access reference fact RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_reference_fact_emission_test -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_reference_fact_emission_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_local_semantic_query_test zr_vm_cli -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && node tests/cli/repl_type_call_member_smoke.js ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_reference_fact_emission_test zr_vm_language_server_local_semantic_query_test zr_vm_cli -j 6 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_reference_fact_emission_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test && node tests/cli/repl_type_call_member_smoke.js ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_reference_fact_emission_test zr_vm_language_server_local_semantic_query_test zr_vm_cli_executable --parallel 6
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_reference_fact_emission_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe
node .\tests\cli\repl_type_call_member_smoke.js .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe
```

RED：新增 `test_computed_member_access_records_member_reference_without_hiding_index_read` 后，WSL gcc focused parser target 先失败在 `Expected 5 Was 2`，证明 `seed[index]` 的 bracket query 还命中 read reference，而不是 computed member-access fact。GREEN：`type_inference_record_member_access_reference_fact` 现在为 computed member reads materialize index expression facts，并在完整 member expression range 写入 unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS`；reference query 的 narrower-range 优先规则让 `index` token 继续返回 resolved read，`[` 位置返回 member access。REPL reference traversal 为 computed member 选择 index 前的 interior query range，因此 `:type seed[index]` 显示 `Member: index` 和 `Reference: member index`。WSL gcc、WSL clang 和 Windows MSVC focused parser/LSP/REPL tests 均通过；本轮仍不声明 member declaration resolution、computed member-access hover/rich-hover coverage、完整 Debug reference display 或全仓库绿色。下一节覆盖 computed member-access hover/rich-hover。

2026-06-05 LSP computed member-access hover RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_computed_member_hover_test -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_computed_member_hover_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_reference_fact_emission_test -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_reference_fact_emission_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_computed_member_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_reference_fact_emission_test -j 6 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_computed_member_hover_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_reference_fact_emission_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_computed_member_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_reference_fact_emission_test --parallel 6
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_computed_member_hover_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_hover_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_reference_fact_emission_test.exe
```

RED：新增 `tests/language_server/test_lsp_computed_member_hover.c` 后，WSL gcc focused target 先失败：`ExpressionAt` 在 `[` 位置已经找到 `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS`，但 expression payload 仍是 base expression（`hasMember=0`），`GetHover` 返回空。GREEN：`lsp_local_semantic_query.c` 现在用 member reference fact 找回对应 member expression payload，并在需要时 materialize reference node；`lsp_interface.c` 在 signature help 之后让非 identifier 位置优先返回 local fact hover，避免 `[` 被 identifier fallback 归到左侧 `seed`。plain hover 和 rich hover 现在都在 `[` 位置显示 `Reference: member access`、`Symbol: index`、`Member: index`。WSL gcc、WSL clang 和 Windows MSVC focused parser/query/hover targets 均通过；本轮仍不声明 member declaration resolution、完整 Debug reference display 或全仓库绿色。

2026-06-05 LSP stdio computed member inline-value reference fact RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_stdio -j 6 && node tests/language_server/stdio_inline_value_semantic_smoke.js ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_stdio'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_stdio -j 6 && node tests/language_server/stdio_inline_value_semantic_smoke.js ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_stdio'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
where.exe cl
cmake --build build\agent-msvc-tests --config Debug --target zr_vm_language_server_stdio --parallel 6
node tests\language_server\stdio_inline_value_semantic_smoke.js build\agent-msvc-tests\bin\Debug\zr_vm_language_server_stdio.exe
```

RED：扩展 `tests/language_server/stdio_inline_value_semantic_smoke.js` 的 `seed[index];` 断言后，旧 stdio server 返回 `text:"member index"`，证明 inline-value consumer 已拿到 computed member expression payload，但没有消费同一 local semantic query 返回的 `SZrSemanticReferenceFact`。GREEN：`stdio_inline_value.c` 现在把 reference kind 格式化为 compact `reference ...` segment，computed member expression statement 返回 `member index, reference member access`。该 slice 只扩展 stdio inline-value consumer，不改变 parser/type-inference reference emission，不声明 member declaration resolution、完整 Debug reference display 或全仓库绿色；WSL gcc、WSL clang 和 Windows MSVC dedicated inline-value smoke 均通过，MSVC 仍有当前 dirty checkout 的既有 core/parser/library warning。

2026-06-05 Debug variable/evaluate child-shape metadata RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_debug_variable_child_shape_test -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_debug_variable_child_shape_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_debug_variable_child_shape_test -j 6 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_debug_variable_child_shape_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_debug_variable_child_shape_test --parallel 6
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_debug_variable_child_shape_test.exe
```

RED：新增 `tests/debug/test_debug_variable_child_shape.c` 后，WSL gcc focused protocol test 失败为 `Expected 36 Was 0`，证明 `variables` 对 `zr` 全局对象的聚合展开计数已经存在，但每个 value preview 缺少对应的 `namedVariables` / `indexedVariables`。GREEN：`debug_child_shape.c` 现在复用 Debug snapshot 的可见字段计数规则，为普通 object、direct array 和 debug index window 计算 child shape；`debug_protocol.c` 把该 metadata 写入 value preview、stack argument preview 和 `evaluate` result。WSL gcc、WSL clang 和 Windows MSVC focused protocol tests 均通过；这仍是 runtime debug snapshot metadata，不声明 parser semantic fact 复用、完整 Debug reference display 或全仓库绿色。

2026-06-05 Debug indexed evaluate reference-summary RED/GREEN 验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_debug_variable_child_shape_test -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_debug_variable_child_shape_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_debug_variable_child_shape_test -j 6 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_debug_variable_child_shape_test"
cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_debug_variable_child_shape_test --parallel 6
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_debug_variable_child_shape_test.exe
```

RED：扩展 `tests/debug/test_debug_variable_child_shape.c` 的普通 `evaluate("zr[1]")` 断言后，WSL gcc focused protocol test 失败为 `FAIL:global zr`，证明 evaluator 已保留 base reference，但没有把成功的 postfix index 读取暴露给 adapter。GREEN：`debug_eval.c` 现在在安全 member/index resolver 成功后追加 runtime reference suffix，`zr[1]` 返回 `global zr, index access`，成功 member 读取追加 `member <name>`。WSL gcc、WSL clang 和 Windows MSVC focused protocol tests 均通过；这仍是 runtime read-attribution metadata，不声明 parser reference fact 复用、member declaration resolution 或全仓库绿色。

2026-06-05 LSP stdio continuation-only initializer inline-value RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm; node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_stdio'
wsl bash -lc 'cd /mnt/e/Git/zr_vm; cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_stdio -j 6'
wsl bash -lc 'cd /mnt/e/Git/zr_vm; node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_stdio'
wsl bash -lc 'cd /mnt/e/Git/zr_vm; cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_stdio -j 6'
wsl bash -lc 'cd /mnt/e/Git/zr_vm; node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_stdio'
& "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Invoke-VsDevCommand.ps1" cmake --build build\codex-msvc-debug --target zr_vm_language_server_stdio --config Debug -j 6
node tests\language_server\stdio_inline_value_semantic_smoke.js build\codex-msvc-debug\bin\Debug\zr_vm_language_server_stdio.exe
```

RED：扩展 `tests/language_server/stdio_inline_value_semantic_smoke.js` 后，只请求 `var sum =\n  1 + 2;` 的 continuation expression 行时，旧 WSL gcc stdio server 返回 `values=[]`，证明 owner declaration line 不在 request range 时 initializer fact 丢失。GREEN：`stdio_inline_value.c` 现在在 request 从 continuation line 开始且 owner declaration line 不在请求范围内时回溯到 `var name =` owner line，并把 initializer expression 的共享 local semantic fact 仍锚定到变量名 range。为避免继续膨胀 stdio scanner，semantic fact text formatter/query bridge 已抽出到 `stdio_inline_value_semantic_text.c/.h`，`stdio_inline_value.c` 降到 855 行、formatter 模块 233 行。WSL gcc、WSL clang 和 Windows MSVC dedicated inline-value smokes 均通过；本轮不改变 parser/type-inference fact emission，不声明完整 VSCode adapter UI、Debug reference display 或全仓库绿色。

2026-06-05 LSP stdio aggregate expression-statement inline-value RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_stdio -j 6 && node tests/language_server/stdio_inline_value_semantic_smoke.js ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_stdio'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-semantic-wsl-gcc-debug -R "^language_server_stdio_inline_value_semantic_smoke$" --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_stdio -j 6 && node tests/language_server/stdio_inline_value_semantic_smoke.js ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_stdio && ctest --test-dir build/codex-semantic-wsl-clang-debug -R "^language_server_stdio_inline_value_semantic_smoke$" --output-on-failure'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
where.exe cl
cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_stdio --config Debug --parallel 6
node tests\language_server\stdio_inline_value_semantic_smoke.js .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_stdio.exe
ctest --test-dir build\codex-semantic-msvc-debug -C Debug -R "^language_server_stdio_inline_value_semantic_smoke$" --output-on-failure
```

RED：新增 `[1 + 2];` / `[true || false];` 的 stdio inline-value assertions 后，WSL gcc old build 返回 `textDocument/inlineValue must expose nested numeric facts for aggregate expression statements; values=[]`。GREEN：`stdio_inline_value.c` 只把 `[` 纳入 line-start expression statement 的保守起始集合，仍用 `ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt` 查询内部 operator/nested facts 并把文本锚定在 aggregate expression range；WSL gcc、WSL clang 和 Windows MSVC direct smokes 与 registered CTest 均通过。本 slice 不修改 parser/type inference 或 core runtime，也不声明全仓库绿色。

2026-06-05 LSP stdio object aggregate expression-statement inline-value RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && node tests/language_server/stdio_inline_value_semantic_smoke.js ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_stdio'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_stdio -j 8 && ctest --test-dir build/codex-semantic-wsl-gcc-debug -R "^language_server_stdio_inline_value_semantic_smoke$" --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_stdio -j 8 && ctest --test-dir build/codex-semantic-wsl-clang-debug -R "^language_server_stdio_inline_value_semantic_smoke$" --output-on-failure'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
where.exe cl
cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_stdio --parallel 8
ctest --test-dir build\codex-semantic-msvc-debug -C Debug -R "^language_server_stdio_inline_value_semantic_smoke$" --output-on-failure
```

RED：新增 `{[1 + 2]: 4};` inline-value assertion 后，旧 WSL gcc stdio server 返回 `values=[]`，证明 scanner 没有把 `{` 起始的 computed-key object aggregate 交给 shared local semantic query。随后新增多行普通 key object assertion `{\n  a: 1 + 2\n};`，WSL gcc 只返回 computed-key case，失败为 `textDocument/inlineValue must expose nested value facts for multi-line object expression statements`，证明 object-literal start detection 停在当前行，无法看到下一行的 key/value 形状。GREEN：`stdio_inline_value_scan.c/.h` 现在拥有 expression-statement start、跨换行 object-literal-looking start 检测、balanced statement-end 和 query-offset 选择；stdio consumer 只调用这些 scanner API 和 `ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt`。focused smoke 锁住 parser 已能稳定提供事实的 computed-key object form和多行 ordinary-key nested value fact，并且 `stdio_inline_value.c` 仍保持请求编排边界。Broad stdio smoke 还暴露过一个旧 completion assertion 仍要求 `Semantic facts: range 20..20` 作为 detail 前缀；测试现在检查 `range 20..20` 事实本身，以兼容 completion detail 先输出 `expression ...` / `constant ...` 的当前格式。本 slice 不声明完整 parser block/object 消歧、不修改 parser/type inference 或 core runtime，也不声明全仓库绿色。

2026-06-05 REPL expression fact kind/exact constant RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm; cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_cli_executable -j 6 && node tests/cli/repl_type_expression_fact_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli'
wsl bash -lc 'cd /mnt/e/Git/zr_vm; cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_expression_fact_emission_test zr_vm_cli_executable -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_expression_fact_emission_test && node tests/cli/repl_type_expression_fact_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli'
wsl bash -lc 'cd /mnt/e/Git/zr_vm; cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_expression_fact_emission_test zr_vm_cli_executable -j 6 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_expression_fact_emission_test && node tests/cli/repl_type_expression_fact_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_logical_comparison_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_call_member_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_ownership_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_reachability_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-clang-debug -R "^cli_repl_type_expression_fact_smoke$" --output-on-failure'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build\codex-semantic-msvc-debug --target zr_vm_expression_fact_emission_test zr_vm_cli_executable --config Debug -j 6
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_expression_fact_emission_test.exe
node tests\cli\repl_type_expression_fact_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe
```

RED：新增 `tests/cli/repl_type_expression_fact_smoke.js` 后，WSL gcc `:type 42` / `:type 1 + 2` / `:type "zr"` 仍只显示类型和 numeric range，不显示 expression kind/exactness 或 literal constants。GREEN 的第一步让 `repl_semantic_facts.c` 输出 `Expression: <kind> <exactness>` 和 expression fact constant payload，但随即暴露 parser producer gap：`1 + 2` 已有 numeric range `3..3`，expression fact 却没有 `hasConstant`。第二个 RED 在 `tests/parser/test_expression_fact_emission.c` 锁住 binary expression constant payload；最终 GREEN 让 `type_inference_constant_eval.c` 对整数 unary `+`/`-` 和 binary `+`/`-`/`*` 做安全 int64 常量折叠，溢出时不写 exact constant。WSL gcc、WSL clang 和 Windows MSVC focused parser/CLI targets 均通过；相邻 REPL numeric/logical/call/member/ownership/reachability type smokes 也通过。本轮仍不声明 division/modulo folds、unsigned expression constants、Debug fact consumption 或全仓库绿色；string constant escaping display 由后续独立 REPL slice 覆盖。

2026-06-05 REPL expression fact escaped string constant RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_cli_executable -j 6 >/tmp/zr_repl_red_build.log && node tests/cli/repl_type_expression_fact_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_cli_executable zr_vm_expression_fact_emission_test -j 6 && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_expression_fact_emission_test && for script in tests/cli/repl_type_expression_fact_smoke.js tests/cli/repl_type_call_member_smoke.js tests/cli/repl_type_logical_comparison_smoke.js tests/cli/repl_type_ownership_smoke.js tests/cli/repl_type_reachability_smoke.js; do node "$script" build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli || exit 1; done'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_cli_executable zr_vm_expression_fact_emission_test -j 6 && build/codex-semantic-wsl-clang-debug/bin/zr_vm_expression_fact_emission_test && for script in tests/cli/repl_type_expression_fact_smoke.js tests/cli/repl_type_call_member_smoke.js tests/cli/repl_type_logical_comparison_smoke.js tests/cli/repl_type_ownership_smoke.js tests/cli/repl_type_reachability_smoke.js; do node "$script" build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli || exit 1; done'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
where.exe cl
cmake --build build\codex-semantic-msvc-debug --target zr_vm_cli_executable zr_vm_expression_fact_emission_test --config Debug --parallel 6
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_expression_fact_emission_test.exe
node tests\cli\repl_type_expression_fact_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe
```

RED：扩展 `tests/cli/repl_type_expression_fact_smoke.js` 后，旧 REPL `:type "a\"b\\c\n\t"` 输出 raw `Constant: "a"b\c`，后续 decoded newline/tab 把常量拆成多行，证明 shared expression fact 字符串常量的 display path 没有做 escaped formatting。GREEN：`repl_semantic_facts.c` 新增 byte-length based escaped writer，对 quote、backslash、`\n`、`\t`、`\r`、`\b`、`\f` 和其他低控制字节输出稳定 escape 序列，并继续保留已有 `Constant: "zr"` display。WSL gcc、WSL clang 和 Windows MSVC focused parser/CLI targets 均通过；相邻 REPL type smokes 也通过。本轮只声明 REPL `:type` string constant display，不改变 parser fact emission、LSP hover formatter、Debug fact consumption 或全仓库绿色。

2026-06-05 LSP expression fact hover RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm; cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_expression_fact_hover_test -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_expression_fact_hover_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm; cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_expression_fact_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_computed_member_hover_test -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_expression_fact_hover_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_computed_member_hover_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm; cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_expression_fact_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_computed_member_hover_test -j 6 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_expression_fact_hover_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_computed_member_hover_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_expression_fact_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_computed_member_hover_test --config Debug -j 6
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_expression_fact_hover_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_computed_member_hover_test.exe
```

RED：`tests/language_server/test_lsp_expression_fact_hover.c` 证明 `ExpressionAt` 已返回 `ZR_SEMANTIC_EXPRESSION_FACT_BINARY`、`ZR_SEMANTIC_FACT_EXACT`、`Constant: 3` 的底层事实，但 local/public hover 没有显示 expression fact kind/exactness 或 constant。GREEN：新增 `lsp_local_semantic_expression_text.c/.h` 专门格式化 expression fact hover 文本，`lsp_local_semantic_query.c` 同时在 standalone local fact hover 和追加到普通 symbol hover 的 fact markdown 中调用该 formatter。WSL gcc、WSL clang focused expression/local-query/local-hover/computed-member hover targets 均通过；Windows MSVC expression-hover、local-query、computed-member hover targets 通过。后续 escaped-string slice 扩展同一个 formatter；本轮不声明完整 Windows LSP hover suite 或全仓库绿色。

2026-06-05 LSP expression fact rich-hover role RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_expression_fact_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_computed_member_hover_test -j 6 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_expression_fact_hover_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_computed_member_hover_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_expression_fact_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_computed_member_hover_test -j 6 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_expression_fact_hover_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_computed_member_hover_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --config Debug --target zr_vm_language_server_expression_fact_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_computed_member_hover_test --parallel 6; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_expression_fact_hover_test.exe; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_hover_test.exe; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_computed_member_hover_test.exe
```

RED：扩展 expression hover 测试后，plain/public hover 已显示 `Expression: binary exact` 和 `Constant: 3`，但 rich-hover 中这些 sections 仍落入 generic `detail` role。GREEN：`lsp_interface.c` 的既有 rich-hover label-to-role table 增加 `Expression -> expression` 和 `Constant -> constant`，没有改变 parser/type inference fact emission 或 hover formatter。WSL gcc、WSL clang 和 Windows MSVC focused expression/query/local-hover/computed-member hover targets 均通过；本轮不声明全仓库绿色。

2026-06-05 LSP expression fact escaped string constant hover RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_expression_fact_hover_test -j 6 && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_expression_fact_hover_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_expression_fact_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_computed_member_hover_test zr_vm_language_server_lsp_interface_test -j 6 && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_expression_fact_hover_test && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_hover_test && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_computed_member_hover_test && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_expression_fact_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_computed_member_hover_test zr_vm_language_server_lsp_interface_test -j 6 && build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_expression_fact_hover_test && build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test && build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_hover_test && build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_computed_member_hover_test && build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
where.exe cl
cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_expression_fact_hover_test zr_vm_language_server_local_semantic_query_test zr_vm_language_server_local_semantic_hover_test zr_vm_language_server_computed_member_hover_test zr_vm_language_server_lsp_interface_test --config Debug --parallel 6
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_expression_fact_hover_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_hover_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_computed_member_hover_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_lsp_interface_test.exe
```

RED：扩展 `tests/language_server/test_lsp_expression_fact_hover.c` 后，旧 LSP formatter 对 `return "a\"b\\c\n\t";` 的 shared string constant 输出 raw `Constant: "a"b\c`，decoded newline/tab 把 local hover markdown 拆成多行，public hover 和 rich hover 也无法稳定暴露 escaped constant。GREEN：`lsp_local_semantic_expression_text.c` 现在按 `ZrCore_String_GetByteLength` 输出 escaped string constants，覆盖 quote、backslash、`\n`、`\t`、`\r`、`\b`、`\f` 和其他低控制字节；plain local hover、public hover 和 rich-hover `constant` role 都显示同一 escaped payload。WSL gcc、WSL clang 和 Windows MSVC focused expression/query/local-hover/computed-member/interface targets 均通过；本轮不改变 parser fact emission、REPL display 或 Debug fact consumption，不声明全仓库绿色。

2026-06-05 LSP completion initializer expression fact detail RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_inlay_semantic_facts_test -j 6 && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_inlay_semantic_facts_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_inlay_semantic_facts_test zr_vm_language_server_lsp_interface_test -j 6 && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_inlay_semantic_facts_test && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_inlay_semantic_facts_test zr_vm_language_server_lsp_interface_test -j 6 && build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_inlay_semantic_facts_test && build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
where.exe cl
cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_inlay_semantic_facts_test zr_vm_language_server_lsp_interface_test --config Debug --parallel 6
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_inlay_semantic_facts_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_lsp_interface_test.exe
```

RED：新增 `LSP Completion Detail Uses Initializer Expression Fact` 后，`sum` completion detail 仍只有 `private int` 和 `Semantic facts: range 3..3, unsigned 3..3`，缺少 `expression binary exact` 与 `constant 3`。GREEN：`lsp_completion_semantic_facts.c` 现在读取 initializer 的 `SZrSemanticExpressionFact` 并在 numeric/logical/ownership details 前追加紧凑 `expression <kind> <exactness>` 与 scalar constant payload。随后单独新增 escaped-string RED，证明 `label` completion detail 只有 `expression literal exact`，没有 escaped `constant "a\"b\\c\n\t"`；GREEN 后同一个 completion formatter 用 byte-length escaped writer 输出 quote、backslash、newline、tab、carriage-return 和低控制字节。调试中还发现既有 inlay hint test 使用 exact label `: int, range 3..3`，而当前 numeric fact label 已合法追加 `unsigned 3..3`；测试改为要求同一 label 包含 `: int` 与 `range 3..3`，不改变 inlay production code。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_inlay_semantic_facts_test` 与 `zr_vm_language_server_lsp_interface_test` 均通过；本轮只声明 local-variable initializer completion facts，不声明 full completion resolve UI、signature-help expression fact parity、Debug fact consumption 或全仓库绿色。

2026-06-05 LSP signature-help argument expression fact docs RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_inlay_semantic_facts_test -j 6 && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_inlay_semantic_facts_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_inlay_semantic_facts_test zr_vm_language_server_lsp_interface_test -j 6 && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_inlay_semantic_facts_test && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_inlay_semantic_facts_test zr_vm_language_server_lsp_interface_test -j 6 && build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_inlay_semantic_facts_test && build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_lsp_interface_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
where.exe cl
cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_inlay_semantic_facts_test zr_vm_language_server_lsp_interface_test --config Debug --parallel 6
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_inlay_semantic_facts_test.exe
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_lsp_interface_test.exe
```

RED：扩展 `LSP Signature Help Parameter Docs Use Argument Semantic Facts` 后，`pick(1 + 2, true || false)` 的 numeric parameter documentation 仍只有 `Argument semantic facts: range 3..3, unsigned 3..3`，缺少 `expression binary exact` 与 `constant 3`。GREEN：`lsp_signature_semantic_facts.c` 现在在 argument fact materialization 后读取 `SZrSemanticExpressionFact`，并把紧凑 `expression <kind> <exactness>` 与 scalar constant payload 放在 numeric/logical/ownership details 前；signature docs 继续保留既有 `range 3..3`、`logical true`、`short-circuits` 和 ownership violation text。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_language_server_inlay_semantic_facts_test` 与 `zr_vm_language_server_lsp_interface_test` 均通过；本轮只声明 signature-help argument expression fact docs，不声明 full completion resolve UI、Debug fact consumption 或全仓库绿色。

2026-06-05 REPL prior function call reference RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_cli_executable -j 6 >/tmp/zr_repl_call_ref_build.log && node tests/cli/repl_type_call_reference_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_cli_executable -j 6 && node tests/cli/repl_type_call_reference_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_call_member_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_expression_fact_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_logical_comparison_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_ownership_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_reachability_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-gcc-debug -R "^cli_repl_type_call_reference_smoke$" --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_cli_executable -j 6 && node tests/cli/repl_type_call_reference_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_call_member_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_expression_fact_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_logical_comparison_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_ownership_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_reachability_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-clang-debug -R "^cli_repl_type_call_reference_smoke$" --output-on-failure'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
where.exe cl
cmake -S . -B build\codex-semantic-msvc-debug
cmake --build build\codex-semantic-msvc-debug --target zr_vm_cli_executable --config Debug --parallel 6
node tests\cli\repl_type_call_reference_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe
node tests\cli\repl_type_call_member_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe
node tests\cli\repl_type_expression_fact_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe
node tests\cli\repl_type_logical_comparison_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe
node tests\cli\repl_type_ownership_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe
node tests\cli\repl_type_reachability_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe
ctest --test-dir build\codex-semantic-msvc-debug -C Debug -R "^cli_repl_type_call_reference_smoke$" --output-on-failure
```

RED：新增 `tests/cli/repl_type_call_reference_smoke.js` 后，先在 REPL 提交 `func pick(value: int): int { ... }`，再运行 `:type pick(42)`，旧输出仍是 `Type: object`、`Expression: member exact`、`Call: pick args=1`，没有 `Reference: call pick` 或 `Declared at:`，证明 `:type` 的临时类型环境只重放了 prior variables，没有重放 prior function declarations。GREEN：`repl.c` 现在在 `:type` 分析前先把 prior function signatures 注册到临时 type environment，再按原路径注册 prior variables；parser/type-inference 已有的 function-call reference fact 因此能被 `repl_semantic_facts.c` 直接显示。后续 RED 将同一 smoke 改为 `:type pick(1 + 2)` 后，输出已有 `Numeric range: 3..3`，但没有 argument 的 `Expression: binary exact` / `Constant: 3`，证明 expression-fact walker 在 call/member payload 上停止下钻。GREEN：`repl_semantic_fact_walkers.c` 现在把 call facts 和带 call payload 的 member facts 当作可下钻容器，继续遍历 argument facts，同时保持 binary fact 之后停止下钻以避免 literal leaf noise。WSL gcc、WSL clang 和 Windows MSVC focused CLI target plus direct/registered REPL type smokes 均通过；本轮只声明 REPL `:type` 对 prior typed function calls 的 return type、resolved call reference display 和 call-argument expression fact display，不声明泛型函数签名 parity、完整 Debug fact consumption 或全仓库绿色。

2026-06-05 Debug conditional branch diagnostics RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_debug_expression_diagnostics_test -j 6 && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_debug_expression_diagnostics_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_debug_expression_diagnostics_test -j 6 && build/codex-semantic-wsl-clang-debug/bin/zr_vm_debug_expression_diagnostics_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
where.exe cl
cmake --build build\codex-semantic-msvc-debug --target zr_vm_debug_expression_diagnostics_test --config Debug --parallel 6
.\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_debug_expression_diagnostics_test.exe
```

RED：扩展 `tests/debug/test_debug_expression_diagnostics.c` 后，`true ? : 2` 仍返回 generic `Invalid expression after '?'`，`true ? 1 :` 仍返回 generic `Missing expression after ':'`，证明 conditional safe-evaluate branch 缺失没有使用结构化诊断。GREEN：`debug_eval_diagnostics.c` 新增 missing consequent/alternate branch 诊断，`debug_eval.c` 在泛化 right-operand parser 前做不消费输入的 branch-boundary lookahead。WSL gcc、WSL clang 和 Windows MSVC focused `zr_vm_debug_expression_diagnostics_test` 均通过，18 tests/0 failures；本轮只声明 Debug safe-evaluate 条件表达式缺 branch 的诊断质量，不声明完整 parser fact consumption 或全仓库绿色。

2026-06-05 REPL/LSP conditional branch fact consumer RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_cli_executable zr_vm_language_server_local_semantic_query_test -j 6 && node tests/cli/repl_type_conditional_branch_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-gcc-debug -R "^cli_repl_type_conditional_branch_smoke$" --output-on-failure && build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_local_semantic_query_test'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_cli_executable zr_vm_language_server_local_semantic_query_test -j 6 && node tests/cli/repl_type_conditional_branch_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-clang-debug -R "^cli_repl_type_conditional_branch_smoke$" --output-on-failure && build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_local_semantic_query_test'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake -S . -B build\codex-semantic-msvc-debug; cmake --build build\codex-semantic-msvc-debug --target zr_vm_cli_executable zr_vm_language_server_local_semantic_query_test --config Debug --parallel 6; node tests\cli\repl_type_conditional_branch_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; ctest --test-dir build\codex-semantic-msvc-debug -C Debug -R "^cli_repl_type_conditional_branch_smoke$" --output-on-failure; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_local_semantic_query_test.exe
```

RED：新增 `tests/cli/repl_type_conditional_branch_smoke.js` 后，WSL gcc `:type true ? 1 : 2` 已输出 `Type: int`、selected `Numeric range: 1..1`、`Expression: conditional exact` 和 skipped-branch reachability，但没有输出 constant condition 的 `Logical value: true`，证明 REPL 只读取了 root expression 的 logical fact。`tests/language_server/test_lsp_local_semantic_query.c` 新增的 conditional branch query 是 characterization 覆盖，旧 local query 已能在 skipped branch 位置返回 conditional expression fact、numeric/logical/reachability facts。GREEN：`repl_semantic_facts.c` 新增 expression-wide logical fact traversal，`repl.c` 改为调用该共享 writer；REPL 不新增条件分支推断，只遍历当前 expression AST 并读取 `SZrSemanticContext`。WSL gcc、WSL clang 和 Windows MSVC focused CLI/LSP local-query 验证均通过；本轮只声明 REPL 条件表达式 nested logical display 和 LSP query coverage，不声明完整 REPL/LSP parity 或全仓库绿色。

2026-06-05 REPL nested numeric fact display RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_cli_executable -j 6 && node tests/cli/repl_type_nested_numeric_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_expression_fact_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_conditional_branch_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_reachability_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-gcc-debug -R "^cli_repl_type_nested_numeric_smoke$" --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_cli_executable -j 6 && node tests/cli/repl_type_nested_numeric_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_expression_fact_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_conditional_branch_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_reachability_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-clang-debug -R "^cli_repl_type_nested_numeric_smoke$" --output-on-failure'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake -S . -B build\codex-semantic-msvc-debug; cmake --build build\codex-semantic-msvc-debug --target zr_vm_cli_executable --config Debug --parallel 6; node tests\cli\repl_type_nested_numeric_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_expression_fact_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_conditional_branch_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_reachability_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; ctest --test-dir build\codex-semantic-msvc-debug -C Debug -R "^cli_repl_type_nested_numeric_smoke$" --output-on-failure
```

RED：新增 `tests/cli/repl_type_nested_numeric_smoke.js` 后，WSL gcc `:type [1 + 2]` 只输出 `Type: int[1]<int>` 和 `Expression: array exact`，没有输出 element expression 的 `Numeric range: 3..3`，证明 REPL numeric display 仍是 root-only。GREEN：新增 `repl_semantic_expression_walk.c/.h` 作为 REPL-local expression AST walker，`repl_semantic_facts.c` 通过它遍历 numeric facts 并在命中 structural fact 后停止下钻，`repl.c` 改为调用 expression-wide numeric writer。WSL gcc、WSL clang 和 Windows MSVC focused CLI smokes 与 registered CTest 均通过；本轮只声明 REPL nested numeric fact display，不声明新的 parser numeric inference 或全仓库绿色。

2026-06-05 REPL nested expression fact display RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_cli_executable -j 6 && node tests/cli/repl_type_nested_expression_fact_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_nested_numeric_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_expression_fact_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_conditional_branch_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_reachability_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-gcc-debug -R "^cli_repl_type_nested_expression_fact_smoke$" --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_cli_executable -j 6 && node tests/cli/repl_type_nested_expression_fact_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_nested_numeric_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_expression_fact_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_conditional_branch_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_reachability_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-clang-debug -R "^cli_repl_type_nested_expression_fact_smoke$" --output-on-failure'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake -S . -B build\codex-semantic-msvc-debug; cmake --build build\codex-semantic-msvc-debug --target zr_vm_cli_executable --config Debug --parallel 6; node tests\cli\repl_type_nested_expression_fact_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_nested_numeric_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_expression_fact_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_conditional_branch_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_reachability_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; ctest --test-dir build\codex-semantic-msvc-debug -C Debug -R "^cli_repl_type_nested_expression_fact_smoke$" --output-on-failure
```

RED：新增 `tests/cli/repl_type_nested_expression_fact_smoke.js` 后，WSL gcc `:type [1 + 2]` 已输出 array root expression fact 和 nested numeric range，但没有输出 nested `Expression: binary exact` / `Constant: 3`，证明 REPL expression display 仍是 root-only。GREEN：`repl_semantic_facts.c` 新增 expression-wide writer 并复用 `repl_semantic_expression_walk.c/.h`；array/object root facts 会继续下钻，binary fact 会输出后停止下钻以避免 literal leaf noise。WSL gcc、WSL clang 和 Windows MSVC focused CLI smokes 与 registered CTest 均通过；本轮只声明 REPL nested expression fact display，不声明新的 parser expression fact production 或全仓库绿色。

2026-06-05 REPL nested ownership fact display RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_cli_executable -j 6 && node tests/cli/repl_type_nested_ownership_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_ownership_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_nested_expression_fact_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_nested_numeric_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && node tests/cli/repl_type_expression_fact_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-gcc-debug -R "^cli_repl_type_nested_ownership_smoke$" --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-semantic-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_cli_executable -j 6 && node tests/cli/repl_type_nested_ownership_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_ownership_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_nested_expression_fact_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_nested_numeric_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && node tests/cli/repl_type_expression_fact_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-clang-debug -R "^cli_repl_type_nested_ownership_smoke$" --output-on-failure'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake -S . -B build\codex-semantic-msvc-debug; cmake --build build\codex-semantic-msvc-debug --target zr_vm_cli_executable --config Debug --parallel 6; node tests\cli\repl_type_nested_ownership_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_ownership_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_nested_expression_fact_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_nested_numeric_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; node tests\cli\repl_type_expression_fact_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe; ctest --test-dir build\codex-semantic-msvc-debug -C Debug -R "^cli_repl_type_nested_ownership_smoke$" --output-on-failure
```

RED：新增 `tests/cli/repl_type_nested_ownership_smoke.js` 后，WSL gcc `:type [%borrow(owner)]` 已输出 aggregate borrowed type、array expression fact、ownership builtin expression fact，但没有输出 nested `Ownership: borrow %borrowed`，证明 REPL ownership display 仍是 root-only。GREEN：新增 `repl_semantic_fact_walkers.c`，把 expression-tree numeric/expression visitors 从 `repl_semantic_facts.c` 移出并加入 ownership visitor；`repl.c` 改为调用 expression-wide ownership writer。WSL gcc、WSL clang 和 Windows MSVC focused CLI smokes 与 registered CTest 均通过；本轮只声明 REPL nested ownership fact display，不声明新的 parser ownership inference 或全仓库绿色。

2026-06-05 REPL aggregate-start expression execution RED/GREEN 验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_cli_executable zr_vm_instruction_execution_test -j 6 && node tests/cli/repl_expression_aggregate_smoke.js build/codex-semantic-wsl-gcc-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-gcc-debug -R "^cli_repl_expression_aggregate_smoke$" --output-on-failure'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_cli_executable zr_vm_instruction_execution_test -j 6 && node tests/cli/repl_expression_aggregate_smoke.js build/codex-semantic-wsl-clang-debug/bin/zr_vm_cli && ctest --test-dir build/codex-semantic-wsl-clang-debug -R "^cli_repl_expression_aggregate_smoke$" --output-on-failure'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-semantic-msvc-debug --target zr_vm_cli_executable zr_vm_instruction_execution_test --config Debug --parallel 6; ctest --test-dir build\codex-semantic-msvc-debug -C Debug -R "^cli_repl_expression_aggregate_smoke$" --output-on-failure; node tests\cli\repl_expression_aggregate_smoke.js build\codex-semantic-msvc-debug\bin\Debug\zr_vm_cli.exe
```

RED：新增 `tests/cli/repl_expression_aggregate_smoke.js` 后，WSL gcc 对普通 REPL 输入 `[1 + 2][0]` 先报 `Expected ';'` / `期望 ';'`，证明 REPL expression wrapper 把行首 `[` 排除在裸表达式之外；放开 scanner 后又暴露 `SET_BY_INDEX: receiver must be an object or array`，证明数组字面量元素初始化在执行层覆盖了接收者槽位或读取了错误 frame slot。GREEN：`repl.c` 允许 `[` 起始的完整裸表达式进入 wrapping；`compile_array_literal` 把元素表达式编译到 receiver 之后的临时槽，避免覆盖 array receiver；`CREATE_ARRAY` / `CREATE_OBJECT` / `SET_BY_INDEX` 在 dispatch 中刷新逻辑 frame destination/value slots，确保 array literal 元素写入真实 receiver。WSL gcc、WSL clang 和 Windows MSVC focused REPL smoke 与 registered CTest 均通过；`CREATE_ARRAY With Elements Execution` 的 focused 输出在 WSL gcc/clang 下显示 `Result: [1, 2, 3]` 并通过。本轮只声明 aggregate-start REPL expression execution 和数组字面量元素写入路径，不声明 array value stringification、AOT/value-type runtime 或全仓库绿色。

2026-06-07 parser/LSP using resource close RED/GREEN 补充：

继续扩展 `%using (resource { return resource; }` RED 时，旧路径只失败 using resource close case；GREEN 新增 `missing_using_resource_close` builder/reporter，并让 `parse_using_statement_body` 在 using resource 到达 using body 前仍未看到 `)` 时报告结构化 using-resource-close 诊断并释放已解析 resource。WSL gcc、WSL clang 和 Windows MSVC focused statement/declaration/parser diagnostics targets 均通过；本轮仍不声明全仓库绿色，也不触碰 constructor receiver、SemIR、AOT/value-type、Debug 或 REPL 路径。

2026-06-07 parser/LSP switch case header close RED/GREEN 补充：

继续扩展 `switch (choice) { (1 { return 1; } }` RED 时，旧路径只失败 switch case header close case；GREEN 新增 `missing_switch_case_header_close` builder/reporter，并让 `parse_switch_expression` 在普通 switch case expression 到达 case body 前仍未看到 `)` 时报告结构化 switch-case-header-close 诊断并释放已解析 switch expression、case value 和 case array。WSL gcc、WSL clang 和 Windows MSVC focused statement/declaration/parser diagnostics targets 均通过；本轮仍不声明全仓库绿色，也不触碰 constructor receiver、SemIR、AOT/value-type、Debug 或 REPL 路径。

2026-06-07 parser/LSP switch body close RED/GREEN 补充：

继续扩展 `switch (choice) { (1) { return 1; }` RED 时，旧路径只失败 switch body close case；GREEN 新增 `missing_switch_body_close` builder/reporter，并让 `parse_switch_expression` 在 switch body 到达输入结束前仍未看到最终 `}` 时报告结构化 switch-body-close 诊断并释放已解析 switch expression、case array 和 default case。GCC statement-only focused 验证已通过；完整 gcc/clang/MSVC focused statement/declaration/parser diagnostics 组合结果记录在 acceptance evidence 中。本轮仍不声明全仓库绿色，也不触碰 constructor receiver、SemIR、AOT/value-type、Debug 或 REPL 路径。

2026-06-07 parser/LSP statement body opener RED/GREEN 验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_statement_parser_diagnostics_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_declaration_parser_diagnostics_test && ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_parser_diagnostics_test"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test -j 8 && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_statement_parser_diagnostics_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_declaration_parser_diagnostics_test && ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_parser_diagnostics_test"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; cmake --build build\codex-semantic-msvc-debug --target zr_vm_language_server_statement_parser_diagnostics_test zr_vm_language_server_declaration_parser_diagnostics_test zr_vm_language_server_parser_diagnostics_test --config Debug --parallel 8; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_statement_parser_diagnostics_test.exe; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_declaration_parser_diagnostics_test.exe; .\build\codex-semantic-msvc-debug\bin\Debug\zr_vm_language_server_parser_diagnostics_test.exe
```

RED：新增 `tests/language_server/test_lsp_statement_parser_diagnostics.c` 后，WSL gcc 先失败于 `if (ready)\nreturn 1;`，证明旧 parser/LSP 路径没有 `missing_statement_body_open` code、具体问题文本和建议。GREEN：`diagnostic_builder.c` / `parser_diagnostics.c` 新增 shared builder/reporter，`parse_if_expression`、`parse_while_loop` 和 traditional `parse_for_loop` 在控制语句 header 后未看到 `{` 时报告结构化 body-opener 诊断；`for` fixture 使用 `for (;;)`，避免当前 `for(var ...)` / `foreach` 分流细节干扰 body-opener 覆盖。后续扩展 `switch (choice)\nreturn 1;` RED 时，旧路径只失败 switch body-open case；GREEN 让 `parse_switch_expression` 在消费 `)` 后、进入 cases 解析前复用同一 reporter。继续扩展 `switch (choice) { (1)\nreturn 1; }` 和 `switch (choice) { ()\nreturn 1; }` RED 时，旧路径先失败 switch case body-open case；GREEN 让 `parse_switch_expression` 在普通 case/default case header 后进入 block parser 前复用同一 reporter，并清理已解析的 switch expression、case value 和 case array。再扩展 `if (ready) { return 1; } else\nreturn 2;` RED 时，旧路径只失败 else body-open case；GREEN 让 `parse_if_expression` 的 plain `else` branch 在进入 block parser 前复用同一 reporter，而 `else if` 继续走 nested-if 解析。继续扩展 `for (var item in items)\nreturn item;` RED 时，旧路径只失败 foreach body-open case；GREEN 让 `parse_foreach_loop` 在消费 `)` 后、进入 block parser 前复用同一 reporter，并清理已解析的 pattern/type/iterable expression。继续扩展 `try\nreturn 1;`、`try { throw 1; } catch (error)\nreturn 2;` 和 `try { return 1; } finally\nreturn 2;` RED 时，旧路径先失败 try body-open case；GREEN 让 `parse_try_catch_finally_statement` 在 try/catch/finally header 后进入 block parser 前复用同一 reporter，并在 catch/finally 缺 body-opener 时清理已解析的 try block、catch pattern 和已有 catch clauses。继续扩展 `%using (resource)\nreturn resource;` RED 时，旧路径只失败 using body-open case；GREEN 让 `parse_using_statement_body` 的 block-scoped `using (...)` branch 在进入 block parser 前复用同一 reporter，并清理已解析 resource。继续扩展 `if (ready) { return 1;` RED 时，旧路径只失败 missing block close case；GREEN 新增 `missing_block_close` builder/reporter，并让 `parse_block` 在到达 EOS 前仍未看到 `}` 时报告结构化 block-close 诊断并释放已解析 statement array。继续扩展 `try { throw 1; } catch (error { return 2; }` RED 时，旧路径只失败 catch pattern close case；GREEN 新增 `missing_catch_pattern_close` builder/reporter，并让 `parse_try_catch_finally_statement` 在 catch pattern 到达 catch body 前仍未看到 `)` 时报告结构化 catch-pattern-close 诊断并释放已解析 catch pattern、已有 catch clauses 和 try block。WSL gcc、WSL clang 和 Windows MSVC focused statement/declaration/parser diagnostics targets 均通过；本轮不声明全仓库绿色，也不触碰 constructor receiver、SemIR、AOT/value-type、Debug 或 REPL 路径。
