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
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
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
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c
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
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_child_shape.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval_diagnostics.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval_internal.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_semantic_facts.c
  - tests/parser/test_semantic_facts.c
  - tests/parser/test_expression_fact_emission.c
  - tests/parser/test_reference_fact_emission.c
  - tests/parser/test_logical_fact_emission.c
  - tests/parser/test_object_literal_key_lowering.c
  - tests/parser/test_parser.c
  - tests/parser/test_type_inference.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_ownership_diagnostics.c
  - tests/language_server/test_lsp_inlay_semantic_facts.c
  - tests/language_server/test_lsp_logical_semantic_query.c
  - tests/language_server/test_lsp_local_semantic_query.c
  - tests/language_server/test_lsp_local_semantic_hover.c
  - tests/language_server/test_lsp_computed_member_hover.c
  - tests/language_server/test_lsp_reachability_semantic_query.c
  - tests/language_server/test_lsp_call_member_semantic_query.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/stdio_smoke.js
  - tests/language_server/stdio_inline_value_semantic_smoke.js
  - tests/debug/test_debug_expression_diagnostics.c
  - tests/debug/test_debug_variable_child_shape.c
  - tests/cli/test_cli_repl_e2e.c
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
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
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
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c
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
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_child_shape.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval_diagnostics.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_eval_internal.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_protocol.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug_snapshot.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
  - tests/parser/test_parser.c
  - tests/parser/test_expression_fact_emission.c
  - tests/parser/test_reference_fact_emission.c
  - tests/parser/test_logical_fact_emission.c
  - tests/parser/test_object_literal_key_lowering.c
  - tests/debug/test_debug_expression_diagnostics.c
  - tests/debug/test_debug_variable_child_shape.c
  - tests/cli/test_cli_repl_e2e.c
  - tests/language_server/test_ownership_diagnostics.c
  - tests/language_server/test_lsp_inlay_semantic_facts.c
  - tests/language_server/test_lsp_logical_semantic_query.c
  - tests/language_server/test_lsp_local_semantic_query.c
  - tests/language_server/test_lsp_local_semantic_hover.c
  - tests/language_server/test_lsp_computed_member_hover.c
  - tests/language_server/test_lsp_call_member_semantic_query.c
  - tests/CMakeLists.txt
plan_sources:
  - user: 2026-06-03 语义推断、诊断、LSP、Debug、REPL 能力增强目标
  - docs/superpowers/specs/2026-06-03-zr-vm-semantic-inference-design.md
  - docs/superpowers/plans/2026-06-03-zr-vm-semantic-inference-fact-layer.md
  - .codex/plans/ZR_LSP 现代能力对齐计划.md
  - .codex/plans/Rust-First using  Ownership 语义收敛计划.md
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
  - tests/language_server/test_lsp_call_member_semantic_query.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/stdio_smoke.js
  - tests/language_server/stdio_inline_value_semantic_smoke.js
  - tests/debug/test_debug_expression_diagnostics.c
  - tests/debug/test_debug_variable_child_shape.c
  - tests/cli/test_cli_repl_e2e.c
  - tests/cli/repl_type_ownership_smoke.js
  - tests/cli/repl_type_call_member_smoke.js
  - tests/cli/repl_type_logical_comparison_smoke.js
  - tests/cli/repl_type_reachability_smoke.js
  - tests/acceptance/2026-06-03-semantic-inference-fact-layer.md
doc_type: module-detail
---

# Semantic Fact Layer

`SZrSemanticContext` 是编译期语义事实的唯一共享容器。parser、type inference、semantic analyzer 负责把表达式类型、引用、数值、可达性、逻辑和所有权事实写入这里；LSP 已开始查询这些事实，Debug 和 REPL 已开始接入同一诊断/表达式方向，完整事实复用仍在后续推进，避免各自复制一套局部推断规则。

当前阶段建立容器和查询契约，并已开始从 type inference 写入表达式、数值、确定性逻辑和 ownership builtin 事实，从 LSP semantic analyzer 写入声明/使用引用事实、不可达事实、编辑器短路/分支逻辑事实和所有权违规事实。parser 也开始把代表性语法错误转换成结构化诊断，再通过 LSP 展示具体原因和建议。LSP 局部查询已接入表达式事实、数值事实、引用事实、诊断失败和显式 unknown 分流；hover/rich hover 现在也能把调用/成员 expression payload 作为 `Call:` / `Member:` 行和稳定 role 暴露给编辑器，并能在 `seed[index]` 的 `[` 这类非 identifier 位置通过 member-access reference fact 恢复 computed member payload。Stdio inline values 也复用同一 local semantic query，覆盖 local initializer、简单 return、简单 line-start expression statement（包括 `!true;` / `-42;` 这类 unary 起始形式，以及 `pick(42);` / `seed.value;` / `seed[index];` 这类 call/member 起始形式）、多行 return expression、return 后换行开始的 expression、等号后换行的 local initializer，以及 request range 从 continuation line 开始的简单 operator-split expression statement 的 fact-backed `InlineValueText`；当同一 query 返回 reference fact 时，inline value 会追加 `reference ...`，例如 computed member read 的 `reference member access`。Debug 和 REPL 已开始以安全局部方式消费诊断和表达式能力；Debug 数据面现在也在协议边界报告 value preview / evaluate result 的 named/indexed child shape，帮助 adapter 做局部展开判断，但这仍是 runtime snapshot metadata，不是 parser semantic fact。REPL 的 `:type <expression>` 已是第一个命令行事实消费者：它直接调用 parser/type inference，并能在同一 REPL 会话中复用 prior declaration 源码上下文来输出同一 `SZrSemanticContext` 里的数值/逻辑/所有权事实、调用/成员 expression payload、member-access reference facts 和 member-write reference facts。完整 Debug parser fact display、长期 REPL runtime cell 作用域和跨工具共享查询 parity 仍在后续任务中接入。

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

Debug safe evaluate 的数值语义错误也开始走 richer diagnostic 文本：非数值 operand 参与数值运算、除零、取模零值或非整数取模、unary `-` 作用在非数值上时，错误会说明实际 operand 类型、具体原因和建议动作。成员/索引/括号/条件结构的语法错误也不再停留在 `expected member name`、`missing ']'`、`missing ')'` 或 generic `expected expression` 这类短句，`true.`、`true[0`、`(true || false` 和缺少 `:` 的条件表达式会说明缺少的 token、错误原因和下一步修复建议。这仍然是调试时的只读安全求值器，不是新的完整编译期语义事实层；后续如果要把 Debug 与 parser shared local query 完整打通，应继续复用 `SZrSemanticContext` 的事实，而不是在 Debug 内部复制一套类型推断。当前 Debug 代码已经把 safe-evaluate diagnostics/right-operand helper 抽到 `debug_eval_diagnostics.c` / `debug_eval_internal.h`，后续应继续保持这种窄模块边界。

## REPL Type Query Consumer

`zr_vm_cli/src/zr_vm_cli/repl/repl.c` 的 `:type <expression>` 命令是 REPL 的第一条共享事实消费路径。它不会执行目标表达式，而是创建 fresh analysis state，将当前 REPL 会话中已成功提交的 declaration-style 源码和参数表达式一起解析，把 prior variable declarations 注册进 parser type environment，调用 `ZrParser_ExpressionType_Infer` 推断最后一个 expression statement，然后读取同一个 `compilerState.semanticContext`。

当前输出范围保持窄而可验证：

- 总是输出 parser type formatter 给出的 `Type: ...`。
- 若当前表达式节点有 numeric fact，则输出 exact range、unsigned range 或 `may overflow` 提示。
- 若当前表达式节点有 logical fact，则输出已知布尔值或短路说明。`true || false` 当前会输出 `Type: bool`、`Logical value: true` 和 `Logical flow: short-circuits right operand`，这些信息来自 parser/type inference 写入的同一个 `SZrSemanticContext`。
- 若当前表达式或其子表达式范围有 reachability fact，则输出不可达原因。`true || false` 的 `false` operand 会通过 parser/type inference 写入 `ZR_SEMANTIC_REACHABILITY_SHORT_CIRCUIT`，`:type true || false` 因此会额外输出 `Reachability: unreachable because short-circuit skips evaluation`。REPL 只遍历表达式树并按 range 查询 `SZrSemanticContext`，不复制短路判断。
- 若当前表达式节点有 ownership fact，则输出 ownership action 和 qualifier，或显示 violation 文本。`%borrow(owner)` 当前会输出 `Type: %borrowed int` 和 `Ownership: borrow %borrowed`，这些信息来自 `infer_construct_expression_type` 成功路径记录的 ownership builtin fact。
- 若当前表达式节点有 call/member payload，则输出共享 expression fact 里的 token 信息。`pick(42)` 会显示 `Call: pick args=1`；`seed.value` 会显示 `Member: value`。REPL 不解析调用文本来重建这些字段，而是读取 `SZrSemanticExpressionFact.hasCallInfo/hasMemberInfo`。
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

parser 现在保留旧 `TZrParserErrorCallback`，同时在 `SZrParserState` 上新增可选 `TZrParserStructuredErrorCallback`。旧回调仍接收 `range/message/token`，结构化回调接收 `SZrStructuredDiagnostic`，包含 stable code、message、cause 和 suggestion。`report_structured_parser_error` 会先调用结构化回调，再调用旧字符串回调，保证旧调用方不需要迁移。

当前代表性语法诊断包括：

- `missing_expression_after_assignment`: `var x = ;` 或 `var x = <eos>` 在 `=` 后直接遇到语句终止符时产生。message 是 `Missing expression after '='`，cause 说明初始化已开始但没有值表达式，suggestion 建议在 `;` 前添加表达式或移除 `=` initializer。
- `missing_right_operand`: 二元、逻辑或赋值操作符右侧直接遇到语句终止符或源码结束时产生，例如 `1 +`、`true &&`、`value = ;`。message 是 `Missing expression after '<op>'`，cause 说明该操作符需要右侧表达式但表达式提前结束，suggestion 建议补上右侧表达式或移除该操作符。

LSP incremental parser 设置结构化回调并把它转换成 `SZrDiagnostic`。同一错误随后触发的旧字符串回调会被 LSP collector 跳过一次，避免用户看到一条新结构化诊断和一条旧 `parser_syntax_error` 重复诊断。最终 LSP wire struct 不新增字段；`lsp_interface_support.c` 会在 `message` 内追加：

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

`ZrLanguageServer_LspLocalSemanticQuery_AppendFactsToHover` 把当前 query 命中的共享事实追加到已有 hover markdown 上。这个 API 让传统 symbol hover 保留原本的变量/类型/文档内容，同时展示局部 reference fact，例如 `Reference: read` / `Reference: write` 和声明 token 的 `Declared at: line:column`。局部 reachability fact 也会携带具体原因，例如 `Reachability: unreachable after return`、`unreachable after throw` 或 `because short-circuit logic skips it`，而不是只展示一个不可达布尔状态。ownership 违规事实会把状态和诊断 message 保持在同一 `Ownership:` 行，例如 `Ownership: violation - Loaned value cannot escape its owner`，这样 plain hover 可读，rich hover 也能把完整所有权原因放进稳定 `ownership` role。调用/成员 payload 也在同一 formatter 中追加：`pick(42)` 的 call target token 会显示 `Call: pick args=1`，rich hover role 为 `call`；`seed.value` 的 member token 会显示 `Member: value`，rich hover role 为 `member`。事实格式化留在 `lsp_local_semantic_query.c`，`lsp_interface.c` 只负责在 hover 编排路径上调用它，避免把本地事实展示规则继续堆进已经很大的接口文件。

`lsp_inlay_hints.c` 现在单独拥有 LSP inlay hint 生成，不再继续把 hint 逻辑堆在 `lsp_interface.c` 中。它保持原来的触发边界：只为已有的未显式注解 local、field 或无返回注解函数生成类型 hint，不会给每个子表达式额外插入提示。对于 unannotated local，hint 会查看 initializer 对应的共享 facts；如果 initializer 已有 numeric/logical fact，label 会在类型后追加紧凑语义，例如 `: int, range 3..3` 或短路/逻辑值提示。这样 VSCode 可以在最常见的局部推断位置直接看到 parser/type inference 已证明的事实，同时仍复用 `SZrSemanticContext`，不在 LSP 里复制数值或逻辑推断。

`lsp_completion_semantic_facts.c` 现在单独拥有 completion item 的共享事实追加逻辑。`ZrLanguageServer_Lsp_EnrichCompletionItemMetadata` 仍负责原来的 symbol detail、documentation 和 resolved-type 补充；找到本地变量 symbol 后，它委托 `ZrLanguageServer_Lsp_EnrichCompletionItemSemanticFacts` 查看变量 initializer 的 parser facts。若 initializer 还没有 materialized expression fact，该 helper 通过 `ZrLanguageServer_SemanticAnalyzer_InferExactExpressionType` 触发同一条 type-inference/fact emission 路径，然后读取同一个 `SZrSemanticContext` 的 numeric/logical/ownership facts。当前已验证的 completion detail 追加包括 `Semantic facts: range 3..3`、`Semantic facts: logical true, short-circuits`，以及 `Semantic facts: ownership violation: Owned value cannot flow into a plain GC value implicitly`。这让 VSCode 补全面板也能消费局部推断事实和所有权诊断原因，同时避免把事实格式化继续堆进已经很大的 `lsp_interface_support.c`。

`lsp_signature_semantic_facts.c` 现在单独拥有 signature help 参数文档的共享事实格式化。`signature help` 保留原有声明参数 label，但会在对应参数 documentation 上追加 `Argument semantic facts: ...`，内容来自实际 argument 的 numeric/logical/ownership facts。若 overload compatibility 因语义违规 argument 被拒绝，`lsp_signature_help.c` 会回退到唯一同名且同 arity 的声明候选，让 VSCode 仍能显示声明签名和局部 ownership/numeric/logical facts，而不是返回 no signature help。这个回退只服务 LSP 展示健壮性；编译器/semantic analyzer 的诊断仍按原规则产生，不会把非法调用视为类型兼容。

`stdio_inline_value.c` 现在也通过 `ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt` 消费同一局部事实层。`textDocument/inlineValue` 保留原来的 `InlineValueVariableLookup`，用于调试器按变量名取运行时值；如果 local initializer 在当前 `SZrSemanticContext` 中有 numeric/logical fact，则额外在变量名范围上返回 `InlineValueText`。initializer 可以和 `=` 同行，也可以从下一行开始；两种情况下 semantic text 都挂在变量名范围上，例如 `var sum =\n 1 + 2;` 对 `sum` 同时返回 variable lookup 和 `range 3..3`。`return` 表达式和缩进后位于行首的简单 expression statement 现在也走同一查询入口：stdio 只定位表达式范围并优先查询 operator 或 payload token，事实仍由 parser/type inference 和 local semantic query 提供，例如 `return 1 + 2;`、`return 1 +\n 2;`、`return\n 1 + 2;` 或 `1 + 2;` 在表达式范围上返回 `range 3..3`，`true || false;` 在完整 logical expression 范围上返回 `logical true, short-circuits`，`var seed = 2; seed + 3;` 会在 `seed + 3` 范围上返回 `range 5..5`。当 editor 只请求 operator-split expression statement 的 continuation line，例如 request range 从 `1 +\n 2;` 的第二行开始时，stdio 会回溯到 owner expression statement，仍返回覆盖完整表达式的 fact-backed inline value，而 request range 同时包含 owner line 时不重复发射。Unary 起始的 expression statement 也复用这条路径：`!true;` 在 unary expression 范围上返回 `logical false`，`-42;` 在 unary numeric expression 范围上返回 `range -42..-42`。Call/member 起始的 expression statement 只格式化已有 expression payload：`pick(42);` 在 call expression 范围上返回 `call pick args=1`，`seed.value;` 在 member expression 范围上返回 `member value`。数值 fact 目前格式化为 `range min..max`、unsigned range 或 `may overflow`；逻辑 fact 格式化为 `logical true/false` 和 `short-circuits`；call/member payload 格式化为 `call <target> args=N` 和 `member <name>`。逻辑 initializer、return expression 和 supported expression statement 会查询 `&&` / `||` operator 位置来命中已有的结构化 logical expression fact，member statement 会查询 `.` 后的成员 token，call statement 使用表达式起点的 call target token，避免在 stdio 层复制短路、unary、数值或 call/member 推断。当前 expression-statement 扫描保持保守：只覆盖缩进后行首、分号结束的数字/括号/boolean literal、identifier、`!` 或 `-` 起始形式；continuation owner 回溯只覆盖前一行以 expression continuation operator 结尾的简单 statement；call/member inline text 必须来自共享 `SZrSemanticExpressionFact`，stdio 不会自己解析源码来合成 payload。

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

`zr_vm_language_server_local_semantic_hover_test` 覆盖：

- `LSP Local Expression Hover Surfaces Assignment Write Reference Fact` 验证 `var seed = 1; seed = 3;` 的 assignment target token 通过 `ExpressionAt` 返回 `ZR_SEMANTIC_REFERENCE_WRITE`，plain hover 输出 `Reference: write` / `Symbol: seed` / `Declared at: 1:5`，rich hover 同步暴露 `reference`、`symbol` 和 `declaration` roles。
- `LSP Local Hover Surfaces Call/Member Payloads` 先确认 `ExpressionAt` 在 `pick(42)` 的 call target token 和 `seed.value` 的 member token 上命中带 payload 的 expression fact，再验证 plain hover 输出 `Call: pick args=1` / `Member: value`，rich hover 暴露 `call` / `member` roles。

`zr_vm_language_server_local_semantic_query_test` 覆盖：

- `LSP Local Expression Query Returns Numeric Range Fact` 验证 `1 + 2` 的 `+` operator 位置命中二元表达式 fact，并返回 exact numeric promotion range `3..3`。
- `LSP Local Expression Query Returns Numeric Overflow Fact` 验证 `9223372036854775807 + 1` 的 `+` operator 位置返回 overflow numeric fact，不伪造 range。
- `LSP Local Expression Query Returns Logical Short Circuit Fact` 验证 `true || false` 的 `||` operator 位置返回 exact `bool` expression fact、short-circuit logical fact 和 short-circuit reachability fact。
- `LSP Local Reference Query Returns Member Access Fact` 验证 `seed.value` 的 `value` token 位置返回 unresolved `ZR_SEMANTIC_REFERENCE_MEMBER_ACCESS`，而 assignment target 上的 `value` 仍由 member-write fact 优先。

`zr_vm_language_server_inlay_semantic_facts_test` 覆盖：

- `LSP Inlay Hint Uses Initializer Numeric Fact` 先以 RED 确认原 inlay label 只有 `: int`，随后验证 `var sum = 1 + 2;` 的未注解 local hint 会复用 initializer numeric fact，输出 `: int, range 3..3`。
- `LSP Completion Detail Uses Initializer Numeric Fact` 先以 RED 确认 `sum` completion detail 只有 `private int`，随后验证补全 detail 复用 initializer numeric fact 并包含 `range 3..3`。
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
- `textDocument/inlineValue` 当 request range 从 operator-split expression statement 的 continuation line 开始时，仍回溯到 owner statement 并返回完整表达式范围上的 semantic fact，例如 `1 +\n 2;` 的第二行请求返回 `range 3..3`，range 覆盖 `1 +` 到 `2`。

`tests/cli/repl_type_reachability_smoke.js` 覆盖：

- `:type true || false` 输出 `Type: bool`、`Logical flow: short-circuits right operand` 和 `Reachability: unreachable because short-circuit skips evaluation`。
- 该 smoke 不执行目标表达式，证明 REPL 通过 parser/type inference 写入的 shared logical/reachability facts 显示短路语义，而不是把 `true` 作为运行时结果打印。

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
