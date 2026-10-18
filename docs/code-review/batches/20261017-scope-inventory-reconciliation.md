# 首方范围与内容摘要校正（2026-10-17）

本批校正文件清单的范围及版本状态，供后续逐函数、结构、接口与语句块复审排队。它没有完成新的代码审查单元；既有 coverage 叶表和历史契约原样保留。

## 输入与使用边界

- 基线 main：91fbe908bc6c1585a4af3d1842a32bd1b1bfe2d8。采用当前人工维护的 scripts/code_review_inventory.py 分类规则，并以 Git 已跟踪及未忽略的新文件集合交叉核对。
- 投影前扫描窗口的范围为 4,194 个文件，原清单 4,092 个，新增 102 个；排除第三方、生成产物、锁文件和二进制等既定范围外文件。此数字依赖当前可检索工作树，不能证明所有函数及间接入口已覆盖。
- 该扫描窗口的原工作树清单列为 reviewed 1,849 / pending 2,243。其中 59 个 reviewed 文件内容摘要已变，更正为 pending；102 个缺项一律补为 pending。校正后工作树为 reviewed 1,790 / pending 2,404。reviewed 这里只表示内容摘要匹配既有文件记录，不能替代逐单位契约及验证证据。
- 350 个旧摘要漂移中，本批选择与其他会话清单差异不重叠的 341 条：59 条撤回文件级完成状态、282 条只刷新原 pending 摘要。另 9 条属于其他会话的 51 条未提交清单改动，原始行及行序全部保留。
- 102 个新文件都在扫描时 Macro 实际源码 pin 枚举中。本批只改元数据；其源码写入仍依赖活跃验证会话的明确归属释放。不能根据 SSA、LSP、整理会话的既往释放推断新控制器已结束。

## 生命周期与失败状态

本批源摘要的观察时点为 2026-10-17T19:12:11.638440+00:00，对应投影前逐文件读取的有限窗口，不是源码冻结声明。其他会话后续可修改 pending 文件或新增首方路径；独立复核与提交收据另行记录差量，不将其偷偷合入本批 443 个选中记录，也不给它们已完成或运行信用。清单本批新增和更新的 443 条记录均为 pending，摘要保留为扫描时点证据；后续漂移仍须所属模块复审。若当前已有 reviewed 文件出现新的内容漂移，则不能沿用其完成判断。

摘要变化只撤回文件级完成状态，不删除旧契约和旧问题记录。每条选中记录保留 oldhash 与前次 note，后续模块须复查当前定义、全部可检索调用方、宏/回调/跨语言入口、失败和释放路径，再决定是否恢复 reviewed。新增行中的既有 canonical 映射仅作追溯；它不是新函数或新测试信用。

共享工作树中 51 条外来清单行和正常索引的三项 plan 删除、Cargo.lock 添加均隔离保留；提交仅投影本批单元格与新增键。既有 4,092 行及其顺序不整体重排，新 102 行按路径排序追加。因此现有 inventory check 的严格全表排序/内容比较仍可报告 stale；必须保留真实退出码，不能用局部校正宣称全仓检查通过。

## 模块分布

| 模块 | 撤回 reviewed | 刷新原 pending 摘要 | 新增 pending |
| --- | ---: | ---: | ---: |
| .github | 0 | 1 | 0 |
| scripts | 0 | 1 | 0 |
| tests/CMakeLists.txt | 0 | 1 | 0 |
| tests/cmake | 0 | 22 | 19 |
| tests/compileTime | 0 | 1 | 0 |
| tests/container | 0 | 0 | 1 |
| tests/core | 1 | 6 | 2 |
| tests/exceptions | 0 | 1 | 0 |
| tests/ffi | 0 | 1 | 0 |
| tests/fixtures | 0 | 0 | 1 |
| tests/language_server | 4 | 4 | 5 |
| tests/library | 0 | 1 | 1 |
| tests/parser | 24 | 65 | 47 |
| tests/thread | 0 | 1 | 0 |
| tests/tooling | 0 | 1 | 0 |
| tests/unity_config.h | 1 | 0 | 0 |
| zr_vm_cli | 3 | 2 | 1 |
| zr_vm_common | 1 | 1 | 0 |
| zr_vm_core | 5 | 29 | 4 |
| zr_vm_language_server | 4 | 8 | 0 |
| zr_vm_language_server_extension | 0 | 1 | 0 |
| zr_vm_lib_container | 0 | 1 | 0 |
| zr_vm_lib_debug | 0 | 1 | 0 |
| zr_vm_lib_ffi | 1 | 0 | 0 |
| zr_vm_lib_thread | 0 | 1 | 0 |
| zr_vm_library | 4 | 3 | 0 |
| zr_vm_parser | 11 | 129 | 21 |

## 59 个需重新复审的旧完成文件

下面仅列撤回完成状态的文件，当前完整摘要及旧摘要在 inventory 对应行中保存；不据此判断原注释仍然正确。

| 文件 | 旧摘要前 12 位 | 当前摘要前 12 位 |
| --- | --- | --- |
| tests/core/test_ssa_runtime_objects.c | 07ca1ff58881 | e0c90af8611c |
| tests/language_server/test_ownership_diagnostics.c | 9f48ca58600c | 66dad58dac79 |
| tests/language_server/test_ownership_diagnostics_owner_set_cases.h | ea58822aa156 | 85b1a25c74fd |
| tests/language_server/test_ownership_diagnostics_region_cases.h | dc58460924db | 9fc2368ad986 |
| tests/language_server/test_ownership_diagnostics_weak_receiver_cases.h | a6f84f58fc4f | a52f79d8692e |
| tests/parser/ssa_state_map_fixture.h | 8c52fcfc80e3 | 02b55c83c7f5 |
| tests/parser/test_aot_c_global_shared_library_smoke.c | 516603c9d0bb | 1ba32c54131f |
| tests/parser/test_aot_c_guardrail_contracts.c | b260f156b128 | f51828105a9d |
| tests/parser/test_aot_c_load_const_scalar.c | f66eeb3fce66 | 52e54f4fa2c6 |
| tests/parser/test_aot_c_typed_scalar.c | 99edd1c88767 | 222afefbdf4c |
| tests/parser/test_cfg_constant_conditions.c | ce1f40ccb1c6 | d24c3ccf118d |
| tests/parser/test_cfg_finally_abrupt.c | 07e545e762d4 | a3f78ebacbd8 |
| tests/parser/test_cfg_float_arithmetic_conditions.c | e70f0d42d7e1 | 02a71e9c181c |
| tests/parser/test_cfg_integer_bitwise_conditions.c | f6b6de0ed82c | d07162d3d422 |
| tests/parser/test_cfg_reachability.c | c34b84b5a038 | 53f2bfb0bb04 |
| tests/parser/test_cfg_switch_constants.c | 79e8559aa76b | d7bfd8f98d57 |
| tests/parser/test_cfg_try_catch_edges.c | f886d35e2674 | c6fdd3b21b03 |
| tests/parser/test_cfg_typed_catch_branch_flow.c | fb789932a5d6 | 91a0925f3c9d |
| tests/parser/test_cfg_typed_catch_flow.c | 413259cc9b93 | f6f3fb4be90a |
| tests/parser/test_cfg_typed_catch_loop_flow.c | 94a9dff93901 | 0228d498fe23 |
| tests/parser/test_cfg_typed_catch_switch_flow.c | 2305ad236995 | 04aa03e1d18b |
| tests/parser/test_semir_dynamic_call_deopt.c | 0bec627594ee | 76f9bbefd641 |
| tests/parser/test_ssa_compare_metadata_guards.c | 4be74007d609 | 55f1930fcc3d |
| tests/parser/test_ssa_core_model.c | c7eb65c31447 | cc3313d754ca |
| tests/parser/test_ssa_deopt_aggregates.c | 577e3e75110c | 970e4129e311 |
| tests/parser/test_ssa_effects_verifier.c | 18729c913346 | e329bec18426 |
| tests/parser/test_ssa_module_constant_pool_verifier.c | 1a7570a45617 | a718d28dc151 |
| tests/parser/test_ssa_state_maps.c | 36cbe3a99f00 | 28778d625367 |
| tests/parser/test_ssa_value_validation.c | 2e4c17b7f080 | bf628b2e47cd |
| tests/unity_config.h | 45a7953baa66 | 89585a41562d |
| zr_vm_cli/src/zr_vm_cli/command/command.c | 40399ee44f9b | 3a171a3b439f |
| zr_vm_cli/src/zr_vm_cli/repl/repl_semantic_fact_walkers.c | a697474d991c | 89736f03cdea |
| zr_vm_cli/src/zr_vm_cli/repl/repl_session.c | e7425b0274be | 218222ccc3a4 |
| zr_vm_common/include/zr_vm_common/zr_io_conf.h | 629c0f7cf3e2 | 05de7187867f |
| zr_vm_core/include/zr_vm_core/gc_domain.h | c9d44c8683df | c586c7ed8556 |
| zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c | 356b558109f9 | d469336f09ef |
| zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_binding_rows.c | 740c5d4fde34 | 1de9eee6b064 |
| zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c | 25b3a1da3e5c | da4c6d9e861a |
| zr_vm_core/src/zr_vm_core/module/module_loader.c | c132aee76064 | 72df96ae247f |
| zr_vm_language_server/src/zr_vm_language_server/interface/lsp_binary_metadata_coordinates.c | 2997234c8833 | 583f07dd2c60 |
| zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_position.c | b115f68d7965 | 30c16eba95bb |
| zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_imports.c | 3bf30fc60b4c | 247c6971eaee |
| zr_vm_language_server/src/zr_vm_language_server/project/lsp_project_source_rename.c | 18b45e78dcba | c971e432327d |
| zr_vm_lib_ffi/CMakeLists.txt | a53e0f536d90 | 85167d3a467d |
| zr_vm_library/include/zr_vm_library/aot_runtime.h | 0232b02f82af | 14a922076162 |
| zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_values.c | 12d8307d8822 | f9166d27762b |
| zr_vm_library/src/zr_vm_library/native_binding/native_binding_call_binding.c | d8a53a568916 | f42ee3793c91 |
| zr_vm_library/src/zr_vm_library/project/project.c | 1c1748e2f6d9 | 19b815707c76 |
| zr_vm_parser/include/zr_vm_parser/location.h | 4655bbaac57a | 4e8c026efc3f |
| zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c | b84d10870ea4 | 061133cfd36d |
| zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder_fix_disposition.c | b65a78ec2dc0 | a79f133035b3 |
| zr_vm_parser/src/zr_vm_parser/parser/parser_expression_fragment.c | 382f50fdf43c | 0d5d1a08c264 |
| zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c | fbb9aa83841b | 97ea9d1bd6b5 |
| zr_vm_parser/src/zr_vm_parser/parser/parser_loops.c | b80e1d3c3317 | b1617ce84b09 |
| zr_vm_parser/src/zr_vm_parser/parser/parser_reference_syntax.c | 795f85ee7459 | 88dc11578cd6 |
| zr_vm_parser/src/zr_vm_parser/parser/parser_reserved_task.c | 127f0c97e876 | 921c4ddd671d |
| zr_vm_parser/src/zr_vm_parser/parser/parser_struct_init.c | 43e2899c7954 | c68986f96868 |
| zr_vm_parser/src/zr_vm_parser/parser/parser_yield.c | 24821d9981b9 | da8a1a7dd2d7 |
| zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c | e4544397b5ba | d0caee7e1668 |

## 102 个补登文件

| 文件 | 模块 / 角色 |
| --- | --- |
| tests/cmake/container-pair-numeric-tests.cmake | tests/cmake / config |
| tests/cmake/libffi-asm-dependencies-validation.cmake | tests/cmake / config |
| tests/cmake/msvc-object-paths-validation.cmake | tests/cmake / config |
| tests/cmake/native-call-binding-status-tests.cmake | tests/cmake / config |
| tests/cmake/run_array_bounds.cmake | tests/cmake / config |
| tests/cmake/run_binary_freshness.cmake | tests/cmake / config |
| tests/cmake/run_constructor_prototype_growth.cmake | tests/cmake / config |
| tests/cmake/runtime-compiler-boundary-tests.cmake | tests/cmake / config |
| tests/cmake/ssa-execir-mutation-boundary-check.cmake | tests/cmake / config |
| tests/cmake/ssa-execir-mutation-boundary.cmake | tests/cmake / config |
| tests/cmake/ssa-gc-map-metadata-consumers.cmake | tests/cmake / config |
| tests/cmake/ssa-gc-map-parser-consumers.cmake | tests/cmake / config |
| tests/cmake/ssa-gc-map-source-preflight.cmake | tests/cmake / config |
| tests/cmake/ssa-sccp-pooled-int64.cmake | tests/cmake / config |
| tests/cmake/ssa-source-integer-expression-faults.cmake | tests/cmake / config |
| tests/cmake/ssa-source-integer-expression-proof.cmake | tests/cmake / config |
| tests/cmake/ssa-source-integer-expression-support.cmake | tests/cmake / config |
| tests/cmake/value-debug-string-boundary-tests.cmake | tests/cmake / config |
| tests/cmake/zr_vm_lsp_declaration_budget_tests.cmake | tests/cmake / config |
| tests/container/test_container_pair_numeric.c | tests/container / test |
| tests/core/test_typed_opcode_operands.c | tests/core / test |
| tests/core/test_value_debug_string_bounds.c | tests/core / test |
| tests/fixtures/parser/constructor_prototype_growth.zr | tests/fixtures / fixture |
| tests/language_server/test_semantic_analyzer_generic_member_signature_cases.h | tests/language_server / test |
| tests/language_server/test_semantic_analyzer_generic_signature_range_cases.h | tests/language_server / test |
| tests/language_server/test_semantic_analyzer_lsp_native_range_consumer_cases.h | tests/language_server / test |
| tests/language_server/test_semantic_declaration_budget.c | tests/language_server / test |
| tests/language_server/test_semantic_environment_budget_cases.h | tests/language_server / test |
| tests/library/test_native_call_binding_status.c | tests/library / test |
| tests/parser/support/ssa_execir_builder_storage_cases.inc | tests/parser / test |
| tests/parser/support/ssa_execir_gc_map_outer_shape_cases.inc | tests/parser / test |
| tests/parser/support/ssa_gc_map_parser_consumers_fixture.h | tests/parser / test |
| tests/parser/support/ssa_gc_map_source_preflight_fixture.c | tests/parser / test |
| tests/parser/support/ssa_gc_map_source_preflight_fixture.h | tests/parser / test |
| tests/parser/support/ssa_sccp_pooled_int64_faults.c | tests/parser / test |
| tests/parser/support/ssa_sccp_pooled_int64_fixture.c | tests/parser / test |
| tests/parser/support/ssa_sccp_pooled_int64_fixture.h | tests/parser / test |
| tests/parser/support/ssa_source_integer_expression_faults_fixture.c | tests/parser / test |
| tests/parser/support/ssa_source_integer_expression_faults_fixture.h | tests/parser / test |
| tests/parser/support/ssa_source_integer_expression_faults_frame_shim.h | tests/parser / test |
| tests/parser/support/ssa_source_integer_expression_faults_graph_shim.h | tests/parser / test |
| tests/parser/support/ssa_source_integer_expression_faults_producer_shim.h | tests/parser / test |
| tests/parser/support/ssa_source_integer_expression_proof_shim.h | tests/parser / test |
| tests/parser/test_aot_c_dynamic_return_representation_cases.h | tests/parser / test |
| tests/parser/test_aot_c_generic_struct_return_cases.h | tests/parser / test |
| tests/parser/test_aot_c_scalar_call_proof_observation.h | tests/parser / test |
| tests/parser/test_aot_c_scalar_call_result_proof_cases.h | tests/parser / test |
| tests/parser/test_aot_integer_boundaries.c | tests/parser / test |
| tests/parser/test_binary_loader_rejection.c | tests/parser / test |
| tests/parser/test_callable_parameter_place_cases.h | tests/parser / test |
| tests/parser/test_compiled_function_lifetime.c | tests/parser / test |
| tests/parser/test_definite_assignment_callable_identity_cases.h | tests/parser / test |
| tests/parser/test_flat_expression_compile.c | tests/parser / test |
| tests/parser/test_parser_nesting_limit.c | tests/parser / test |
| tests/parser/test_primary_expression_source_range_cases.h | tests/parser / test |
| tests/parser/test_semantic_da_ast_fact_cases.h | tests/parser / test |
| tests/parser/test_semantic_loop_range_evidence_cases.h | tests/parser / test |
| tests/parser/test_semantic_receiver_guard_fact_cases.h | tests/parser / test |
| tests/parser/test_semantic_using_release_evidence_cases.h | tests/parser / test |
| tests/parser/test_semir_call_result_type_identity_cases.h | tests/parser / test |
| tests/parser/test_ssa_exec_ir_header_boundary.c | tests/parser / test |
| tests/parser/test_ssa_gc_map_metadata_consumers.c | tests/parser / test |
| tests/parser/test_ssa_gc_map_parser_consumers.c | tests/parser / test |
| tests/parser/test_ssa_gc_map_source_preflight.c | tests/parser / test |
| tests/parser/test_ssa_pass_manager_scalar_metadata.c | tests/parser / test |
| tests/parser/test_ssa_sccp_pooled_int64.c | tests/parser / test |
| tests/parser/test_ssa_source_integer_expression_faults.c | tests/parser / test |
| tests/parser/test_ssa_source_integer_expression_proof.c | tests/parser / test |
| tests/parser/test_token_source_composite_range_cases.h | tests/parser / test |
| tests/parser/test_token_source_construction_range_cases.h | tests/parser / test |
| tests/parser/test_token_source_diagnostic_evidence_cases.h | tests/parser / test |
| tests/parser/test_token_source_mixed_postfix_cases.h | tests/parser / test |
| tests/parser/test_token_source_projection_cases.h | tests/parser / test |
| tests/parser/test_token_source_range_evidence_cases.h | tests/parser / test |
| tests/parser/test_token_source_semantic_range_cases.h | tests/parser / test |
| tests/parser/test_weak_owner_release_evidence_cases.h | tests/parser / test |
| zr_vm_cli/src/zr_vm_cli/command/command_numbers_internal.h | zr_vm_cli / code |
| zr_vm_core/include/zr_vm_core/integer.h | zr_vm_core / code |
| zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_gc_map_internal.h | zr_vm_core / code |
| zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_mutation_internal.h | zr_vm_core / code |
| zr_vm_core/src/zr_vm_core/io/io_bytecode_verify.h | zr_vm_core / code |
| zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_parameters.c | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_parameters.h | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build_storage_internal.h | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_constants.c | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_constants.h | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/exec_ir/source/exec_ir_source_integer_expression.c | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_internal.h | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_strings.c | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/parser/parser_ownership_strings.h | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/parser/parser_range_evidence.c | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/parser/parser_range_evidence.h | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/parser/parser_source_projection.c | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/parser/parser_source_projection.h | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/semantic/semantic_diagnostic_evidence.c | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/semantic/semantic_diagnostic_evidence.h | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/syntax/source_identity.c | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/syntax/source_identity.h | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_da_ast.c | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/type_inference/dataflow/semantic_da_ast.h | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_context_lookup.h | zr_vm_parser / code |
| zr_vm_parser/src/zr_vm_parser/type_inference/type_environment_variable_lookup.c | zr_vm_parser / code |

## 保留的九条外来摘要漂移

- scripts/layout/apply_moves.py
- tests/cmake/source-layout-private-consumers.cmake
- tests/parser/test_pre_semantic_ir.c
- tests/tooling/test_layout_moves.py
- zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c
- zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
- zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_internal.h
- zr_vm_parser/src/zr_vm_parser/compiler/compile_time/compile_time_executor.c
- zr_vm_parser/src/zr_vm_parser/compiler/statement/compile_statement.c

## 证据与验收限制

范围原始报告为 E:/cargo-targets/zr_vm/reports/first-party-review-01a13463/20261017-pending-module-queue-audit-r2.json，SHA256 2273c0186effd2ac0d26d059411596008756649e00e302c067946c995b657390；writer 45784 与独立完整 reader 47832 均实际退出 0。初次 reader 的协调文本断言过窄失败保留，未改写原件。Root 在投影前重新枚举路径并逐个核对 4,194 个实际文件摘要及分类。

501 份 coverage 叶表中共 43,525 个唯一 (file, unit, kind) 键，未见重复键；该盘点不是契约真实性或当前版本完整性证明。zr_vm_core_math_conversion.tsv 中五条既有 FIXED 标签不符合四种结论规范，留待所属模块带调用证据复审；本批没有将它们自动改为任一结论或修复行为。

本批只运行现有清单 check 的前后基线以及暂存差异检查；不修改产品、测试、脚本或构建配置，不运行原生编译/测试，不新增实现镜像测试。清单 check 的预期既有 stale 返回码为 1；实际 stdout/stderr 和选择投影在同报告目录 scope-inventory-reconciliation-* 中保存。暂存检查、独立暂存复核与最终提交由负责人串行完成。函数级新增 0，历史 Root 已交付计数仍为 2,872；整个仓库尚未完成验收。

产物生命周期仍为 OPEN：保留范围、暂存与提交证据，复用既有构建/缓存；本批释放空间 0 B。
