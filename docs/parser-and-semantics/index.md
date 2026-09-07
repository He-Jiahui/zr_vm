---
related_code:
  - scripts/syntax_migration_inventory.py
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/cfg.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_definite_assignment.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/semantic_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/semantic_reaching_definitions.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/attribute_contract.h
  - zr_vm_parser/include/zr_vm_parser/compile_tool.h
  - zr_vm_parser/include/zr_vm_parser/comptime_contract.h
  - zr_vm_parser/include/zr_vm_parser/declaration_transform_contract.h
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_flow.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_flow_internal.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_liveness.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_conflicts.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_format.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_index.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_adapter.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_definition.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_format.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_control_flow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_loops.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_argument_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_query_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_assembly.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape_statements.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape_internal.h
  - zr_vm_parser/src/zr_vm_parser/attribute_contract.c
  - zr_vm_parser/src/zr_vm_parser/comptime_contract.c
  - zr_vm_parser/src/zr_vm_parser/declaration_transform_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_evaluator.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_attribute_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_declaration_transform.c
  - zr_vm_parser/src/zr_vm_parser/compiler/comptime_runtime_contract.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_binary_metadata_coordinates.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_descriptor_metadata_coordinates.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_definition_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_definition_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_relation_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_relation_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_analysis.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_constant_condition.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_reachability.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_expression_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_references.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
implementation_files:
  - scripts/syntax_migration_inventory.py
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/cfg.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_flow.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_flow_internal.h
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_liveness.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_conflicts.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_format.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_index.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_adapter.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_definition.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_format.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_control_flow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_loops.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_definite_assignment.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/semantic_definite_assignment.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/semantic_reaching_definitions.c
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_argument_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_query_diagnostics.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_assembly.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape_statements.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_reference_escape_internal.h
  - zr_vm_parser/src/zr_vm_parser/attribute_contract.c
  - zr_vm_parser/src/zr_vm_parser/comptime_contract.c
  - zr_vm_parser/src/zr_vm_parser/declaration_transform_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_evaluator.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_attribute_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_declaration_transform.c
  - zr_vm_parser/src/zr_vm_parser/compiler/comptime_runtime_contract.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_binary_metadata_coordinates.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_descriptor_metadata_coordinates.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_analysis.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_query_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_relation_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_relation_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_constant_condition.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_reachability.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_expression_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_references.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c
plan_sources:
  - docs/plans/syntax/2026-07-18-06-percent-migration-lsp-fixtures-design.md
  - docs/plans/syntax/06-percent-migration-lsp-fixtures/m1-migration-inventory-implementation-plan.md
  - user: 2026-03-28 实现“ZR 全目标回归强化与 Field-Scoped using 语义计划”
  - user: 2026-04-04 实现“ZR LSP 语义内核与元信息推断增强计划”
  - user: 2026-04-04 拆分边界“final function assembly + invariant validation”独立出去
  - user: 2026-04-08 Rust-First Ownership / GC 分层设计
  - user: 2026-05-16 Rust-First using / Ownership 语义收敛计划
  - .codex/plans/ZR 全目标回归强化与 Field-Scoped using 语义计划.md
  - .codex/plans/Rust-First Ownership  GC 分层设计.md
  - .codex/plans/Rust-First using  Ownership 语义收敛计划.md
  - docs/superpowers/specs/2026-06-03-zr-vm-semantic-inference-design.md
  - docs/superpowers/plans/2026-06-03-zr-vm-semantic-inference-fact-layer.md
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/03-lsp-robustness-and-position.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/scripts/test_syntax_migration_inventory.py
  - tests/parser/test_cfg_reachability.c
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_reference_loan_nll.c
  - tests/parser/test_reference_escape_closure_suspension.c
  - tests/parser/test_resource_owner_borrow_receiver.c
  - tests/parser/test_canonical_type_graph.c
  - tests/parser/test_cfg_constant_conditions.c
  - tests/parser/test_cfg_switch_constants.c
  - tests/parser/test_cfg_finally_abrupt.c
  - tests/parser/test_dataflow_engine.c
  - tests/parser/test_compiler_semantic_query_diagnostics.c
  - tests/parser/test_semantic_query.c
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_query_call_argument_mapping_cases.h
  - tests/parser/test_semantic_facts.c
  - tests/parser/test_type_inference.c
  - tests/parser/test_parser.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_local_semantic_hover.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_reaching_definition_navigation.c
  - tests/language_server/stdio_smoke.js
  - tests/acceptance/2026-06-03-semantic-inference-fact-layer.md
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/test_lsp_project_utf16_ranges.c
  - tests/language_server/test_lsp_source_contracts.c
  - tests/parser/test_compiler_features.c
  - tests/compileTime/test_attribute_contract.c
  - tests/compileTime/test_comptime_contract.c
  - tests/compileTime/test_comptime_runtime_contract.c
  - tests/compileTime/test_declaration_transform_contract.c
  - tests/module/test_module_system.c
  - tests/parser/test_union.c
  - tests/acceptance/2026-06-17-union-types.md
  - tests/acceptance/2026-06-20-semantic-stage1-cfg.md
  - tests/acceptance/2026-06-20-semantic-stage1-dataflow.md
  - tests/acceptance/2026-06-20-semantic-stage1-semantic-query.md
  - tests/acceptance/2026-07-19-syntax-01-m1-canonical-type-graph.md
  - tests/acceptance/2026-07-19-syntax-01-m3-pre-semantic-ir.md
  - tests/acceptance/2026-07-20-syntax-02-m3-reference-loan-nll.md
  - tests/acceptance/2026-07-20-syntax-02-m4-receiver-readonly-call-boundary.md
  - tests/acceptance/2026-07-20-syntax-02-m5-reference-escape-closure-suspension.md
doc_type: category-index
---

# Parser And Semantics

本目录记录 parser、semantic analyzer 和编译期前端共享的语言行为。

## Shared Semantic Facts

parser semantic context 现在持有表达式、引用、数值、可达性、逻辑和所有权事实数组。type inference 和 semantic analyzer 在同一次分析中追加事实，LSP 局部查询消费这些事实，并在语法不完整时返回精确事实、诊断阻断结果或显式 unknown。

二元 `+` 在类型推断中保留字符串拼接语义：任一操作数已知为 `string` 时，表达式结果推断为 `string`，不再先尝试普通数值 common type。这样 native/project helper 中的 `path + "/" + name`、`"checksum=" + value` 等拼接不会被误降级为 `object`，后续 `writeText(path, text: string)` 这类签名检查也能保持精确。

CFG/dataflow 现在已开始给引用事实补充控制流敏感 payload：definite-assignment 可在分支汇合处标记 `UNINIT` / `MAYBE_INIT` / `INIT`，也能让声明初始化器内的自读在声明完成前保持 `UNINIT`；reaching-definitions 可在单一定义到达时保留 `definitionRange`，并在不同分支写入不同 token 时清掉单一跳转目标，让 LSP 定义查询保守回退到声明。

诊断应包含 stable code、具体原因和建议动作。parser 仍保留 legacy error callback，但当 LSP 或工具可消费结构化诊断时，优先使用 structured diagnostic，再通过标准 LSP message 展示 cause 和 suggestion。

语义诊断的生产边界已经收敛到 parser/compiler persistent facts：LSP semantic analyzer
只能触发 parser/compiler producer、物化 `SemanticQuery_Diagnostics` 并调用统一 structured
projector。analyzer rule 源文件不得直接构造 LSP diagnostic、调用 parser diagnostic builder，
也不得自行追加 semantic diagnostic fact；source contract 会阻止这些入口重新出现。

## 当前主题

- `semantic-query-api-foundation.md`
  - imported members without source ranges retain parser-owned external target identity
  - LSP navigation and semantic tokens require exact metadata token/hash/kind agreement
  - provider generation zero remains explicitly unavailable and is never inferred
  - `ExternalReferences` exposes stable complete external tuples and omits incomplete facts
  - project references resolve exact metadata rows across snapshots in both declaration-to-use and use-to-use directions
  - import alias definitions consume exact import-origin relations and metadata-projected virtual URIs without request-time AST bindings
- `external-callable-value-canonical-facts.md`
  - canonical TypeId/signature facts for binary and provider callable values
  - unresolved source identity and exact fact-owned LSP fail-closed behavior
- `callable-value-shadow-resolution.md`
  - lexical callable aliases and lambdas retain canonical function metadata
  - same-scope values shadow ordinary functions without hiding their own callable contract
  - source signature help consumes `CallAt/FormatCall` and has no compiler/name fallback
- `canonical-signature-help-consumption.md`
  - source direct and receiver calls consume canonical `CallAt/FormatCall` facts
  - binary/native callables use the external canonical adapter with provider identity
  - request-time receiver inference, member-name search, and AST signature reconstruction are removed
- `ast-and-syntax-contracts.md`
  - contextual unified property grammar across class, struct, resource class, and interface
  - one `PROPERTY_DECLARATION` with ordered accessor children and exact recovery ranges
  - canonical `let`/`var` local and explicit-field bindings with stable lexer ids
  - properties never synthesize or infer backing fields
  - legacy property AST retained only for numeric compatibility and migration rejection
- `type-inference.md`
  - one visible PropertySymbol with linked accessor SymbolIds and canonical property TypeId
  - exact getter/setter/init role selection and receiver-effect contracts
  - structured constructor/init-accessor phase and exactly-once immutable-field initialization
  - explicit fields alone own TypeLayout, reflection field rows, and initialization bitmap positions
  - structured property serialization/reflection with legacy artifact reader fallback
  - typed getter/setter/init lowering through the linked accessor identity
  - single-evaluation compound assignment with receiver/RHS/exception ordering
  - inline-struct receiver-source provenance and source/artifact dispatch parity
  - ref/ref-readonly getter invariants, `PROPERTY_REF_GET`/deref Place lowering, and the managed
    reference artifact/AOT boundary
  - canonical PropertyQuery, exact binary prototype-row joins, structured legacy migration, and
    declaration-contract incremental invalidation without member-name reconstruction

- `canonical-binding-injection.md`
  - temporary type-environment bindings retain externally verified SymbolId, TypeId, and declaration range
  - ordinary identifier inference publishes the supplied canonical reference identity without allocating a replacement
  - Debug/REPL keeps frame PlaceId in its validated runtime context and fails closed for unavailable bindings

- `repl-closure-submissions.md`
  - generation-checked closure environments replace source replay across REPL cells
  - submission bindings preserve canonical SymbolId, TypeId, PlaceId, declaration range, and callable signatures
  - ref-like, borrowed, loaned, and invalid identity values fail closed at the cell boundary

- `canonical-type-graph.md`
  - immutable structural type nodes and canonical `TypeId` identity
  - hash-indexed interning and binary `TypeId` lookup
  - legacy inferred-type, callable-contract, value-construction, and LSP projections
- `place-cfg-graph.md`
  - stable session-local Place identity and projection paths
  - dynamic typed CFG edges, cleanup routing, and suspension topology
- `async-task-syntax-and-effect.md`
  - explicit `async fn ...: zr.task.Task<T>` source contract
  - role-based direct `await` payload inference and suspend/resume CFG topology
  - Syntax 12 M6.2 rejects legacy `%async`/`%await` compatibility input
- `reference-place-out-flow.md`
  - exact ref/out call markers and Place-only arguments
  - field-sensitive out initialization across normal and exceptional flow
  - named argument contract mapping and cross-call out responsibility transfer
- `reference-loan-nll.md`
  - CFG backward LoanId liveness and last-use region contraction
  - ref-slot reaching values with Store kill/gen and Load propagation
  - capability-aware shared/mutable conflict and multi-parent nested reborrow
  - cycle/reachability guards and structured overlap diagnostics
  - property-produced receiver/owner loan propagation and the pre-creation ValueId reuse guard
- `receiver-readonly-call-boundary.md`
  - canonical readonly/writable receiver effects and serialized member contracts
  - owner auto-deref capability matrix across seven dispatch kinds
  - two-phase receiver reservation, activation and call-scoped loan facts
  - resolved target SymbolId/declaration-range boundary for semantic/LSP consumers
- `reference-escape-closure-suspension.md`
  - local/function/caller/heap-static escape facts with conservative unknown
  - ref return, storage, lambda/local-function capture and writable-capture NLL
  - await/yield suspension rejection and call-scoped native ref defaults
- `pre-semantic-ir-flow.md`
  - pre-execution semantic instructions with owned Place/CFG/Value/loan state
  - compiler ordering and the execution SemIR compatibility boundary
  - separate initialization, availability, borrowing, escape, and reachability joins
- `iterator-yield-suspension.md`
  - `yield expression;` as a normal `FunctionDefinition` statement
  - explicit canonical `zr.iteration.Iterator<T>` carrier and element contract
  - `YIELD_VALUE` / `YIELD_SUSPEND` / `YIELD_RESUME` / `ITERATOR_COMPLETE`
    pre-SemIR facts without an iterator runtime frame
- `ffi-extern-declarations.md`
  - `native extern("lib") decl` 与 `native extern("lib") { decls }` 源级 FFI 语法
  - extern function / struct / enum / delegate 的 declaration metadata 和 lowering 规则
  - `compileTimeTypeEnv` 与真正 compile-time callable 的边界
- `dynamic-iteration-semir-execbc-aot.md`
  - dynamic foreach 在 `SemIR -> ExecBC -> AOT` 三层中的职责边界
  - `DYN_ITER_INIT` / `DYN_ITER_MOVE_NEXT` 的稳定 runtime contract
  - `SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE` 的 quickening 约束
- `dynamic-meta-tail-call-semir-execbc-aot.md`
  - `META_CALL`、`DYN_TAIL_CALL`、`META_TAIL_CALL` 的稳定语义边界
  - tail-call site 上 dynamic/meta dispatch 不再退化成普通 `FUNCTION_TAIL_CALL`
  - 安全条件下的真实 `callInfo` frame reuse 与异常 / `using` 回退边界
  - AOT backend 继续只依赖共享的 `FUNCTION_PRECALL` runtime contract
- `call-site-quickening-meta-access-semir-aot.md`
  - 零参数 call-site superinstruction、cached `META_CALL/DYN_CALL`、以及 cached tail-call 的 ExecBC quickening 边界
  - property getter/setter 现在直接落成 ExecBC `META_GET` / `META_SET`，并保持同名 `SemIR` / AOT 契约
  - meta access 与 cached dynamic/meta call site 现在都带显式 `CALLSITE_CACHE_TABLE`，并使用固定 2-slot PIC
  - child function 的 `.zri`/AOT metadata 会递归输出，constant function/closure 会重绑到 quickened child function tree
  - quickened ExecBC 与稳定语义层继续解耦
- `ownership-builtins-semir-aot.md`
  - `ref` / `ref readonly` 与五个 ownership intrinsic 的 ExecBC、SemIR、AOT 契约
  - ownership expression 与 statement `using` 的边界
  - 旧 ownership helper 不进入现行 artifact surface
- `ownership-intrinsics-and-receiver-guards.md`
  - `share/degrade/wake/intoGc/drop` 的 reserved intrinsic、Place/Loan/effect facts
  - `.` / `?.` 的 nullable/Weak target guard、单次 wake、suffix skip 与 `NullReferenceError`
  - interpreter、AOT、LSP、structured migration fix 和性能边界
- `csharp-value-type-semir-aot.md`
  - C#-style `struct` value-place SemIR contract
  - inline struct field address/load/store and by-value copy metadata
  - AOT ExecIR runtime-contract boundary for typed value operations
- `aot-function-reachability-manifest.md`
  - AOT function reachability root/edge reason schema and fail-closed graph validation
  - deterministic retained-function manifest and predecessor-chain invariants
- `aot-type-layout-reachability-manifest.md`
  - AOT retained-layout frame edges and reflection-annotation roots
  - deterministic type-layout manifest, root precedence, count parity, and unresolved-layout rejection
- `compiler-final-function-assembly.md`
  - `compiler.c` 只保留 orchestration，最终 `SZrFunction` 装配沉到 `compiler_function_assembly.c`
  - script wrapper / top-level function declaration 共用同一套 final assembly 逻辑
  - `CREATE_CLOSURE -> childFunctions` child graph 不变量在装配期统一校验
- `owned-field-lifecycle.md`
  - direct `Unique<T>` / `Shared<T>` field 的字段生命周期语义
  - field-scoped `using` 的生命周期边界
  - owner 值跨入 plain GC world 必须显式 `intoGc(owner)` 或 bridge
- `resource-unique-drop.md`
  - `resource class`、`own T(...)`、`drop(owner)` 的 type-directed 生命周期合同
  - direct `Unique<T>` 无 control block 的 move、partial construction 与逆序 Drop
  - VM/AOT cleanup 顺序、M4 explicit domain roots 与 `intoGc(owner)` bridge
- `resource-shared-weak.md`
  - process-local non-atomic `Shared<T>` / stable `Weak<T>` control lifetime
  - last-strong Drop、implicit weak、last-strong-drop 后的 wake failure 与 cleanup mirror 同步
  - structured `resource_shared_strong_cycle` lint，以及 final Option surface 的明确边界
  - `Weak<T>` 通过 `wake(weak)` 显式留存，或用 `.` / `?.` 执行 direct/optional target access
  - cleanup plan 与 prototype metadata 的传播路径
- `resource-owner-borrow-receiver.md`
  - `Unique<T>` mutable/shared 与 `Shared<T>` readonly receiver capability
  - `in T` owner reborrow、two-phase receiver loan 与 last-use NLL
  - active ref 对 drop/share/move 的 canonical Place/LoanId 冲突门禁
  - receiver-tied ref return provenance 与 direct source TypeDef 保守边界
  - `intoGc(owner)` / `INTO_GC_BOX` 对同一 Place/LoanId exclusive-consumption facts 的复用
- `canonical-receiver-call-diagnostics.md`
  - receiver method overload 与参数失配只由 parser/type inference 解析和发布
  - descriptor 2011、argument/parameter ranges、related information 与 typed fix 共用 canonical producer
  - LSP 删除 method-name/parameter/type-text 重建，仅投影 semantic query fact
- `canonical-call-argument-mapping.md`
  - source call argument-to-parameter dense mapping 与 snapshot borrowed ownership
  - named binding、canonical TypeId、passing mode、exact/implicit conversion
  - `ref`/`out` structured marker range 与 argument AST range合并，不扫描source text
  - malformed mapping按selected callable contract fail closed
- `semantic-scope-fact-ownership.md`
  - source scope 和 visible-symbol fact 的 type/method owner 使用同一 snapshot 的 canonical SymbolId
  - generic parameter 发布可能扩容 symbol array，visitor 在发布前保存 owner ID，避免借用 record 失效
  - class/struct/interface 的 generic type 与 method 均有强制搬迁分配器回归
- `semantic-call-fact-ownership.md`
  - call signature interning 前保存 declared-function validity、receiver 与 effects
  - canonical type array 搬迁后仅通过稳定 TypeId 重绑定声明契约
  - 强制搬迁回归验证 callable identity、return substitution 与 readonly contract
- `semantic-assignment-fact-ownership.md`
  - parser 完整赋值推断统一产生左值 Write、右值 Read 和详细兼容性诊断
  - diagnostic source/expected ranges 来自右值与 canonical binding declaration
  - LSP 只投影事实，覆盖失败赋值及文档更新后的高亮角色
- `lsp-typecheck-canonical-binding.md`
  - LSP typecheck 通过 identifier pattern range 命中 source-local canonical symbol
  - inferred binding 复用 SymbolId/TypeId 与 declaration range，避免重复 semantic records
  - analyzer identity、structured local query 和 hover 的 exactness/lifetime 边界
- `semantic-fact-layer.md`
  - `SZrSemanticContext` 统一持有表达式、引用、数值、可达性、逻辑和所有权事实
  - 事实层提供 append-by-copy、reset/free 和按节点/位置查询契约
  - type inference 已开始写入字面量和二元数值表达式事实；数值分支层已覆盖 simple true/false branch refinement、segment payload 展示，以及 simple if/else 同名数值赋值的 post-branch range join
  - LSP semantic analyzer 已开始写入声明和使用引用事实，并保留完整 token 范围
  - reference facts 已携带 `definitionRange` / `hasDefinitionRange`，直线 reaching-defs 后处理可把 read 指向最近 declaration/write，`DefinitionOf` 已优先消费该载荷返回到达的 write fact
  - reference facts 已有 optional definite-assignment 状态载荷，identifier write fact 会预填 `INIT`，直线 definite-assignment 后处理可让 read 继承最近 declaration/write 的状态，CFG-backed definite-assignment resolver 也已能在分支汇合处把 source read 标成 `MAYBE_INIT`，让 `var seed = seed` 这类 initializer read 在声明完成前保持 `UNINIT`，并在同一 source `finally` body 被克隆到多条 CFG path 时合并 read 状态；read fact 上的 `UNINIT` / `MAYBE_INIT` 可被 semantic query 转成未初始化读取诊断
  - semantic analyzer 已在不可达语句、常量分支和确定性短路处写入 reachability/logical facts
  - semantic analyzer 已能从同作用域 `var const flag = true/false` 的布尔初始化推断局部分支不可达、常量真分支退出后的后续不可达，并通过 LSP hover/rich hover 暴露具体原因
  - semantic analyzer 已为代表性所有权违规写入 ownership facts，并用 stable code/cause/suggestion 替代泛化 `type_mismatch`
  - parser 已为代表性语法错误提供结构化诊断，LSP 会展示 stable code、cause 和 suggestion
  - LSP 局部语义查询已返回 fact、diagnostic-backed failure 或 explicit unknown，并让 hover 避免在语法阻断位置继续误导性推断
  - Debug、REPL 后续消费共享事实，避免重复局部推断
  - `cfg-reachability-foundation.md`
  - parser CFG scaffold 构建 entry/statement/exit block graph
  - 顺序语句可达性传播覆盖 `return`/`throw`/`break`/`continue` 后的不可达事实
  - `if` statement 已建立 then/else/join 分支图，并覆盖 bool literal、unary `!`、logical `&&`/`||`、短路决定项以及 integer/string/char/float literal 比较条件折叠后的不可达事实
  - `switch` statement 已建立第一版 case body 图，并覆盖 case body 内 terminator 后不可达；bool/integer/string/char/float 常量 selector 会剪掉不匹配的同类常量 case，其中 bool selector/case 可复用 unary/logical 折叠，匹配常量 case 会让后续 default/无命中路径被标为常量分支不可达
  - CFG 构图实现已拆分出 `cfg_control_flow.c`、`cfg_loops.c` 和 `cfg_internal.h`，避免 `cfg.c` 接近 1000 行后继续堆叠控制结构逻辑
  - `while` statement 已建立 condition/body/back-edge/join 循环图，覆盖 `while(false)` 与 folded-false 条件循环体不可达，并把 body 内 `break` 接到 join、`continue` 接回 condition
  - 传统 `for` statement 已建立 init/condition/body/step/back-edge/join 基础图，覆盖 `for(false)` 与 folded-false 条件循环体不可达，并把 body 内 `break` 接到 join、`continue` 接到 step-entry 或 condition
  - `foreach` statement 已建立 entry/body/back-edge/join 基础图，覆盖 body 内 terminator 后不可达，并把 body 内 `break` 接到 join、`continue` 接回 foreach iteration block
  - `try` statement 已建立 try/catch/finally body 图，并覆盖 try/catch/finally body 内 terminator 后不可达；try/catch 内 `return`/`throw`/`break`/`continue` 现在也会进入 finally body，且只有 abrupt completion 时不会让 try 后续语句变可达；单一和 mixed normal/break/continue path 都会按 completion kind 拆分 finally CFG 路径并回到对应目标
  - switch-local break 已审计为当前语言/编译器不支持：编译器只通过 loop label stack 解析 break/continue，switch 不提供 break label
  - 后续需扩展精确 catch 异常匹配/过滤边、union 穷尽分支、区间/符号值驱动等更广泛常量条件折叠和具体 dataflow 分析
- `dataflow-engine-foundation.md`
  - CFG 上的 forward/backward 工作队列执行框架
  - 每个 block 持有 in/out state，analysis 通过 init/join/transfer 回调定义半格语义
  - 当前引擎过滤 entry 不可达块，避免 unreachable 语句污染后向分析，并在首次到达 block 时复制输入状态、后续路径再执行 join
  - definite assignment 已有 `UNINIT` / `INIT` / `MAYBE_INIT` 状态格 helper，覆盖直线路径保持 `INIT` 和分支 join 后变 `MAYBE_INIT`，并已有 CFG-backed resolver 把分支汇合状态、声明初始化器内 self-read 顺序和 cloned-finally read joins 写回 reference read facts
  - reaching-defs 已有直线 reference fact 载荷和首版 CFG-backed branch-join producer；不同分支写入不同 token 时会清掉单一 `definitionRange`，loop fixed point / 多定义展示仍待后续具体分析
- [semantic-type-use-publication.md](semantic-type-use-publication.md)
  - Shared analysis-time type-use publication preserves closed TypeId and exact
    declaration SymbolId, including nested generic ranges and failed-conversion atomicity.
- `semantic-query-api-foundation.md`
  - parser 侧公共 semantic query 查询面骨架
  - `TypeAt`、`DefinitionOf`、`FactsAt` 和 `Diagnostics` 的当前语义
  - `DefinitionOf` 已在 read fact 带 `definitionRange` 时优先返回匹配 declaration/write，再回退 declaration 查询
  - LSP definition 已通过 `lsp_semantic_definition_query.c` 消费直线和首版 CFG-backed reaching-defs，使本地 read after write 能跳到到达的 write token，并在 divergent branch writes 后回退到 declaration
  - `Diagnostics` 已把 scope 内不可达 reachability facts 映射为结构化 `unreachable_code` warning，也能把带 definite-assignment 状态的 read fact 映射为 `uninitialized_read` / `possibly_uninitialized_read`，并覆盖直线 resolver 与 CFG-backed resolver 产出的 read 状态
  - LSP semantic analyzer 已在常规语义检查后消费 semantic query diagnostics，作为未被旧 analyzer 诊断覆盖位置的补充诊断源发布到 `GetDiagnostics`
  - LSP snapshot 在 source facts 后发布 import-origin relations；alias definition 只消费 exact SymbolId/TypeId relation 与 metadata URI projection，歧义 origin fail closed
  - compiler frontend 已在 `compile_script` 成功完成语句编译和 typed metadata 后发布 semantic query diagnostics 到 `SZrSemanticContext.queryDiagnostics` cache，warning 级 query diagnostics 不会置位编译错误状态，并已覆盖 CFG-backed 分支汇合 `possibly_uninitialized_read`；外部/二进制诊断序列化仍待后续切片
  - module/node scope 过滤边界，以及后续局部重算和更完整诊断来源
- `lsp-semantic-resolution-and-native-imports.md`
  - `this` / `super` / `comptime` / `#zr.testing.test#` / lambda 的局部符号命中规则
  - reference tracker 的“最窄范围优先”策略
  - `import("zr.math")` 如何在语义分析阶段预热 native metadata，支撑 `init math.Vector3(...).y`
  - imported type 只允许 `module.Type` 或 `var {Type} = import(...)` 两种显式绑定路径
  - nested native module lookup 与 compile-only imported stub 如何避免递归和 runtime prototype 污染
- `lsp-semantic-cache-storage.md`
  - primary/scoped `SZrAnalysisCache` 的 exact capacity-storage accounting
  - cache-only recursive release、scoped analyzer identity preservation 与 analysis-time rehydration
  - workspace LRU、256MiB budget、historical semantic snapshots 与 peak-memory report 的明确后续边界
- `lsp-historical-semantic-snapshots.md`
  - 每 URI 两份完整 historical semantic analyzer state 的 newest-first query
  - primary analyzer identity 保持、历史 AST 单一所有权与 scoped-cache borrowed-AST rollover 失效
  - canonical URI 的递归 project-import dependency fence、transitive invalidation 与共享 snapshot identity
  - historical semantic state 与 cache-storage eviction 的所有权边界
- `lsp-incremental-declaration-reparse.md`
  - `full_reparse`、`token_equivalent` 与 `declaration_reparse` 的显式 mode 合同
  - 唯一 top-level declaration 的严格替换条件，以及 block/边界不确定 edit 的 full fallback
  - declaration identity 变化后的 semantic-cache invalidation 与 canonical public-contract dependency gate
  - 10,000 次 UTF-8/UTF-16 differential、同构 JSON 与 fallback/percentile telemetry acceptance gate
- `lsp-workspace-semantic-cache-lru.md`
  - primary/scoped/history analyzer 的 exact cache-storage workspace LRU
  - 默认 256MiB budget、可配置 hard cap、recency、cache-only release 与公共统计
  - process peak memory 和完整 L6 stress matrix 的明确后续边界
- `lsp-binary-metadata-coordinate-projection.md`
  - binary typed-export 的 one-based byte line/column 与 LSP UTF-16 range 之间的窄转换合同
  - 有 source snapshot 时按 byte offset 精确转换；无 snapshot 时保留 artifact structural coordinates
  - definition/references/documentHighlight 从 source usage 与 `.zro` declaration 双向命中同一 declaration fact
- `lsp-descriptor-metadata-coordinate-projection.md`
  - descriptor-plugin receiver type member fact优先于通用import-chain解释
  - compact synthetic member coordinates仅由descriptor `sourceKind`显式投影
  - completion、definition、references和documentHighlight消费同一member identity
- `union-types.md`
  - Rust-like `union` 声明、unit/tuple/struct variant AST 和泛型声明解析
  - `Shape.Circle(...)` / `Option<int>.Some(...)` 构造器解析、类型推断和 object carrier lowering
  - LSP type prototype / symbol 支持，以及后续 pattern matching、tagged layout、owner drop 分派边界
- `syntax-migration-inventory.md`
  - Syntax 06A 的只读 legacy syntax inventory、确定性 report schema 和显式 exclusion 边界
  - `machineApplicable`、`maybeIncorrect`、`requiresReview`、`blocked` 与 `targetNotPromoted`
    的分类合同，以及下游计划的 promotion gate
- `current-syntax-convergence.md`
  - `README.md` 作为现行语言表面的唯一规范，以及 parser/LSP 对 `module`、`import(...)`、`ref` 的已实现边界
  - 生产 parser 一次性拒绝旧 `%` 关键字，migration frontend 只生成诊断/编辑，普通 `%` 保持取模
- `compile-time-typed-generation.md`
  - `zr.compile` typed descriptor、comptime effect/budget、AttributeUsage 与 immutable declaration Patch 契约
  - 当前 GeneratedField rebind/layout 能力，以及仍阻断 Gate 11 M4/M5 的 typed additions/consumer 缺口
- `legacy-syntax-migration-frontend.md`
  - parser-owned migration plan、词法屏蔽边界和 source-hash/overlap 防御
  - 当前 parser/compiler 已证明的 `resource class` machine edit 与其余 review/gate 边界
  - property producer ownership、06A LSP handoff 和 06B formal diagnostic 责任划分

## 阅读顺序

1. 先看 `ffi-extern-declarations.md`，了解 `native extern` 语法、descriptor schema 和 `zr.ffi` lowering 路径。
2. 再看 `dynamic-iteration-semir-execbc-aot.md`，了解动态迭代在 SemIR、ExecBC quickening 与 AOT 契约之间的边界。
3. 然后看 `dynamic-meta-tail-call-semir-execbc-aot.md`，了解 dynamic/meta 调用在 tail-site 上的稳定语义契约。
4. 再看 `call-site-quickening-meta-access-semir-aot.md`，了解 zero-arg/cached call-site quickening、meta access PIC、以及 child artifact 对齐规则。
5. 再看 `ownership-builtins-semir-aot.md`，了解 ownership contract 与 statement `using` 的稳定语义边界。
6. 再看 `csharp-value-type-semir-aot.md`，了解 C#-style struct value-place SemIR 和 AOT 边界。
7. 再看 `compiler-final-function-assembly.md`，了解 parser orchestration 与 final function assembly 的边界。
8. 再看 `owned-field-lifecycle.md`，了解 direct owner field 的生命周期语义。
9. 再看 `resource-unique-drop.md`，了解 resource/Unique 的确定性构造、move、Drop 与 VM/AOT 边界。
10. 再看 `resource-shared-weak.md`，了解 Shared/Weak stable control、last-strong Drop 与强环 lint。
11. 再看 `resource-owner-borrow-receiver.md`，了解 owner reborrow、receiver capability、
    NLL conflict 与 ref-result provenance。
12. 再看 `semantic-fact-layer.md`，了解 parser 侧共享语义事实容器、LSP 局部查询三态结果和查询契约。
13. 再看 `cfg-reachability-foundation.md`，了解 Stage 1 CFG 可达性事实生产者的当前范围和后续边界。
14. 再看 `dataflow-engine-foundation.md`，了解 Stage 1 通用 dataflow 引擎骨架和当前验证边界。
15. 再看 `semantic-query-api-foundation.md`，了解 Stage 1 公共语义查询面骨架和当前限制。
16. 再看 `union-types.md`，了解 union 前端 slice、构造器 lowering 和后续模式匹配边界。
17. 最后看 `lsp-semantic-resolution-and-native-imports.md`，了解 language server 如何消费 parser/native import metadata 并稳定命中局部语义引用。
18. 接着看 `lsp-binary-metadata-coordinate-projection.md`，了解 binary declaration identity 如何跨 artifact byte coordinates 与 LSP UTF-16 边界保持一致。
19. 再看 `lsp-descriptor-metadata-coordinate-projection.md`，了解 native descriptor type-member identity 如何跨compact coordinates与LSP consumer保持一致。
20. 再看 `place-cfg-graph.md`，了解 session-local Place identity、typed CFG edge 与 cleanup routing。
21. 再看 `pre-semantic-ir-flow.md`，了解前置 Semantic IR、compiler bridge、flow facts 与 execution sidecar 边界。
22. 再看 `reference-loan-nll.md`，了解 LoanId 传播、NLL、reborrow 与 Place overlap 冲突。
23. 再看 `receiver-readonly-call-boundary.md`，了解 receiver effect、owner auto-deref 与
    two-phase method call loan。
24. 再看 `reference-escape-closure-suspension.md`，了解 ref escape lattice、closure capture
    和 suspension 静态边界。
25. 再看 `syntax-migration-inventory.md`，了解 Syntax 06A 对 legacy source、fixture 和文档 snippet
   的只读盘点边界，以及 M2/M3 前的 target promotion gate。
26. 再看 `current-syntax-convergence.md`，了解 README 标准语法、一次性生产切换与 migration-only 边界。
27. 再看 `compile-time-typed-generation.md`，了解 Gate 11 typed descriptor、attribute、comptime 与 declaration Patch 的当前实现和明确缺口。
28. 再看 `legacy-syntax-migration-frontend.md`，了解 M2 parser plan、可发布 edit 和 formal cutover
   前的 LSP 边界。
29. 再看 `lsp-semantic-cache-storage.md`，了解 semantic cache 的计量、释放和重新初始化边界。
30. 再看 `lsp-workspace-semantic-cache-lru.md`，了解 workspace 预算如何只淘汰精确计量的 cache storage。
31. 再看 `lsp-incremental-declaration-reparse.md`，了解声明级替换、明确 fallback 和 public-contract 驱动的下游失效边界。
32. 再看 `canonical-call-argument-mapping.md`，了解 call mapping、passing marker range 与
    snapshot ownership合同。
33. 需要落代码时，再对照 frontmatter 里的 `related_code` 和 `tests` 追踪实现与验证入口。
