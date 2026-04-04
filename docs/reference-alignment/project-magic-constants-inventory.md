---
related_code:
  - zr_vm_common/include/zr_vm_common.h
  - zr_vm_common/include/zr_vm_common/zr_hash_conf.h
  - zr_vm_common/include/zr_vm_common/zr_runtime_limits_conf.h
  - zr_vm_common/include/zr_vm_common/zr_gc_internal_conf.h
  - zr_vm_common/include/zr_vm_common/zr_parser_conf.h
  - zr_vm_common/include/zr_vm_common/zr_abi_conf.h
  - zr_vm_common/include/zr_vm_common/zr_path_conf.h
  - zr_vm_common/include/zr_vm_common/zr_constant_reference_conf.h
  - zr_vm_common/include/zr_vm_common/zr_runtime_sentinel_conf.h
  - zr_vm_common/include/zr_vm_common/zr_array_conf.h
  - zr_vm_library/include/zr_vm_library/conf.h
  - zr_vm_library/src/zr_vm_library/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding_internal.h
  - zr_vm_cli/include/zr_vm_cli/conf.h
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/src/zr_vm_library/project.c
  - zr_vm_cli/src/zr_vm_cli/project.h
  - zr_vm_cli/src/zr_vm_cli/project.c
  - zr_vm_cli/src/zr_vm_cli/app.c
  - zr_vm_cli/src/zr_vm_cli/compiler.c
  - zr_vm_cli/src/zr_vm_cli/repl.c
  - zr_vm_cli/src/zr_vm_cli/command.h
  - zr_vm_core/include/zr_vm_core/global.h
  - zr_vm_core/include/zr_vm_core/stack.h
  - zr_vm_core/include/zr_vm_core/module.h
  - zr_vm_core/src/zr_vm_core/gc_internal.h
  - zr_vm_core/src/zr_vm_core/gc.c
  - zr_vm_core/src/zr_vm_core/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/module_loader.c
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/compiler_instruction.c
  - zr_vm_parser/src/zr_vm_parser/compiler_state.c
  - zr_vm_parser/src/zr_vm_parser/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression_values.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler_class_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler_closure.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler_interface.c
  - zr_vm_parser/src/zr_vm_parser/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/compile_time_executor_support.c
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution_control.c
  - zr_vm_core/src/zr_vm_core/meta.c
  - zr_vm_core/src/zr_vm_core/value.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/reflection.c
  - zr_vm_core/src/zr_vm_core/hash.c
  - zr_vm_core/src/zr_vm_core/object.c
  - zr_vm_core/src/zr_vm_core/module_prototype.c
  - zr_vm_core/include/zr_vm_core/string.h
  - zr_vm_lib_math/include/zr_vm_lib_math/conf.h
  - zr_vm_lib_math/include/zr_vm_lib_math/math_common.h
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
  - zr_vm_lib_container/include/zr_vm_lib_container/conf.h
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_lib_ffi/include/zr_vm_lib_ffi/conf.h
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime_internal.h
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime_support.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - zr_vm_lib_system/include/zr_vm_lib_system/conf.h
  - zr_vm_lib_system/src/zr_vm_lib_system/fs.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/execution_meta_access.c
  - zr_vm_core/src/zr_vm_core/constant_reference.c
  - zr_vm_core/src/zr_vm_core/gc_object.c
  - zr_vm_core/src/zr_vm_core/gc_sweep.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_core/src/zr_vm_core/reflection.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler_quickening.c
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser_extern.c
  - zr_vm_parser/src/zr_vm_parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/parser_literals.c
  - zr_vm_parser/src/zr_vm_parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser_struct.c
  - zr_vm_parser/src/zr_vm_parser/parser_types.c
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/src/zr_vm_language_server/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_interface_support.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/symbol_table.c
  - zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_project_imports.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_project_features.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_project_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_semantic_tokens.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
  - zr_vm_language_server/stdio/stdio_documents.c
  - zr_vm_language_server/stdio/stdio_json.c
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - scripts/audit_magic_constants.py
implementation_files:
  - zr_vm_common/include/zr_vm_common.h
  - zr_vm_common/include/zr_vm_common/zr_hash_conf.h
  - zr_vm_common/include/zr_vm_common/zr_runtime_limits_conf.h
  - zr_vm_common/include/zr_vm_common/zr_gc_internal_conf.h
  - zr_vm_common/include/zr_vm_common/zr_parser_conf.h
  - zr_vm_common/include/zr_vm_common/zr_abi_conf.h
  - zr_vm_common/include/zr_vm_common/zr_path_conf.h
  - zr_vm_common/include/zr_vm_common/zr_constant_reference_conf.h
  - zr_vm_common/include/zr_vm_common/zr_runtime_sentinel_conf.h
  - zr_vm_library/include/zr_vm_library/conf.h
  - zr_vm_library/src/zr_vm_library/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding_internal.h
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
  - zr_vm_cli/include/zr_vm_cli/conf.h
  - zr_vm_cli/src/zr_vm_cli/project.c
  - zr_vm_cli/src/zr_vm_cli/app.c
  - zr_vm_cli/src/zr_vm_cli/compiler.c
  - zr_vm_cli/src/zr_vm_cli/project.h
  - zr_vm_cli/src/zr_vm_cli/repl.c
  - zr_vm_cli/src/zr_vm_cli/command.h
  - zr_vm_core/src/zr_vm_core/gc.c
  - zr_vm_core/src/zr_vm_core/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution_control.c
  - zr_vm_core/src/zr_vm_core/meta.c
  - zr_vm_core/src/zr_vm_core/value.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/string.h
  - zr_vm_core/src/zr_vm_core/execution_meta_access.c
  - zr_vm_core/src/zr_vm_core/constant_reference.c
  - zr_vm_core/src/zr_vm_core/gc_object.c
  - zr_vm_core/src/zr_vm_core/gc_sweep.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_core/src/zr_vm_core/reflection.c
  - zr_vm_core/src/zr_vm_core/object.c
  - zr_vm_core/src/zr_vm_core/module_prototype.c
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser_extern.c
  - zr_vm_parser/src/zr_vm_parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/parser_literals.c
  - zr_vm_parser/src/zr_vm_parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser_struct.c
  - zr_vm_parser/src/zr_vm_parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler_state.c
  - zr_vm_parser/src/zr_vm_parser/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression_values.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression_support.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler_class_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler_closure.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler_struct.c
  - zr_vm_parser/src/zr_vm_parser/compiler_interface.c
  - zr_vm_parser/src/zr_vm_parser/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/type_system.c
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/compile_time_executor_support.c
  - zr_vm_lib_container/include/zr_vm_lib_container/conf.h
  - zr_vm_lib_math/include/zr_vm_lib_math/conf.h
  - zr_vm_lib_math/src/zr_vm_lib_math/common.c
  - zr_vm_lib_ffi/include/zr_vm_lib_ffi/conf.h
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime_support.c
  - zr_vm_lib_system/include/zr_vm_lib_system/conf.h
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/src/zr_vm_language_server/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_interface_support.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_project_features.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer_support.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_semantic_tokens.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer_typecheck.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c
  - zr_vm_language_server/stdio/stdio_json.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/stdio_transport.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
  - zr_vm_language_server/stdio/stdio_documents.c
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - scripts/audit_magic_constants.py
plan_sources:
  - user: 2026-04-04 implement 项目级魔法数与常量收敛计划
  - .codex/plans/项目级魔法数与常量收敛计划.md
tests:
  - scripts/audit_magic_constants.py
doc_type: inventory
---

# 项目级魔法数与常量收敛 inventory

这份 inventory 是当前批次常量收敛的唯一清单。后续继续迁移时，以这里的“原位置 / 含义 / 目标归属 / 是否跨模块”为准，不再回到零散聊天记录或临时 grep 结果。

## 收敛规则

- 真正跨模块共享的运行时策略、阈值、ABI、格式、路径与工件常量进入 `zr_vm_common`
- 单模块使用但属于配置性质的常量进入该模块 `conf.h`
- 描述表、字段名数组、token/keyword 表、帮助文本、JSON/type-hint blob 保留在实现或描述表，不按 `conf` 常量处理
- 已有公共别名如果仍在对外头文件里暴露，可以暂时保留为 alias，但底层数值源头必须收敛到 `zr_vm_common`

## 本批次已进入 `zr_vm_common`

| 常量 | 原位置 | 含义 | 目标归属 | 是否跨模块 |
| --- | --- | --- | --- | --- |
| `ZR_GLOBAL_API_STRING_CACHE_BUCKET_COUNT` | `zr_vm_core/include/zr_vm_core/global.h` | API 字符串缓存桶数 | `zr_runtime_limits_conf.h` | 是 |
| `ZR_GLOBAL_API_STRING_CACHE_BUCKET_DEPTH` | `zr_vm_core/include/zr_vm_core/global.h` | API 字符串缓存桶深度 | `zr_runtime_limits_conf.h` | 是 |
| `ZR_STRING_TABLE_INITIAL_SIZE_LOG2` | `zr_vm_core/include/zr_vm_core/global.h` | 字符串表初始 log2 容量 | `zr_runtime_limits_conf.h` | 是 |
| `ZR_OBJECT_TABLE_INITIAL_SIZE_LOG2` | `zr_vm_core/include/zr_vm_core/global.h` | 对象/原型哈希表初始 log2 容量 | `zr_runtime_limits_conf.h` | 是 |
| `ZR_STACK_NATIVE_CALL_RESERVED_MIN` | `zr_vm_core/include/zr_vm_core/stack.h` | 原生调用最小栈预留 | `zr_runtime_limits_conf.h` | 是 |
| `ZR_VM_READ_ALL_IO_FALLBACK_CAPACITY` | `zr_vm_core/src/zr_vm_core/module_loader.c`, `zr_vm_parser/src/zr_vm_parser/type_inference_import_metadata.c` | `read-all` 类 IO 的回退初始容量 | `zr_runtime_limits_conf.h` | 是 |
| `ZR_IO_SOURCE_SIGNATURE_LENGTH / ZR_IO_SOURCE_HEADER_OPT_BYTES` | `zr_vm_parser/src/zr_vm_parser/writer.c` | `.source` 文件头的 signature 宽度与保留 `opt` 字节数，writer 不再直接裸写 `4` 和 `3` | `zr_io_conf.h` | 是 |
| `ZR_IO_SOURCE_PATCH_HAS_COMPILE_TIME_METADATA / ZR_IO_SOURCE_PATCH_HAS_PROTOTYPE_BLOB / ZR_IO_SOURCE_PATCH_HAS_SEMIR_METADATA / ZR_IO_SOURCE_PATCH_HAS_MEMBER_ENTRIES / ZR_IO_SOURCE_PATCH_HAS_CALLSITE_CACHE` | `zr_vm_core/src/zr_vm_core/io.c` | `.zro/.source` reader 依据 patch 版本打开 compile-time metadata、prototype blob、SemIR metadata、member entries 与 callsite cache 等代际特性 | `zr_io_conf.h` | 是 |
| `ZR_GC_WORK_TO_MEMORY_BYTES` | `zr_vm_core/src/zr_vm_core/gc_internal.h` | GC work 到 debt byte 的换算倍率 | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_SWEEP_SLICE_BUDGET_MAX` | `zr_vm_core/src/zr_vm_core/gc_internal.h` | sweep 单步最大 slice 预算 | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_DEFAULT_PAUSE_BUDGET` | `zr_vm_core/src/zr_vm_core/gc_internal.h` | 默认 pause budget | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_DEFAULT_SWEEP_SLICE_BUDGET` | `zr_vm_core/src/zr_vm_core/gc_internal.h` | 默认 sweep slice budget | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_FINALIZER_BATCH_MAX` | `zr_vm_core/src/zr_vm_core/gc_internal.h` | 单批 finalizer 最大个数 | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_FINALIZER_WORK_COST` | `zr_vm_core/src/zr_vm_core/gc_internal.h` | finalizer work 成本折算 | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_DEBT_CREDIT_BYTES` | `zr_vm_core/src/zr_vm_core/gc.c`, `zr_vm_core/src/zr_vm_core/gc_cycle.c` | 增量/idle GC 步进后回冲 debt 的字节额度 | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_MANAGED_MEMORY_DRIFT_TOLERANCE_DIVISOR` | `zr_vm_core/src/zr_vm_core/gc_cycle.c` | GC 估算 managed memory 与真实对象大小偏差的重校准阈值分母 | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_GRAY_LIST_DUPLICATE_SCAN_LIMIT` | `zr_vm_core/src/zr_vm_core/gc_mark.c` | gray list 重复检测保护上限 | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_PROPAGATE_ALL_ITERATION_LIMIT` | `zr_vm_core/src/zr_vm_core/gc_mark.c` | mark propagate 全量循环保护上限 | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_RUN_UNTIL_STATE_ITERATION_LIMIT` | `zr_vm_core/src/zr_vm_core/gc_cycle.c` | `run_until_state` 循环保护上限 | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_GENERATIONAL_FULL_SWEEP_ITERATION_LIMIT` | `zr_vm_core/src/zr_vm_core/gc_cycle.c` | generational full sweep 循环保护上限 | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_SHUTDOWN_FULL_COLLECTION_ITERATION_LIMIT` | `zr_vm_core/src/zr_vm_core/gc.c` | 全局状态析构期间 full GC 推进循环的保护上限 | `zr_gc_internal_conf.h` | 是 |
| `ZR_PARSER_LEXER_BUFFER_INITIAL_SIZE` | `zr_vm_parser/src/zr_vm_parser/lexer.c` | lexer 初始缓冲大小 | `zr_parser_conf.h` | 是 |
| `ZR_PARSER_MAX_CONSECUTIVE_ERRORS` | `zr_vm_parser/src/zr_vm_parser/parser.c` | parser 最大连续错误数 | `zr_parser_conf.h` | 是 |
| `ZR_PARSER_MAX_RECOVERY_SKIP_TOKENS` | `zr_vm_parser/src/zr_vm_parser/parser.c` | parser 恢复阶段最大跳过 token 数 | `zr_parser_conf.h` | 是 |
| `ZR_PARSER_COMPILE_TIME_RUNTIME_SAFE_MAX_DEPTH` | `zr_vm_parser/src/zr_vm_parser/compiler_instruction.c` | compile-time value 投影到 runtime 的递归深度上限 | `zr_parser_conf.h` | 是 |
| `ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH` | `zr_vm_parser/src/zr_vm_parser/compile_expression_types.c`, `zr_vm_parser/src/zr_vm_parser/type_inference_core.c` | 类型原型/成员递归查找保护上限 | `zr_parser_conf.h` | 是 |
| `ZR_PARSER_DYNAMIC_CAPACITY_GROWTH_FACTOR / ZR_PARSER_INITIAL_CAPACITY_PAIR / ZR_PARSER_INITIAL_CAPACITY_TINY / ZR_PARSER_INITIAL_CAPACITY_SMALL / ZR_PARSER_INITIAL_CAPACITY_MEDIUM / ZR_PARSER_INITIAL_CAPACITY_LARGE / ZR_PARSER_INSTRUCTION_INITIAL_CAPACITY` | `zr_vm_parser/src/zr_vm_parser/compiler_state.c`, `compiler_class.c`, `compiler_struct.c`, `compiler_interface.c`, `compiler_extern_declaration.c`, `type_inference_import_metadata.c`, `type_inference_native.c`, `type_system.c`, `semantic.c`, `compile_time_executor_support.c`, `compiler_closure.c`, `compiler_bindings.c`, `parser.c`, `parser_class.c`, `parser_extern.c`, `parser_expression_primary.c`, `parser_interface.c`, `parser_literals.c`, `parser_statements.c`, `parser_struct.c`, `parser_types.c`, `ast.c`, `lexer.c` | parser 编译器状态、语法树/块/成员/参数 collection 初始容量分层，以及 AST/lexer 共享动态扩容倍率 | `zr_parser_conf.h` | 是 |
| `ZR_PARSER_I32_NONE` | `zr_vm_parser/src/zr_vm_parser/compiler_class_support.c`, `compiler_generic_semantics.c`, `type_inference_generic_calls.c` | parser 内部若干 helper 使用的 `TZrInt32` “未命中索引” 哨兵，避免继续散写 raw `-1` | `zr_parser_conf.h` | 是 |
| `ZR_PARSER_U32_NONE / ZR_PARSER_SLOT_NONE / ZR_PARSER_MEMBER_ID_NONE / ZR_PARSER_INDEX_NONE` | `zr_vm_parser/src/zr_vm_parser/compiler_bindings.c`, `zr_vm_parser/src/zr_vm_parser/compile_expression_values.c`, `zr_vm_parser/src/zr_vm_parser/compile_expression.c`, `zr_vm_parser/src/zr_vm_parser/compile_expression_support.c`, `zr_vm_parser/src/zr_vm_parser/compile_expression_types.c`, `zr_vm_parser/src/zr_vm_parser/compiler_class_support.c`, `zr_vm_parser/src/zr_vm_parser/compiler_closure.c`, `zr_vm_parser/src/zr_vm_parser/compiler_extern_declaration.c`, `zr_vm_parser/src/zr_vm_parser/compiler_function.c`, `zr_vm_parser/src/zr_vm_parser/compiler_extern_emit.c`, `zr_vm_parser/src/zr_vm_parser/compiler_instruction.c`, `zr_vm_parser/src/zr_vm_parser/compiler_locals.c`, `zr_vm_parser/src/zr_vm_parser/compile_expression_call.c`, `zr_vm_parser/src/zr_vm_parser/compile_statement.c`, `zr_vm_parser/src/zr_vm_parser/compile_statement_flow.c` | parser 统一的 `TZrUInt32 all-ones` 哨兵边界，用于 slot、member id、child/closure index 与 generic index 的“未命中/失败”语义，也覆盖局部变量/栈槽绑定与分配 helper，以及 `compiler_locals.c` 内 constant-index wrapper 的失败返回边界 | `zr_parser_conf.h` | 是 |
| `ZR_PARSER_LABEL_ID_NONE` | `zr_vm_parser/src/zr_vm_parser/compiler_scope.c`, `zr_vm_parser/src/zr_vm_parser/compile_statement_flow.c` | parser 控制流 label/finally label 的“未创建/不存在”哨兵 | `zr_parser_conf.h` | 是 |
| `ZR_PARSER_LEXER_EOZ` | `zr_vm_parser/src/zr_vm_parser/lexer.c`, `zr_vm_parser/src/zr_vm_parser/parser_state.c` | lexer `currentChar` 跨文件共享的 end-of-input 哨兵 | `zr_parser_conf.h` | 是 |
| `ZR_PARSER_ARRAY_SIZE_BUFFER_LENGTH / ZR_PARSER_INTEGER_BUFFER_LENGTH / ZR_PARSER_GENERATED_NAME_BUFFER_LENGTH / ZR_PARSER_TYPE_NAME_BUFFER_LENGTH / ZR_PARSER_DETAIL_BUFFER_LENGTH / ZR_PARSER_ERROR_BUFFER_LENGTH / ZR_PARSER_TEXT_BUFFER_LENGTH / ZR_PARSER_DECLARATION_BUFFER_LENGTH` | `zr_vm_parser/src/zr_vm_parser/type_inference_core.c`, `compiler_class.c`, `compiler_struct.c`, `type_inference_native.c`, `compile_time_executor_support.c`, `type_inference.c`, `parser_state.c`, `writer_intermediate.c`, `compiler_extern_helpers.c`, `compile_statement.c`, `type_inference_generic_calls.c`, `type_inference_passing_modes.c`, `compiler_class_support.c`, `compiler_generic_semantics.c`, `compile_expression.c`, `compile_expression_call.c`, `compile_time_executor.c`, `parser_declarations.c`, `parser_extern.c`, `parser_expression_primary.c`, `lexer.c` | parser synthetic name、类型名、细节文本、通用诊断、声明文本、snippet 与 typed metadata 输出的共享缓冲区上限 | `zr_parser_conf.h` | 是 |
| `ZR_PARSER_ERROR_SNIPPET_CONTEXT_RADIUS / ZR_PARSER_ERROR_SNIPPET_FOCUS_COLUMN` | `zr_vm_parser/src/zr_vm_parser/lexer.c`, `zr_vm_parser/src/zr_vm_parser/parser_state.c` | parser 诊断 snippet 左右上下文窗口和 caret 对齐列 | `zr_parser_conf.h` | 是 |
| `ZR_TYPE_RANGE_INT8_MIN / ZR_TYPE_RANGE_INT8_MAX / ZR_TYPE_RANGE_INT16_MIN / ZR_TYPE_RANGE_INT16_MAX / ZR_TYPE_RANGE_INT32_MIN / ZR_TYPE_RANGE_INT32_MAX / ZR_TYPE_RANGE_INT64_MIN / ZR_TYPE_RANGE_INT64_MAX / ZR_TYPE_RANGE_UINT8_MAX / ZR_TYPE_RANGE_UINT16_MAX / ZR_TYPE_RANGE_UINT32_MAX / ZR_TYPE_RANGE_UINT64_MAX` | `zr_vm_parser/src/zr_vm_parser/type_inference.c` | 编译期整数字面量对目标基础类型做 range-check 时使用的统一上下界；`uint64` 当前显式受 AST `TZrInt64` 存储上限约束 | `zr_type_conf.h` | 是 |
| `ZR_RUNTIME_SMALL_TEXT_BUFFER_LENGTH / ZR_RUNTIME_MEMBER_NAME_BUFFER_LENGTH / ZR_RUNTIME_TYPE_NAME_BUFFER_LENGTH / ZR_RUNTIME_DIAGNOSTIC_BUFFER_LENGTH / ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH / ZR_RUNTIME_ERROR_BUFFER_LENGTH / ZR_RUNTIME_DEBUG_STRING_BUFFER_LENGTH / ZR_RUNTIME_DEBUG_COLLECTION_PREVIEW_MAX / ZR_RUNTIME_REFLECTION_FORMAT_BUFFER_LENGTH / ZR_RUNTIME_OBJECT_CALL_INLINE_ARGUMENT_CAPACITY / ZR_RUNTIME_OBJECT_PROTOTYPE_INITIAL_CAPACITY / ZR_RUNTIME_OBJECT_PROTOTYPE_GROWTH_FACTOR / ZR_RUNTIME_PROTOTYPE_INHERIT_INITIAL_CAPACITY / ZR_NATIVE_CALL_STACK_ERROR_HANDLER_GUARD_DIVISOR / ZR_NATIVE_CALL_STACK_ERROR_HANDLER_GUARD_MULTIPLIER` | `zr_vm_core/src/zr_vm_core/debug.c`, `execution_dispatch.c`, `meta.c`, `value.c`, `reflection.c`, `object.c`, `module_prototype.c`, `function.c` | core 运行时错误、调试字符串预览、反射格式化、对象调用 inline scratch 参数容量、对象原型成员表扩容策略、error-handler 二次 native 栈保护比例，以及原型继承解析的共享 buffer/容量上限 | `zr_runtime_limits_conf.h` | 是 |
| `ZR_META_CALL_UNARY_ARGUMENT_COUNT / ZR_META_CALL_MAX_ARGUMENTS / ZR_META_CALL_SELF_SLOT_OFFSET / ZR_META_CALL_SECOND_ARGUMENT_SLOT_OFFSET` | `zr_vm_core/src/zr_vm_core/execution_control.c`, `zr_vm_core/src/zr_vm_core/execution_dispatch.c`, `zr_vm_core/src/zr_vm_core/meta.c` | 元调用 helper 共享的 unary/binary arity、self/second-arg 栈槽偏移，以及 scratch slot / stackTop 计算约定 | `zr_meta_conf.h` | 是 |
| `ZR_STABLE_HASH_FILE_CHUNK_BUFFER_LENGTH / ZR_STABLE_HASH_HEX_BUFFER_LENGTH / ZR_STABLE_HASH_HEX_PRINTF_FORMAT / ZR_STABLE_HASH_FNV1A64_OFFSET_BASIS / ZR_STABLE_HASH_FNV1A64_PRIME` | `zr_vm_cli/src/zr_vm_cli/compiler.c`, `zr_vm_cli/src/zr_vm_cli/project.c`, `zr_vm_library/src/zr_vm_library/aot_runtime.c` | CLI 增量 manifest 与 AOT runtime 工件比对共用的稳定哈希 chunk 大小、FNV-1a 参数、hex 缓冲和格式化约定 | `zr_hash_conf.h` | 是 |
| `ZR_RUNTIME_REFLECTION_MEMBER_HASH_BASE / ZR_RUNTIME_REFLECTION_METHOD_HASH_BASE` | `zr_vm_core/src/zr_vm_core/reflection.c` | reflection 参数、成员与 method info 的 hash/id 分段基数，避免 helper 里继续散写 `+1` / `+100` 约定 | `zr_hash_conf.h` | 是 |
| `ZR_VM_NATIVE_RUNTIME_ABI_VERSION` | `zr_vm_library/include/zr_vm_library/native_binding.h` | native runtime ABI 版本 | `zr_abi_conf.h` | 是 |
| `ZR_VM_NATIVE_PLUGIN_ABI_VERSION` | `zr_vm_library/include/zr_vm_library/native_registry.h` | native plugin ABI 版本 | `zr_abi_conf.h` | 是 |
| `ZR_NATIVE_MODULE_INFO_VERSION` | `zr_vm_core/include/zr_vm_core/module.h` | native module metadata version | `zr_abi_conf.h` | 是 |
| `ZR_CLI_MANIFEST_VERSION` | `zr_vm_cli/src/zr_vm_cli/project.h` | CLI incremental manifest 版本 | `zr_abi_conf.h` | 是 |
| `ZR_VM_PATH_LENGTH_MAX` | `zr_vm_library/include/zr_vm_library/conf.h` 原先 `ZR_LIBRARY_MAX_PATH_LENGTH` 数值源头 | 项目级路径长度上限 | `zr_path_conf.h` | 是 |
| `ZR_VM_POSIX_DIRECTORY_CREATE_MODE` | `zr_vm_cli/src/zr_vm_cli/project.c`, `zr_vm_lib_system/src/zr_vm_lib_system/fs.c` | POSIX 目录创建默认模式 | `zr_path_conf.h` | 是 |
| `ZR_VM_SOURCE_MODULE_FILE_EXTENSION` / `..._LENGTH` | `zr_vm_library/src/zr_vm_library/project.c`, `zr_vm_cli/src/zr_vm_cli/project.c` | 源文件扩展名与裁剪长度 | `zr_path_conf.h` | 是 |
| `ZR_VM_BINARY_MODULE_FILE_EXTENSION` / `..._LENGTH` | `zr_vm_library/src/zr_vm_library/project.c`, `zr_vm_cli/src/zr_vm_cli/project.c` | 二进制工件扩展名与裁剪长度 | `zr_path_conf.h` | 是 |
| `ZR_VM_INTERMEDIATE_MODULE_FILE_EXTENSION` / `..._LENGTH` | `zr_vm_cli/src/zr_vm_cli/project.c` | 中间工件扩展名与裁剪长度 | `zr_path_conf.h` | 是 |
| `EZrConstantReferenceStepType / ZR_CONSTANT_REF_STEP_TO_UINT32 / ZR_CONSTANT_REF_STEP_FROM_UINT32` | `zr_vm_parser/include/zr_vm_parser/compiler.h`, `zr_vm_core/src/zr_vm_core/constant_reference.c` | 编译器与运行时共享的 constant-reference 路径步骤标签及存储转换约定 | `zr_constant_reference_conf.h` | 是 |
| `ZR_RUNTIME_CALLSITE_CACHE_MEMBER_ENTRY_NONE` | `zr_vm_core/include/zr_vm_core/function.h`, `zr_vm_core/src/zr_vm_core/execution_meta_access.c`, `zr_vm_parser/src/zr_vm_parser/compiler_quickening.c` | 运行时 PIC/callsite cache 的“未命中成员描述符”哨兵 | `zr_runtime_sentinel_conf.h` | 是 |
| `ZR_RUNTIME_DEBUG_HOOK_LINE_NONE` | `zr_vm_core/src/zr_vm_core/function.c`, `zr_vm_core/src/zr_vm_core/debug.c` | synthetic debug-hook `CALL/RETURN` 事件没有真实源码行号时使用的无行号哨兵 | `zr_runtime_sentinel_conf.h` | 是 |
| `ZR_DEBUG_SIGNAL_NONE` | `zr_vm_core/src/zr_vm_core/debug.c` | execution trace/debug dispatch 的“无 trap / 无调试信号”边界 | `zr_common_conf.h` | 是 |
| `ZR_RUNTIME_SEMIR_DEOPT_ID_NONE` | `zr_vm_parser/src/zr_vm_parser/compiler_semir.c`, `zr_vm_parser/src/zr_vm_parser/compiler_quickening.c` | SemIR / quickening / writer-io-runtime 链路共享的“无 deopt”哨兵；`0` 表示该条 SemIR 指令或 callsite cache 没有可回退 deopt 记录 | `zr_runtime_sentinel_conf.h` | 是 |
| `ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND` | `zr_vm_core/src/zr_vm_core/gc_object.c`, `zr_vm_core/src/zr_vm_core/gc_sweep.c`, `zr_vm_core/src/zr_vm_core/state.c` | GC 与状态析构阶段共享的低地址无效指针保护下界 | `zr_runtime_sentinel_conf.h` | 是 |
| `ZR_RUNTIME_REFLECTION_ENTRY_HASH_SALT` | `zr_vm_core/src/zr_vm_core/reflection.c` | 将模块 `__entry` 反射对象与普通成员哈希空间隔离的盐值 | `zr_runtime_sentinel_conf.h` | 是 |
| `ZR_THREAD_STATUS_INVALID` | `zr_vm_core/src/zr_vm_core/exception.c` | native exception fallback 在未显式设置状态时使用的无效 thread-status 哨兵 | `zr_thread_conf.h` | 是 |
| `ZR_EXCEPTION_SOURCE_LINE_NONE` | `zr_vm_core/src/zr_vm_core/exception.c` | exception stack-frame / unhandled 打印路径中“无可映射源码行”的本地边界；继续保留 `0`，避免把 debug-hook 专用的 `ZR_RUNTIME_DEBUG_HOOK_LINE_NONE` 暴露到用户栈追踪 | 文件私有常量 | 否 |
| `ZR_TYPE_INFERENCE_GENERIC_START_NOT_FOUND / ZR_TYPE_INFERENCE_OVERLOAD_SCORE_INCOMPATIBLE` | `zr_vm_parser/src/zr_vm_parser/type_inference_core.c` | generic type-name 解析的“尚未遇到 `<`”游标哨兵，以及 overload scoring 的“候选不兼容”私有评分哨兵 | 文件私有常量 | 否 |
| `ZR_META_COMPARE_RESULT_LESS / ZR_META_COMPARE_RESULT_EQUAL / ZR_META_COMPARE_RESULT_GREATER` | `zr_vm_core/src/zr_vm_core/meta.c` | `%compare` float helper 的本地三态比较结果 | 文件私有常量 | 否 |
| `ZR_LSP_STDIO_HEX_DIGIT_INVALID / ZR_LSP_PROJECT_HEX_DIGIT_INVALID` | `zr_vm_language_server/stdio/stdio_documents.c`, `zr_vm_language_server/src/zr_vm_language_server/lsp_project.c` | URI percent-decoding helper 的非法十六进制字符返回值 | 文件私有常量 | 否 |
| `ZR_LSP_SIGNATURE_BINDING_INDEX_NONE / ZR_LSP_RANGE_SPAN_SCORE_INVALID` | `zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c`, `zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c` | signature-help generic binding lookup 的未命中索引哨兵，以及 reference span score 的无效值哨兵 | 文件私有常量 | 否 |
| `ZR_LSP_SYMBOL_POSITION_COMPARE_* / ZR_LSP_SEMANTIC_TOKEN_COMPARE_* / ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN` | `zr_vm_language_server/src/zr_vm_language_server/symbol_table.c`, `zr_vm_language_server/src/zr_vm_language_server/lsp_semantic_tokens.c` | LSP 文件位置比较三态结果，以及 semantic-token helper 的未知类型/排序比较私有结果码 | 文件私有常量 | 否 |
| `ZR_EXCEPTION_HANDLER_STACK_INITIAL_CAPACITY / ZR_EXCEPTION_HANDLER_STACK_GROWTH_FACTOR` | `zr_vm_core/src/zr_vm_core/execution_control.c` | exception-handler stack 本地扩容起点与倍率 | 文件私有常量 | 否 |
| `ZR_SCOPE_CLEANUP_CLOSED_COUNT_NONE` | `zr_vm_core/src/zr_vm_core/execution_control.c` | scope-cleanup helper 没有关闭任何注册清理项时返回的本地零结果 | 文件私有常量 | 否 |
| `ZR_CLOSURE_CLOSED_COUNT_NONE` | `zr_vm_core/src/zr_vm_core/closure.c` | `CloseRegisteredValues` helper 没有关闭任何已注册值时返回的本地零结果 | 文件私有常量 | 否 |
| `ZR_GC_IGNORE_REGISTRY_INITIAL_CAPACITY / ZR_GC_IGNORE_REGISTRY_GROWTH_FACTOR` | `zr_vm_core/src/zr_vm_core/gc_object.c` | GC ignored-object registry 本地扩容起点与倍率 | 文件私有常量 | 否 |
| `ZR_LSP_NATIVE_MODULE_COMPLETION_MAX_DEPTH` | `zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer.c` | native module descriptor completion helper 的本地递归深度保护上限 | 文件私有常量 | 否 |
| `ZR_HASH_SET_CAPACITY_GROWTH_FACTOR / ZR_HASH_SET_MAX_LOAD_NUMERATOR / ZR_HASH_SET_MAX_LOAD_DENOMINATOR` | `zr_vm_core/include/zr_vm_core/hash_set.h`, `zr_vm_core/src/zr_vm_core/hash_set.c` | hash-set helper 的本地扩容倍率与 resize threshold 负载因子分数 | 文件私有常量 | 否 |
| `ZR_SEMIR_TYPE_TABLE_DEFAULT_ENTRY_COUNT / ZR_SEMIR_TYPE_TABLE_DEFAULT_INDEX / ZR_SEMIR_OWNERSHIP_STATE_TABLE_CAPACITY / ZR_SEMIR_OWNERSHIP_STATE_INDEX_FIRST / ZR_SEMIR_DEOPT_ID_FIRST` | `zr_vm_parser/src/zr_vm_parser/compiler_semir.c` | SemIR helper 的默认类型表基项个数、默认 type-table 回退索引、ownership-state 去重表容量、ownership-state 首槽回退索引与首个可分配 deopt id；其中 `deopt none=0` 已升级为共享 contract `ZR_RUNTIME_SEMIR_DEOPT_ID_NONE` | 文件私有常量 | 否 |
| `ZR_AOT_COUNT_NONE / ZR_AOT_FUNCTION_TREE_ROOT_INDEX / ZR_AOT_EMBEDDED_BLOB_EMPTY_BYTE` | `zr_vm_parser/src/zr_vm_parser/backend_aot.c` | AOT helper 的空计数、flatten 后根函数索引和空 embedded blob 占位字节；仅服务当前 lowering/writer 实现，不形成跨模块协议 | 文件私有常量 | 否 |
| `ZR_COMPILER_QUICKENING_MEMBER_FLAGS_NONE` | `zr_vm_parser/src/zr_vm_parser/compiler_quickening.c` | quickening 在找不到 member entry 或成员表缺失时返回的本地零 flags 回退值 | 文件私有常量 | 否 |

## 本批次已进入模块 `conf.h`

| 常量 | 原位置 | 含义 | 目标归属 | 是否跨模块 |
| --- | --- | --- | --- | --- |
| `ZR_MATH_PI` | `zr_vm_lib_math/include/zr_vm_lib_math/math_common.h` 原 `M_PI` fallback | math 圆周率常量 | `zr_vm_lib_math/conf.h` | 否 |
| `ZR_MATH_TAU` | `zr_vm_lib_math/src/zr_vm_lib_math/module.c` 原 `M_PI * 2.0` | math `TAU` 常量 | `zr_vm_lib_math/conf.h` | 否 |
| `ZR_MATH_E` | `zr_vm_lib_math/src/zr_vm_lib_math/module.c` 原 `2.71828182845904523536` | math `E` 常量 | `zr_vm_lib_math/conf.h` | 否 |
| `ZR_MATH_EPSILON` | `zr_vm_lib_math/include/zr_vm_lib_math/math_common.h` | math 近似比较 epsilon | `zr_vm_lib_math/conf.h` | 否 |
| `ZR_MATH_FORMAT_BUFFER_LENGTH` | `zr_vm_lib_math/src/zr_vm_lib_math/common.c` | math `toString`/格式化结果缓冲区长度 | `zr_vm_lib_math/conf.h` | 否 |
| `ZR_CONTAINER_SEQUENCE_INITIAL_CAPACITY` | `zr_vm_lib_container/src/zr_vm_lib_container/module.c` | `zr.container.Array` 容量下限 | `zr_vm_lib_container/conf.h` | 否 |
| `ZR_CONTAINER_SEQUENCE_GROWTH_FACTOR` | `zr_vm_lib_container/src/zr_vm_lib_container/module.c` | `zr.container.Array` 扩容倍率 | `zr_vm_lib_container/conf.h` | 否 |
| `ZR_CONTAINER_HASH_MIX_PRIME` / `ZR_CONTAINER_HASH_MIX_OFFSET` | `zr_vm_lib_container/src/zr_vm_lib_container/module.c` | `Pair.hashCode()` 哈希混合常量 | `zr_vm_lib_container/conf.h` | 否 |
| `ZR_FFI_ERROR_BUFFER_LENGTH` | `zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c`, `zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime_internal.h` | FFI 错误缓冲区长度 | `zr_vm_lib_ffi/conf.h` | 否 |
| `ZR_FFI_DIAGNOSTIC_BUFFER_LENGTH` | `zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime_support.c` | FFI 内部诊断与异常转错误文本的格式化缓冲区长度 | `zr_vm_lib_ffi/conf.h` | 否 |
| `ZR_SYSTEM_FS_PATH_MAX` | `zr_vm_lib_system/src/zr_vm_lib_system/fs.c` | `zr.system.fs` 的路径缓冲上限 | `zr_vm_lib_system/conf.h` | 否 |
| `ZR_SYSTEM_FS_DIRECTORY_MODE` | `zr_vm_lib_system/src/zr_vm_lib_system/fs.c` | `zr.system.fs` 目录创建模式 | `zr_vm_lib_system/conf.h` | 否 |
| `ZR_LIBRARY_NATIVE_MODULE_RECORDS_INITIAL_CAPACITY / ZR_LIBRARY_NATIVE_BINDING_ENTRIES_INITIAL_CAPACITY / ZR_LIBRARY_NATIVE_PLUGIN_HANDLES_INITIAL_CAPACITY` | `zr_vm_library/src/zr_vm_library/native_binding.c` | native registry 的模块记录、binding entry 与插件句柄初始容量 | `zr_vm_library/conf.h` | 否 |
| `ZR_LIBRARY_NATIVE_INLINE_ARGUMENT_CAPACITY` | `zr_vm_library/src/zr_vm_library/native_binding_dispatch.c` | native 调用稳定参数数组的 inline 容量阈值 | `zr_vm_library/conf.h` | 否 |
| `ZR_LIBRARY_NATIVE_REGISTRY_ERROR_BUFFER_LENGTH` | `zr_vm_library/src/zr_vm_library/native_binding_internal.h` | native registry 最近错误消息缓冲区长度 | `zr_vm_library/conf.h` | 否 |
| `ZR_CLI_ERROR_BUFFER_LENGTH` | `zr_vm_cli/src/zr_vm_cli/app.c`, `zr_vm_cli/src/zr_vm_cli/compiler.c` | CLI 命令解析与项目编译错误缓冲区长度 | `zr_vm_cli/conf.h` | 否 |
| `ZR_CLI_COLLECTION_INITIAL_CAPACITY / ZR_CLI_SMALL_COLLECTION_INITIAL_CAPACITY / ZR_CLI_COLLECTION_GROWTH_FACTOR` | `zr_vm_cli/src/zr_vm_cli/compiler.c`, `zr_vm_cli/src/zr_vm_cli/project.c` | CLI 模块图、manifest 与字符串列表的初始容量和扩容倍率 | `zr_vm_cli/conf.h` | 否 |
| `ZR_CLI_REPL_LINE_BUFFER_LENGTH / ZR_CLI_REPL_BUFFER_INITIAL_CAPACITY` | `zr_vm_cli/src/zr_vm_cli/repl.c` | CLI REPL 单行输入和累积提交缓冲区上限 | `zr_vm_cli/conf.h` | 否 |
| `ZR_LSP_PROJECT_INDEX_INITIAL_CAPACITY` / `ZR_LSP_ARRAY_INITIAL_CAPACITY` / `ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY` / `ZR_LSP_LARGE_ARRAY_INITIAL_CAPACITY` | `zr_vm_language_server/src/zr_vm_language_server/lsp_interface.c`, `symbol_table.c`, `reference_tracker.c`, `lsp_project*.c`, `wasm_exports.cpp` | LSP 上下文、结果集、索引与跟踪器的初始容量 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_HASH_TABLE_INITIAL_SIZE_LOG2` / `ZR_LSP_HASH_MULTIPLIER` | `zr_vm_language_server/src/zr_vm_language_server/lsp_interface.c`, `incremental_parser.c`, `symbol_table.c`, `reference_tracker.c`, `semantic_analyzer_support.c` | LSP 哈希表初始 log2 与内容/AST 哈希混合因子 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_NATIVE_GENERIC_ARGUMENT_MAX` / `ZR_LSP_NATIVE_GENERIC_TEXT_MAX` | `zr_vm_language_server/src/zr_vm_language_server/lsp_interface_support.c` | native type generic 参数与文本渲染上限 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_MARKDOWN_BUFFER_SIZE` | `zr_vm_language_server/src/zr_vm_language_server/lsp_interface.c` | hover/markdown 拼接缓冲上限 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_SIGNATURE_RANGE_PACK_BASE` | `zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c`, `zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c` | signature help 与 reference tracker 共同使用的 line/column span 打包基数 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_MEMBER_RECURSION_MAX_DEPTH / ZR_LSP_AST_RECURSION_MAX_DEPTH` | `zr_vm_language_server/src/zr_vm_language_server/lsp_interface_support.c`, `semantic_analyzer_support.c`, `lsp_signature_help.c` | LSP 成员收集与 AST 遍历递归保护上限 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_SHORT_TEXT_BUFFER_LENGTH / ZR_LSP_INTEGER_BUFFER_LENGTH / ZR_LSP_TYPE_BUFFER_LENGTH / ZR_LSP_COMPLETION_DETAIL_BUFFER_LENGTH / ZR_LSP_DETAIL_BUFFER_LENGTH / ZR_LSP_TEXT_BUFFER_LENGTH / ZR_LSP_LONG_TEXT_BUFFER_LENGTH / ZR_LSP_HOVER_BUFFER_LENGTH / ZR_LSP_DOCUMENTATION_BUFFER_LENGTH` | `zr_vm_language_server/src/zr_vm_language_server/lsp_interface_support.c`, `lsp_project_features.c`, `semantic_analyzer.c`, `lsp_signature_help.c`, `semantic_analyzer_typecheck.c`, `semantic_analyzer_symbols.c`, `incremental_parser.c` | LSP hover/signature/completion/typecheck 的局部文本与格式化缓冲区上限 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_STDIO_HEADER_BUFFER_LENGTH` | `zr_vm_language_server/stdio/stdio_transport.c` | stdio transport 读取 JSON-RPC 头部行的缓存上限 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_COMMENT_SCAN_LINE_LIMIT / ZR_LSP_COMMENT_BUFFER_LENGTH` | `zr_vm_language_server/src/zr_vm_language_server/lsp_interface_support.c` | LSP 注释回溯扫描行数与注释缓存上限 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_SEMANTIC_TOKEN_INITIAL_CAPACITY` | `zr_vm_language_server/src/zr_vm_language_server/lsp_semantic_tokens.c`, `zr_vm_language_server/stdio/stdio_requests.c`, `zr_vm_language_server/wasm/wasm_exports.cpp` | semantic tokens 中间结果数组在 core/LSP/stdio/wasm 各入口共享的初始容量 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_STDIO_CONTENT_LENGTH_HEADER_PREFIX / ZR_LSP_JSON_RPC_FIELD_JSONRPC / ZR_LSP_JSON_RPC_VERSION / ZR_LSP_JSON_RPC_FIELD_ID / ZR_LSP_JSON_RPC_FIELD_METHOD / ZR_LSP_JSON_RPC_FIELD_PARAMS / ZR_LSP_JSON_RPC_FIELD_RESULT / ZR_LSP_JSON_RPC_FIELD_ERROR / ZR_LSP_JSON_RPC_FIELD_CODE / ZR_LSP_JSON_RPC_FIELD_MESSAGE` | `zr_vm_language_server/stdio/stdio_transport.c`, `zr_vm_language_server/stdio/zr_vm_language_server_stdio.c` | stdio JSON-RPC 报文封装/解析使用的协议字段名、版本字符串与 Content-Length 头前缀 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_FIELD_* / ZR_LSP_METHOD_* / ZR_LSP_MARKUP_KIND_MARKDOWN / ZR_LSP_INSERT_TEXT_FORMAT_KIND_PLAINTEXT / ZR_LSP_INSERT_TEXT_FORMAT_KIND_SNIPPET / ZR_LSP_COMPLETION_TRIGGER_CHARACTER_* / ZR_LSP_SIGNATURE_TRIGGER_CHARACTER_* / ZR_LSP_TEXT_DOCUMENT_SYNC_KIND_INCREMENTAL / ZR_LSP_INSERT_TEXT_FORMAT_PLAIN_TEXT / ZR_LSP_INSERT_TEXT_FORMAT_SNIPPET / ZR_LSP_DIAGNOSTIC_SOURCE_NAME / ZR_LSP_SERVER_NAME / ZR_LSP_SERVER_VERSION` | `zr_vm_language_server/src/zr_vm_language_server/lsp_interface.c`, `zr_vm_language_server/stdio/stdio_json.c`, `zr_vm_language_server/stdio/stdio_documents.c`, `zr_vm_language_server/stdio/stdio_requests.c`, `zr_vm_language_server/wasm/wasm_exports.cpp` | stdio 与 wasm LSP serializer、document sync、initialize capability、completion insertTextFormat 映射、request/notification dispatch 共用的协议字段名、method 名、markup kind、trigger character、sync kind、format tag/enum、diagnostic source 与 server metadata 常量 | `zr_vm_language_server/conf.h` | 否 |
| `EZrLspCompletionItemKind` | `zr_vm_language_server/src/zr_vm_language_server/lsp_interface.c` | completion item `kind` 字段使用的 LSP 协议编号枚举 | `zr_vm_language_server/conf.h` | 否 |
| `EZrLspSymbolKind` | `zr_vm_language_server/src/zr_vm_language_server/lsp_interface_support.c` | symbol information `kind` 字段使用的 LSP 协议编号枚举 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_JSON_RPC_PARSE_ERROR_CODE` / `ZR_LSP_JSON_RPC_INVALID_REQUEST_CODE` | `zr_vm_language_server/stdio/zr_vm_language_server_stdio.c` | stdio JSON-RPC 协议错误码 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_JSON_RPC_METHOD_NOT_FOUND_CODE` / `ZR_LSP_JSON_RPC_INVALID_PARAMS_CODE` | `zr_vm_language_server/stdio/stdio_requests.c` | request dispatch 的 `Method not found` / `Invalid params` 错误码 | `zr_vm_language_server/conf.h` | 否 |

## 兼容 alias

以下旧名字当前仍保留在 public header，但它们不再拥有自己的数值源头：

- `ZR_LIBRARY_MAX_PATH_LENGTH`
  - 现在只是 `ZR_VM_PATH_LENGTH_MAX` 的别名
- `ZR_LIBRARY_BINARY_FILE_EXT`
  - 现在只是 `ZR_VM_BINARY_MODULE_FILE_EXTENSION` 的别名
- `ZR_CLI_SOURCE_HASH_HEX_LENGTH`
  - 现在只是 `ZR_STABLE_HASH_HEX_BUFFER_LENGTH` 的别名

## 模块内共享子系统边界（未进 `conf`）

| 常量 | 原位置 | 含义 | 目标归属 | 是否跨模块 |
| --- | --- | --- | --- | --- |
| `ZR_SEMANTIC_ID_INVALID / ZR_SEMANTIC_ID_FIRST` | `zr_vm_parser/src/zr_vm_parser/semantic.c`, `zr_vm_parser/src/zr_vm_parser/compile_statement.c`, `zr_vm_parser/src/zr_vm_parser/type_system.c` | parser semantic 子系统的 type/symbol/overload/lifetime ID 统一以 `0` 表示 invalid，以 `1` 作为首个可分配 ID | `zr_vm_parser/include/zr_vm_parser/semantic.h` | 否 |

## 明确不纳入 `conf` 的对象

- token / keyword 名称表
- 类型、模块、方法描述表
- 结构体字段名数组，例如 `{"x","y","z"}`
- 帮助文本、诊断全文、type-hint JSON
- `sizeof(...)`、枚举本体值、协议表项数量这类与定义本体强绑定的数据

## 显式豁免清单（本轮新增）

| 常量 / 常量组 | 原位置 | 原因 | 结论 |
| --- | --- | --- | --- |
| `ZrLibMetaMethodDescriptor.maxArgumentCount = (TZrUInt16)0xFFFFu` | `zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c` | `zr.ffi` 的 `ZR_META_CALL` 直接绑定到 `TZrUInt16 maxArgumentCount` 描述符布局，这个值表达“开放位置参数上限”的 ABI/描述表哨兵，而不是运行时策略阈值。 | 明确豁免，保留在描述表初始化处。 |
| `ZR_MATH_MAX_FUNCTIONS / ZR_MATH_MAX_TYPES / ZR_MATH_MAX_HINTS` | `zr_vm_lib_math/src/zr_vm_lib_math/module.c` | 这是模块静态描述数组的编译期表项数量，不是跨模块协议，也不是运行时调参项。 | 明确豁免，保留在模块装配文件。 |
| `Matrix4x4` 维度和构造实参个数相关的 `16` / `4` | `zr_vm_lib_math/src/zr_vm_lib_math/matrix4x4.c`, `zr_vm_lib_math/src/zr_vm_lib_math/matrix4x4_registry.c` | 这些值直接编码了 `4x4` 类型的定义本体、字段布局、构造签名和算法矩阵维度。把它们抽到 `conf` 会削弱类型定义的局部可读性。 | 明确豁免，作为类型形状常量留在 `Matrix4x4` 实现与注册处。 |
| 生产态 `ZrCore_GlobalState_New(..., uniqueNumber, ...)` seed 字面量 `0x1234567890ABCDEF` / `0x5A525F434C495F42ULL` | `zr_vm_library/src/zr_vm_library/common_state.c`, `zr_vm_cli/src/zr_vm_cli/project.c` | 第三个参数只流向 `ZrCore_HashSeed_Create(...)`，语义是调用点身份盐值，不是 ABI、协议号、容量阈值或运行时预算。测试里也广泛存在任意 `uniqueNumber`。 | 明确豁免，保留为调用点身份标签；后续如要统一，仅单独建立 seed/tag 约定层，不并入 `conf`。 |
| wasm bridge 响应 envelope/status 键 `"success"` / `"error"` / `"data"` / `"updated"` / `"closed"` | `zr_vm_language_server/wasm/wasm_exports.cpp` | 这些键属于 wasm 对 JS 暴露的桥接层响应包装，不是 LSP payload 字段，也不是 JSON-RPC protocol key；其中 `updated` / `closed` 还是 bridge 私有操作状态。 | 明确豁免，保留在 wasm bridge 实现，不进入 `zr_vm_language_server/conf.h`。 |
| `ZR_MEMBER_PARAMETER_COUNT_UNKNOWN` 与 native metadata 默认值 `-1` | `zr_vm_parser/src/zr_vm_parser/type_inference_native.c` | 这是 native module metadata schema 中“parameterCount 未知”的本地哨兵，已在实现中命名；默认值 `-1` 只是对应 schema 读取路径，不是 parser 通用容量或策略阈值。 | 明确豁免，保留在 native metadata helper 内，不进入 parser/common `conf`。 |
| `ZR_HASH_SEED_BUFFER_SIZE = (sizeof(TZrUInt64) << 2)` | `zr_vm_core/src/zr_vm_core/hash.c` | 哈希 seed helper 明确将 `timestamp / global address / helper address / uniqueNumber` 四个 `TZrUInt64` 组装为输入缓冲；这是 helper 局部布局形状，不是共享 hash base、协议或预算常量。 | 明确豁免，保留在 `hash.c` 内作为局部 helper 布局常量。 |
| UTF-8 编码 helper 掩码/前缀位宽 `0x7fffffffu` / `0x7fu` / `0x3fu` 与注释里的 `110/1110/11110` | `zr_vm_core/include/zr_vm_core/string.h` | 这些值直接编码 UTF-8 逐字节拼装算法的位掩码和前缀形状，属于字符编码实现本体，不是运行时策略、容量或协议阈值。 | 明确豁免，保留在 UTF-8 helper 内。 |
| `MAX_DELTA ((256UL << ((sizeof(...offset) - 1) * 8)) - 1)` | `zr_vm_core/src/zr_vm_core/closure.c` | 这是 closure `toBeClosedValueOffset` 字段宽度推导出的链表跨段步长上限，属于布局/位宽绑定的局部辅助公式，不是共享策略常量。 | 明确豁免，保留在 `closure.c` 内。 |
| `ZR_TYPE_INFERENCE_GENERIC_START_NOT_FOUND / ZR_TYPE_INFERENCE_OVERLOAD_SCORE_INCOMPATIBLE / ZR_META_COMPARE_RESULT_*` | `zr_vm_parser/src/zr_vm_parser/type_inference_core.c`; `zr_vm_core/src/zr_vm_core/meta.c` | 这批 helper-local sentinel/result code 已在各自文件内命名，但语义仍只属于局部算法控制流，不提升到 `zr_vm_common`。 | 明确豁免为“文件私有常量”，继续保留在实现文件内。 |
| `ZR_LSP_STDIO_HEX_DIGIT_INVALID / ZR_LSP_PROJECT_HEX_DIGIT_INVALID / ZR_LSP_SIGNATURE_BINDING_INDEX_NONE / ZR_LSP_RANGE_SPAN_SCORE_INVALID / ZR_LSP_SYMBOL_POSITION_COMPARE_* / ZR_LSP_SEMANTIC_TOKEN_COMPARE_* / ZR_LSP_SEMANTIC_TOKEN_TYPE_UNKNOWN / ZR_EXCEPTION_HANDLER_STACK_* / ZR_GC_IGNORE_REGISTRY_* / ZR_LSP_NATIVE_MODULE_COMPLETION_MAX_DEPTH` | `zr_vm_language_server/stdio/stdio_documents.c`; `zr_vm_language_server/src/zr_vm_language_server/lsp_project.c`; `lsp_signature_help.c`; `reference_tracker.c`; `symbol_table.c`; `lsp_semantic_tokens.c`; `semantic_analyzer.c`; `zr_vm_core/src/zr_vm_core/execution_control.c`; `gc_object.c` | 这批值分别表达 URI 解码非法输入、binding/span 未命中、局部 compare tri-state、semantic-token unknown、局部动态数组扩容参数和 native completion 深度保护，都是 helper-local 控制流/容量语义，不形成跨文件 contract。 | 明确豁免为“文件私有常量”，继续保留在各自实现文件。 |
| `ZR_HASH_SET_CAPACITY_GROWTH_FACTOR / ZR_HASH_SET_MAX_LOAD_NUMERATOR / ZR_HASH_SET_MAX_LOAD_DENOMINATOR / ZR_SEMIR_TYPE_TABLE_DEFAULT_ENTRY_COUNT / ZR_SEMIR_OWNERSHIP_STATE_TABLE_CAPACITY / ZR_SEMIR_DEOPT_ID_FIRST` | `zr_vm_core/include/zr_vm_core/hash_set.h`; `zr_vm_core/src/zr_vm_core/hash_set.c`; `zr_vm_parser/src/zr_vm_parser/compiler_semir.c` | 这批值分别表达 hash-set 的局部容量策略和 SemIR 构建 helper 的默认基项/临时表容量/deopt id 起始值，语义仍局限于单模块或单文件实现约定，不形成更高层共享协议。 | 明确豁免为“文件私有常量”，继续保留在各自实现文件。 |
| `exception_compute_instruction_offset()` 的 `0` fallback，与 `value_to_int64 / value_to_uint64 / value_to_double` 的 `0 / 0.0` fallback | `zr_vm_core/src/zr_vm_core/exception.c`; `zr_vm_core/src/zr_vm_core/execution_numeric.c` | 前者表达异常栈里 instruction offset 的自然下界 `0`，当前没有独立 “offset none” contract；后者的调用面都先做 numeric/bool 类型约束，`0 / 0.0` 只是防御式自然零值，不承担“失败/未命中” sentinel 语义。 | 明确豁免，保留为自然零值返回，不提升到 common/module conf。 |

## 剩余定义绑定豁免

| 常量 / 常量组 | 原位置 | 含义 | 当前判定 |
| --- | --- | --- | --- |
| `ZR_FIRST_RESERVED = 256` | `zr_vm_parser/include/zr_vm_parser/lexer.h` | 单字符 token 与保留 token 枚举空间的边界基数 | 词法 token 编码定义本体，保留豁免 |
| `EZrLspSemanticTokenType` 顺序编号 `0..10` | `zr_vm_language_server/src/zr_vm_language_server/lsp_semantic_tokens.c` | semantic token type 名称表的协议顺序编号 | semantic-token 枚举定义本体，保留豁免 |
| `ZR_MEMBER_PARAMETER_COUNT_UNKNOWN` | `zr_vm_parser/src/zr_vm_parser/type_inference_native.c` | native metadata schema 的“parameterCount 未知”本地哨兵 | 本地 schema/helper 绑定，保留豁免 |

## 当前 audit backlog

当前 `audit backlog` 已清空；本轮按 inventory 顺序完成了 `io.c` patch feature gates、`reference_tracker.c` 的 range-pack base、`meta/execution_control/execution_dispatch` 的元调用约定、`reflection.c` 的 member/method hash band，以及 `type_inference.c` 的整数 range bounds 收敛。

最新一次 `python scripts/audit_magic_constants.py --json` 结果为 `201 migrated checks / 0 migrated failures / 0 backlog checks / 0 backlog hits`。此前 parser 这批 `TZrUInt32 all-ones` 哨兵热点已经并入现有 `ZR_PARSER_*_NONE` 边界；随后补上了 `writer.c` 的 header widths，以及 `compiler_extern_* / compiler_instruction / compiler_locals / compile_expression_call / compile_statement` 里剩余的 raw all-ones 哨兵。本轮继续把 `exception.c` 的 raw invalid thread-status `-1`、parser 跨文件共享的 `EOZ` 与 `labelId none` 一并并入已有命名边界；再向下收了一层 parser 内部跨 3 个文件复用的 signed helper index sentinel，统一为 `ZR_PARSER_I32_NONE`。按照你这次的新要求，剩余 helper-local 裸值继续在模块内命名：除了 `type_inference_core.c` 的 `ZR_TYPE_INFERENCE_GENERIC_START_NOT_FOUND / ZR_TYPE_INFERENCE_OVERLOAD_SCORE_INCOMPATIBLE`、`meta.c` 的 `ZR_META_COMPARE_RESULT_*` 外，这一轮又补上了 LSP URI 解码非法 hex sentinel、signature-help binding-index none、reference span invalid score、symbol/semantic-token compare tri-state、semantic-token unknown type、exception-handler/GC ignore registry 本地容量策略，以及 native module completion 深度保护；继续向 `zr_vm_core / zr_vm_parser` 下钻后，又补上了 hash-set 的局部扩容/负载因子常量，以及 SemIR helper 的默认类型表基项、ownership-state 表容量和 deopt id 起始值。再往下复扫时，确认 `semantic.c / compile_statement.c / type_system.c` 已经形成 parser 语义层共享的 ID contract，于是把 `invalid=0 / first=1` 收敛为 `semantic.h` 里的 `ZR_SEMANTIC_ID_INVALID / ZR_SEMANTIC_ID_FIRST`，不再在实现里散写 raw `0/1`。本轮继续下钻 `SemIR deopt` 链路后，确认 `compiler_semir -> compiler_quickening -> writer/io -> runtime clone` 已经形成跨 parser/core 的“无 deopt=0”共享 contract，因此把 raw `0` 升格为 `zr_runtime_sentinel_conf.h` 里的 `ZR_RUNTIME_SEMIR_DEOPT_ID_NONE`；同时把 `exception.c` 里 C++ fallback 的 `status == 0` 清理为已有 `ZR_THREAD_STATUS_FINE`。这次再转向 `backend_aot.c / compiler_semir.c / execution_control.c` 后，确认没有新的跨模块共享 contract：`compiler_semir.c` 里剩余 `0` 属于默认 type-table 回退索引和 ownership-state 首槽索引，`execution_control.c` 的 `return 0` 只是 scope cleanup helper 的零关闭数，`backend_aot.c` 的 `0` 则是空计数、flatten 根函数索引与空 embedded blob 占位字节，因此都只收敛为文件私有常量；而生成模板里的 `"return 0;"` 继续视为 AOT 生成代码文本，不当作宿主实现里的新共享 sentinel。复扫 `zr_vm_core / zr_vm_parser` 的 helper hash base 后，仍未发现新的跨文件裸 hash base 候选，当前剩余的 hash helper 只保留 `hash.c` 的局部布局豁免。针对这次额外收窄的候选，`compiler_locals.c` 里真正属于 constant-index 语义的 `0` 回退已经并入 `ZR_PARSER_INDEX_NONE`；`exception.c` 里“无源码行”则明确收敛为文件私有 `ZR_EXCEPTION_SOURCE_LINE_NONE`，而 `exception_compute_instruction_offset()` 与 `execution_numeric.c` 的 `0 / 0.0` 返回则确认为自然零值，继续显式豁免，不误升格为新的共享 sentinel contract。

## 新一轮人工扫描候选（未进 audit）

| 候选 | 原位置 | 含义 | 目标归属 | 当前状态 |
| --- | --- | --- | --- | --- |
| helper-local parse cursor / score / compare result | `zr_vm_parser/src/zr_vm_parser/type_inference_core.c`, `zr_vm_core/src/zr_vm_core/meta.c` | 这批值已改为文件私有命名常量，不再是裸值；语义仍只属于局部 helper | 文件私有常量 | 已命名并保留豁免 |
| LSP/core helper-local sentinel / compare / growth / depth | `zr_vm_language_server/stdio/stdio_documents.c`, `zr_vm_language_server/src/zr_vm_language_server/lsp_project.c`, `lsp_signature_help.c`, `reference_tracker.c`, `symbol_table.c`, `lsp_semantic_tokens.c`, `semantic_analyzer.c`, `zr_vm_core/src/zr_vm_core/execution_control.c`, `gc_object.c` | 这批值已收敛为各文件私有命名常量；其中 semantic-token enum ordinal、schema/layout 绑定值继续明确豁免，没有提升到 common/module conf | 文件私有常量 | 已命名并保留豁免 |
| core/parser helper-local growth and semir sentinels | `zr_vm_core/include/zr_vm_core/hash_set.h`, `zr_vm_core/src/zr_vm_core/hash_set.c`, `zr_vm_parser/src/zr_vm_parser/compiler_semir.c` | hash-set 扩容/负载因子与 SemIR helper 的默认基项、ownership-state 表容量、deopt id 起始值已收敛为本地命名常量；仍然只服务局部实现，不形成共享 contract | 文件私有常量 | 已命名并保留豁免 |
| parser semantic shared id contract | `zr_vm_parser/src/zr_vm_parser/semantic.c`, `zr_vm_parser/src/zr_vm_parser/compile_statement.c`, `zr_vm_parser/src/zr_vm_parser/type_system.c` | 这批 `0/1` 不再视作零散 helper-local 裸值，而是确认形成了语义层共享边界：`0` 表示 invalid，`1` 表示首个可分配 ID，因此收进 `semantic.h` 作为子系统公共 contract，而不是继续散在实现文件或提升到 `zr_parser_conf.h` | 模块内共享头常量 | 已命名并入 `semantic.h` |
| SemIR deopt none shared contract | `zr_vm_parser/src/zr_vm_parser/compiler_semir.c`, `zr_vm_parser/src/zr_vm_parser/compiler_quickening.c`, `zr_vm_parser/src/zr_vm_parser/writer.c`, `zr_vm_core/src/zr_vm_core/io.c`, `zr_vm_core/src/zr_vm_core/io_runtime.c` | 复扫确认 `deoptId=0` 已经不只是 `compiler_semir.c` 内部实现细节，而是穿过 quickening、writer/io 与 runtime clone 的共享“无 deopt”语义，因此提升为 `ZR_RUNTIME_SEMIR_DEOPT_ID_NONE`；仍留在本地的只有 `ZR_SEMIR_DEOPT_ID_FIRST` 这种构建期 helper 起始值 | `zr_runtime_sentinel_conf.h` | 已命名并提升为共享 contract |
| backend_aot / compiler_semir / execution_control helper-local zero values | `zr_vm_parser/src/zr_vm_parser/backend_aot.c`, `zr_vm_parser/src/zr_vm_parser/compiler_semir.c`, `zr_vm_core/src/zr_vm_core/execution_control.c` | 继续复扫后确认这批 `0` 只表达本文件内的空计数、根函数索引、默认 table index、空 embedded blob byte、以及 scope-cleanup 零关闭数，没有形成新的 parser/core 共享 contract，因此全部留在实现文件内命名；AOT 模板字符串里的 `return 0;` 继续视为生成代码文本豁免 | 文件私有常量 | 已命名并保留豁免 |
| core/parser helper hash base 复扫 | `zr_vm_core/src/zr_vm_core/hash.c`, `reflection.c`, `constant_reference.c` 及 parser 相关 helper | 复查后除已迁移的 reflection hash base / salt 与 constant-reference step tags 外，没有新的跨文件裸 hash base/salt 候选；剩余 `ZR_HASH_SEED_BUFFER_SIZE` 仍属局部布局豁免 | 无新增共享 contract | 已判定无新增迁移项 |
## 审计脚本

运行方式：

```powershell
python scripts/audit_magic_constants.py
python scripts/audit_magic_constants.py --json
```

脚本语义：

- `migrated` 区检查本批已经迁移的旧写法是否重新出现
- `backlog` 区列出仍允许留在模块内、等待后续批次继续收敛的候选
- 只要 `migrated` 区出现命中，脚本就返回非零退出码
- `backlog` 区当前只做报告，不作为失败条件

## 后续批次建议

1. 维持 `ffi/math`、`lexer/semantic-token`、`GlobalState uniqueNumber seed` 这批“定义绑定或身份标签豁免”在 inventory 中显式留档，不再反复进入迁移 backlog。
2. 后续如再发现 parser/runtime/LSP 的哨兵、buffer、容量层级、扩容倍率或协议保护值，优先复用既有 `zr_vm_common` 常量边界，不新建同义名字。
3. 下一批人工扫描建议按这个顺序推进：`zr_vm_core / zr_vm_parser` 新出现的局部 signed sentinel 是否继续扩展为共享 contract -> 其余模块新增 helper sentinel/hash base 命中统一先建账再迁移。
4. 新增迁移项时，先在 inventory 建账，再同步到 `scripts/audit_magic_constants.py` 的 `migrated` 区，避免裸字面量回流。
