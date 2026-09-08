---
related_code:
  - zr_vm_common/include/zr_vm_common.h
  - zr_vm_core/include/zr_vm_core.h
  - zr_vm_parser/include/zr_vm_parser.h
  - zr_vm_library/include/zr_vm_library.h
  - zr_vm_language_server/include/zr_vm_language_server.h
  - zr_vm_rust_binding/include/zr_vm_rust_binding.h
implementation_files:
  - zr_vm_core/src/zr_vm_core
  - zr_vm_parser/src/zr_vm_parser
  - zr_vm_library/src/zr_vm_library
  - zr_vm_language_server/src/zr_vm_language_server
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/core-runtime/index.md
  - docs/parser-and-semantics/index.md
  - docs/library-and-builtins/index.md
tests:
  - tests/core/test_session_checkpoint.c
  - tests/parser/test_semantic_query.c
  - tests/library/test_native_registry_descriptor_invalidation.c
doc_type: api-reference
---

# API 索引

## Core

| 头文件 | API 族 | 说明 |
| --- | --- | --- |
| `global.h` / `state.h` | `ZrCore_GlobalState_*`, `ZrCore_State_*` | global/state 创建、loader、主线程和 mutator |
| `value.h` / `string.h` | `ZrCore_Value_*`, `ZrCore_String_*` | 值初始化、复制、字符串和 ownership release |
| `object.h` / `module.h` | `ZrCore_Object_*`, `ZrCore_Module_*` | 原型、成员/index、导出和 module cache |
| `gc.h` | `ZrCore_GarbageCollector_*`, `ZrCore_Gc_*` | full/step、safepoint、barrier、native pin |
| `exception.h` | `ZrCore_Exception_*` | try/throw/catch、异常归一化和诊断 |
| `call_binding.h` | `ZrCore_CallBinding_*` | contract 检查、resolve、generation 和 known call |
| `execution.h` | `ZrCore_Execute`, `ZrCore_Execution_*` | bytecode 驱动、返回值物化 |
| `type_layout.h` | `ZrCore_TypeLayout_*` | inline layout、copy/drop、GC scan |
| `reflection.h` | `ZrCore_Reflection_*` | TypeId/token/member/constructor 查询 |

## Parser

| 头文件 | API 族 | 说明 |
| --- | --- | --- |
| `lexer.h` / `parser.h` | `ZrParser_State_*`, `ZrParser_Parse*` | token、AST 和增量边界 |
| `compiler.h` | `ZrParser_Compiler_*` | source -> function/bytecode |
| `canonical_type.h` | `ZrParser_CanonicalType_*` | TypeId、owner、generic 参数 |
| `cfg.h` | `ZrParser_Cfg_*` | block/edge/reachability |
| `semantic_ir.h` | `ZrParser_SemanticIr_*`, `SemanticFlow_*` | Place/Value/Loan/cleanup/bounds facts |
| `semantic_query.h` | `ZrParser_SemanticQuery_*` | type/call/reference/LSP 查询 |
| `writer.h` | `ZrParser_Writer_*` | `.zro/.zri/.zrs` 和 AOT 输出 |
| `test_contract.h` | `ZrParser_TestManifest_*` | Test phase manifest encode/decode |
| `compile_tool.h` | `ZrParser_CompileTool_*`, `ZrParser_CompileToolArtifact_*` | compile-only provider descriptor、contract hash、`.zrm` build dependency |
| `comptime_contract.h` | `ZrParser_ComptimeEffect_IsAllowed`、`ZrParser_ComptimeBudget_Init`、`ZrParser_ComptimeBudget_TryConsume` | effect policy、fuel/heap/diagnostic budget |
| `declaration_transform_contract.h` | `ZrParser_DeclarationPatch_*` | immutable view、Patch 校验和错误名 |

## Library/provider

| 头文件/模块 | API 族 | 说明 |
| --- | --- | --- |
| `native_registry.h` | `ZrLibrary_NativeRegistry_*` | descriptor attach/register/find/phase |
| `project.h` | `ZrLibrary_Project_*`, `ModuleSpecifier_*` | `.zrp`、module identity、依赖和路径 |
| `file.h` | `ZrLibrary_File_*` | 文件、目录、stream handle、source loader |
| `zrm.h` | `ZrLibrary_Zrm_*` | package/archive entry |
| provider `module.h` | `ZrVmLib*_*` | descriptor 和注册入口 |
| `aot_runtime.h` | `ZrLibrary_AotRuntime_*` | generated function/runtime helpers |
| `task_runtime.h` | `ZrLibrary_TaskRuntime_*` | Job prepare/execute/await/fault |

## Rust binding

`ZrRustBinding_Runtime_*`、`Project_*`、`ProjectSession_*`、`NativeModuleBuilder_*`、
`NativeCallContext_*`、`NativeArgumentView_*` 和 `Value_*` 构成稳定 opaque-handle 层；完整
状态码和参数组见 [Rust binding](../05-interop/rust-binding.md)。
