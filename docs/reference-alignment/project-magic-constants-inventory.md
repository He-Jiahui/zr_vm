---
related_code:
  - zr_vm_common/include/zr_vm_common.h
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
  - zr_vm_core/src/zr_vm_core/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/module_loader.c
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/compiler_instruction.c
  - zr_vm_parser/src/zr_vm_parser/compiler_state.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression_types.c
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
  - zr_vm_core/src/zr_vm_core/meta.c
  - zr_vm_core/src/zr_vm_core/value.c
  - zr_vm_core/src/zr_vm_core/reflection.c
  - zr_vm_core/src/zr_vm_core/module_prototype.c
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
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio.c
  - zr_vm_language_server/stdio/stdio_documents.c
  - zr_vm_language_server/wasm/wasm_exports.cpp
  - scripts/audit_magic_constants.py
implementation_files:
  - zr_vm_common/include/zr_vm_common.h
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
  - zr_vm_cli/include/zr_vm_cli/conf.h
  - zr_vm_cli/src/zr_vm_cli/project.c
  - zr_vm_cli/src/zr_vm_cli/app.c
  - zr_vm_cli/src/zr_vm_cli/compiler.c
  - zr_vm_cli/src/zr_vm_cli/project.h
  - zr_vm_cli/src/zr_vm_cli/repl.c
  - zr_vm_cli/src/zr_vm_cli/command.h
  - zr_vm_core/src/zr_vm_core/gc_mark.c
  - zr_vm_core/src/zr_vm_core/debug.c
  - zr_vm_core/src/zr_vm_core/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/meta.c
  - zr_vm_core/src/zr_vm_core/value.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/execution_meta_access.c
  - zr_vm_core/src/zr_vm_core/constant_reference.c
  - zr_vm_core/src/zr_vm_core/gc_object.c
  - zr_vm_core/src/zr_vm_core/gc_sweep.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_core/src/zr_vm_core/reflection.c
  - zr_vm_core/src/zr_vm_core/module_prototype.c
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/compiler_state.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression_types.c
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
  - zr_vm_language_server/stdio/stdio_requests.c
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
| `ZR_GC_WORK_TO_MEMORY_BYTES` | `zr_vm_core/src/zr_vm_core/gc_internal.h` | GC work 到 debt byte 的换算倍率 | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_SWEEP_SLICE_BUDGET_MAX` | `zr_vm_core/src/zr_vm_core/gc_internal.h` | sweep 单步最大 slice 预算 | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_DEFAULT_PAUSE_BUDGET` | `zr_vm_core/src/zr_vm_core/gc_internal.h` | 默认 pause budget | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_DEFAULT_SWEEP_SLICE_BUDGET` | `zr_vm_core/src/zr_vm_core/gc_internal.h` | 默认 sweep slice budget | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_FINALIZER_BATCH_MAX` | `zr_vm_core/src/zr_vm_core/gc_internal.h` | 单批 finalizer 最大个数 | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_FINALIZER_WORK_COST` | `zr_vm_core/src/zr_vm_core/gc_internal.h` | finalizer work 成本折算 | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_GRAY_LIST_DUPLICATE_SCAN_LIMIT` | `zr_vm_core/src/zr_vm_core/gc_mark.c` | gray list 重复检测保护上限 | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_PROPAGATE_ALL_ITERATION_LIMIT` | `zr_vm_core/src/zr_vm_core/gc_mark.c` | mark propagate 全量循环保护上限 | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_RUN_UNTIL_STATE_ITERATION_LIMIT` | `zr_vm_core/src/zr_vm_core/gc_cycle.c` | `run_until_state` 循环保护上限 | `zr_gc_internal_conf.h` | 是 |
| `ZR_GC_GENERATIONAL_FULL_SWEEP_ITERATION_LIMIT` | `zr_vm_core/src/zr_vm_core/gc_cycle.c` | generational full sweep 循环保护上限 | `zr_gc_internal_conf.h` | 是 |
| `ZR_PARSER_LEXER_BUFFER_INITIAL_SIZE` | `zr_vm_parser/src/zr_vm_parser/lexer.c` | lexer 初始缓冲大小 | `zr_parser_conf.h` | 是 |
| `ZR_PARSER_MAX_CONSECUTIVE_ERRORS` | `zr_vm_parser/src/zr_vm_parser/parser.c` | parser 最大连续错误数 | `zr_parser_conf.h` | 是 |
| `ZR_PARSER_MAX_RECOVERY_SKIP_TOKENS` | `zr_vm_parser/src/zr_vm_parser/parser.c` | parser 恢复阶段最大跳过 token 数 | `zr_parser_conf.h` | 是 |
| `ZR_PARSER_COMPILE_TIME_RUNTIME_SAFE_MAX_DEPTH` | `zr_vm_parser/src/zr_vm_parser/compiler_instruction.c` | compile-time value 投影到 runtime 的递归深度上限 | `zr_parser_conf.h` | 是 |
| `ZR_PARSER_RECURSIVE_MEMBER_LOOKUP_MAX_DEPTH` | `zr_vm_parser/src/zr_vm_parser/compile_expression_types.c`, `zr_vm_parser/src/zr_vm_parser/type_inference_core.c` | 类型原型/成员递归查找保护上限 | `zr_parser_conf.h` | 是 |
| `ZR_PARSER_DYNAMIC_CAPACITY_GROWTH_FACTOR / ZR_PARSER_INITIAL_CAPACITY_PAIR / ZR_PARSER_INITIAL_CAPACITY_TINY / ZR_PARSER_INITIAL_CAPACITY_SMALL / ZR_PARSER_INITIAL_CAPACITY_MEDIUM / ZR_PARSER_INITIAL_CAPACITY_LARGE / ZR_PARSER_INSTRUCTION_INITIAL_CAPACITY` | `zr_vm_parser/src/zr_vm_parser/compiler_state.c`, `compiler_class.c`, `compiler_struct.c`, `compiler_interface.c`, `compiler_extern_declaration.c`, `type_inference_import_metadata.c`, `type_inference_native.c`, `type_system.c`, `semantic.c`, `compile_time_executor_support.c`, `compiler_closure.c`, `compiler_bindings.c`, `parser_expression_primary.c`, `ast.c`, `lexer.c` | parser 编译器状态、局部 collection 初始容量分层，以及 AST/lexer 共享动态扩容倍率 | `zr_parser_conf.h` | 是 |
| `ZR_PARSER_ARRAY_SIZE_BUFFER_LENGTH / ZR_PARSER_INTEGER_BUFFER_LENGTH / ZR_PARSER_GENERATED_NAME_BUFFER_LENGTH / ZR_PARSER_TYPE_NAME_BUFFER_LENGTH / ZR_PARSER_DETAIL_BUFFER_LENGTH / ZR_PARSER_ERROR_BUFFER_LENGTH / ZR_PARSER_TEXT_BUFFER_LENGTH / ZR_PARSER_DECLARATION_BUFFER_LENGTH` | `zr_vm_parser/src/zr_vm_parser/type_inference_core.c`, `compiler_class.c`, `compiler_struct.c`, `type_inference_native.c`, `compile_time_executor_support.c`, `type_inference.c`, `parser_state.c`, `writer_intermediate.c`, `compiler_extern_helpers.c`, `compile_statement.c`, `type_inference_generic_calls.c`, `type_inference_passing_modes.c`, `compiler_class_support.c`, `compiler_generic_semantics.c`, `compile_expression.c`, `compile_expression_call.c`, `compile_time_executor.c`, `parser_declarations.c`, `parser_extern.c`, `parser_expression_primary.c`, `lexer.c` | parser synthetic name、类型名、细节文本、通用诊断、声明文本、snippet 与 typed metadata 输出的共享缓冲区上限 | `zr_parser_conf.h` | 是 |
| `ZR_RUNTIME_SMALL_TEXT_BUFFER_LENGTH / ZR_RUNTIME_MEMBER_NAME_BUFFER_LENGTH / ZR_RUNTIME_TYPE_NAME_BUFFER_LENGTH / ZR_RUNTIME_DIAGNOSTIC_BUFFER_LENGTH / ZR_RUNTIME_QUALIFIED_NAME_BUFFER_LENGTH / ZR_RUNTIME_ERROR_BUFFER_LENGTH / ZR_RUNTIME_REFLECTION_FORMAT_BUFFER_LENGTH / ZR_RUNTIME_OBJECT_PROTOTYPE_INITIAL_CAPACITY / ZR_RUNTIME_OBJECT_PROTOTYPE_GROWTH_FACTOR / ZR_RUNTIME_PROTOTYPE_INHERIT_INITIAL_CAPACITY` | `zr_vm_core/src/zr_vm_core/debug.c`, `execution_dispatch.c`, `meta.c`, `value.c`, `reflection.c`, `object.c`, `module_prototype.c` | core 运行时错误、反射格式化、对象原型成员表扩容策略与原型继承解析的共享 buffer/容量上限 | `zr_runtime_limits_conf.h` | 是 |
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
| `ZR_RUNTIME_INVALID_POINTER_GUARD_LOW_BOUND` | `zr_vm_core/src/zr_vm_core/gc_object.c`, `zr_vm_core/src/zr_vm_core/gc_sweep.c`, `zr_vm_core/src/zr_vm_core/state.c` | GC 与状态析构阶段共享的低地址无效指针保护下界 | `zr_runtime_sentinel_conf.h` | 是 |
| `ZR_RUNTIME_REFLECTION_ENTRY_HASH_SALT` | `zr_vm_core/src/zr_vm_core/reflection.c` | 将模块 `__entry` 反射对象与普通成员哈希空间隔离的盐值 | `zr_runtime_sentinel_conf.h` | 是 |

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
| `ZR_CLI_SOURCE_HASH_HEX_LENGTH` | `zr_vm_cli/src/zr_vm_cli/compiler.c`, `zr_vm_cli/src/zr_vm_cli/project.h` | CLI 增量 manifest 的源码哈希缓冲区长度 | `zr_vm_cli/conf.h` | 否 |
| `ZR_CLI_COLLECTION_INITIAL_CAPACITY / ZR_CLI_SMALL_COLLECTION_INITIAL_CAPACITY / ZR_CLI_COLLECTION_GROWTH_FACTOR` | `zr_vm_cli/src/zr_vm_cli/compiler.c`, `zr_vm_cli/src/zr_vm_cli/project.c` | CLI 模块图、manifest 与字符串列表的初始容量和扩容倍率 | `zr_vm_cli/conf.h` | 否 |
| `ZR_CLI_REPL_LINE_BUFFER_LENGTH / ZR_CLI_REPL_BUFFER_INITIAL_CAPACITY` | `zr_vm_cli/src/zr_vm_cli/repl.c` | CLI REPL 单行输入和累积提交缓冲区上限 | `zr_vm_cli/conf.h` | 否 |
| `ZR_LSP_PROJECT_INDEX_INITIAL_CAPACITY` / `ZR_LSP_ARRAY_INITIAL_CAPACITY` / `ZR_LSP_SMALL_ARRAY_INITIAL_CAPACITY` / `ZR_LSP_LARGE_ARRAY_INITIAL_CAPACITY` | `zr_vm_language_server/src/zr_vm_language_server/lsp_interface.c`, `symbol_table.c`, `reference_tracker.c`, `lsp_project*.c`, `wasm_exports.cpp` | LSP 上下文、结果集、索引与跟踪器的初始容量 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_HASH_TABLE_INITIAL_SIZE_LOG2` / `ZR_LSP_HASH_MULTIPLIER` | `zr_vm_language_server/src/zr_vm_language_server/lsp_interface.c`, `incremental_parser.c`, `symbol_table.c`, `reference_tracker.c`, `semantic_analyzer_support.c` | LSP 哈希表初始 log2 与内容/AST 哈希混合因子 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_NATIVE_GENERIC_ARGUMENT_MAX` / `ZR_LSP_NATIVE_GENERIC_TEXT_MAX` | `zr_vm_language_server/src/zr_vm_language_server/lsp_interface_support.c` | native type generic 参数与文本渲染上限 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_MARKDOWN_BUFFER_SIZE` | `zr_vm_language_server/src/zr_vm_language_server/lsp_interface.c` | hover/markdown 拼接缓冲上限 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_SIGNATURE_RANGE_PACK_BASE` | `zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c` | signature help 回退位置打包基数 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_MEMBER_RECURSION_MAX_DEPTH / ZR_LSP_AST_RECURSION_MAX_DEPTH` | `zr_vm_language_server/src/zr_vm_language_server/lsp_interface_support.c`, `semantic_analyzer_support.c`, `lsp_signature_help.c` | LSP 成员收集与 AST 遍历递归保护上限 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_SHORT_TEXT_BUFFER_LENGTH / ZR_LSP_INTEGER_BUFFER_LENGTH / ZR_LSP_TYPE_BUFFER_LENGTH / ZR_LSP_COMPLETION_DETAIL_BUFFER_LENGTH / ZR_LSP_DETAIL_BUFFER_LENGTH / ZR_LSP_TEXT_BUFFER_LENGTH / ZR_LSP_LONG_TEXT_BUFFER_LENGTH / ZR_LSP_HOVER_BUFFER_LENGTH / ZR_LSP_DOCUMENTATION_BUFFER_LENGTH` | `zr_vm_language_server/src/zr_vm_language_server/lsp_interface_support.c`, `lsp_project_features.c`, `semantic_analyzer.c`, `lsp_signature_help.c`, `semantic_analyzer_typecheck.c`, `semantic_analyzer_symbols.c`, `incremental_parser.c` | LSP hover/signature/completion/typecheck 的局部文本与格式化缓冲区上限 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_STDIO_HEADER_BUFFER_LENGTH` | `zr_vm_language_server/stdio/stdio_transport.c` | stdio transport 读取 JSON-RPC 头部行的缓存上限 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_COMMENT_SCAN_LINE_LIMIT / ZR_LSP_COMMENT_BUFFER_LENGTH` | `zr_vm_language_server/src/zr_vm_language_server/lsp_interface_support.c` | LSP 注释回溯扫描行数与注释缓存上限 | `zr_vm_language_server/conf.h` | 否 |
| `ZR_LSP_SEMANTIC_TOKEN_INITIAL_CAPACITY` | `zr_vm_language_server/src/zr_vm_language_server/lsp_semantic_tokens.c`, `zr_vm_language_server/stdio/stdio_requests.c`, `zr_vm_language_server/wasm/wasm_exports.cpp` | semantic tokens 中间结果数组在 core/LSP/stdio/wasm 各入口共享的初始容量 | `zr_vm_language_server/conf.h` | 否 |
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

## 剩余定义绑定豁免

| 常量 / 常量组 | 原位置 | 含义 | 当前判定 |
| --- | --- | --- | --- |
| `ZR_FIRST_RESERVED = 256` | `zr_vm_parser/include/zr_vm_parser/lexer.h` | 单字符 token 与保留 token 枚举空间的边界基数 | 词法 token 编码定义本体，保留豁免 |
| `EZrLspSemanticTokenType` 顺序编号 `0..10` | `zr_vm_language_server/src/zr_vm_language_server/lsp_semantic_tokens.c` | semantic token type 名称表的协议顺序编号 | semantic-token 枚举定义本体，保留豁免 |

## 当前 audit backlog

最新一次 `python scripts/audit_magic_constants.py --json` 结果为 `109 migrated checks / 0 migrated failures / 0 backlog checks / 0 backlog hits`。本轮按顺序完成了 parser 剩余 buffer 与 `4/8/16/*2`、`zr_vm_core/object.c` prototype 容量策略、以及 LSP `stdio_transport.c` / `stdio_requests.c` / `wasm_exports.cpp` 的迁移；`migrated` 继续负责拦截这些已经完成迁移的常量回流，而显式定义绑定豁免仍只在本文档留档，不转成失败项。

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
2. 后续如新增 parser/runtime/LSP 的 buffer、容量层级、扩容倍率或协议保护值，先在 inventory 建账，再同步到 `scripts/audit_magic_constants.py` 的 `migrated` 区，避免裸字面量回流。
