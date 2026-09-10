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
| `execution_budget.h` / `session_checkpoint.h` | `ZrCore_ExecutionBudget_*`, `ZrCore_SessionCheckpoint_*` | 预算、取消、heap accounting、会话回滚，详见 [Core 宿主状态 API](../05-interop/core-runtime-host-api.md) |
| `value.h` / `string.h` | `ZrCore_Value_*`, `ZrCore_String_*` | 值初始化、复制、字符串和 ownership release |
| `object.h` / `module.h` / `ownership.h` | `ZrCore_Object_*`, `ZrCore_Module_*`, `ZrCore_Ownership_*` | prototype、成员、模块 cache 和 ownership，详见 [Core 值/对象 API](../05-interop/core-value-object-api.md) |
| `object.h` / `module.h` | `ZrCore_Object_*`, `ZrCore_Module_*` | 原型、成员/index、导出和 module cache |
| `gc.h` | `ZrCore_GarbageCollector_*`, `ZrCore_Gc_*` | full/step、safepoint、barrier、native pin |
| `exception.h` | `ZrCore_Exception_*` | try/throw/catch、异常归一化和诊断 |
| `gc.h` / `exception.h` | `ZrCore_Gc_*`, `ZrCore_Exception_*` | pin、barrier、GC domain、try/unwind，详见 [Core GC/异常 API](../05-interop/core-gc-exception-api.md) |
| `call_binding.h` | `ZrCore_CallBinding_*` | contract 检查、resolve、generation 和 known call |
| `execution.h` | `ZrCore_Execute`, `ZrCore_Execution_*` | bytecode 驱动、返回值物化 |
| `stack.h` / `call_info.h` / `function.h` | `ZrCore_Stack_*`, `ZrCore_CallInfo_*`, `ZrCore_Function_StackAnchor*` | slot、可重定位 offset/anchor、frame place、native continuation；执行边界详见 [VM 栈/执行参考](../09-runtime-stack-execution-reference.md) |
| `type_layout.h` | `ZrCore_TypeLayout_*` | inline layout、copy/drop、GC scan |
| `reflection.h` | `ZrCore_Reflection_*` | TypeId/token/member/constructor 查询 |
| system/container/reflection/pool headers | `ZrVmLibSystem_*`, `ZrVmLibContainer_*`, `ZrPool_*`, `ZrCore_Reflection_*` | 模块注册、资源/collection/root 边界、stable handle 与 token/generation，详见 [运行时协同参考](../03-modules/system-container-reflection-runtime-reference.md) |

## Parser

| 头文件 | API 族 | 说明 |
| --- | --- | --- |
| `lexer.h` / `parser.h` | `ZrParser_State_*`, `ZrParser_Parse*` | token、AST 和增量边界 |
| `compiler.h` | `ZrParser_Compiler_*` | source -> function/bytecode |
| `parser.h` + `compiler.h` | `ZrParser_Parse*`, `ZrParser_Source_Compile*` | 分阶段 parser/compiler 生命周期、submission 和 writer 见 [Parser/Compiler 深度 API](../05-interop/parser-compiler-c-api.md) |
| `canonical_type.h` | `ZrParser_CanonicalType_*` | TypeId、owner、generic 参数 |
| `canonical_type.h` / `generic_instantiation.h` | `ZrParser_CanonicalType_*`, `ZrParser_GenericInstantiationTable_*` | `<...>`/`where` grammar 对应的 canonical parameter、type/const instance、C instance id 和 sharing，详见 [泛型实例化参考](../02-language/generic-parameter-instantiation-reference.md) |
| `cfg.h` | `ZrParser_Cfg_*` | block/edge/reachability |
| `semantic_ir.h` | `ZrParser_SemanticIr_*`, `SemanticFlow_*` | Place/Value/Loan/cleanup/bounds facts |
| `cfg.h` / `semantic_ir.h` | `ZrParser_Cfg_*`, `ZrParser_SemanticIr_*`, `ZrParser_SemanticFlow_*` | CFG、SemIR、escape/bounds/loan 流分析以及 borrowed 查询结果，详见 [SemIR/CFG 参考](../08-compiler-semantic-ir-facts-reference.md) |
| `semantic_query.h` | `ZrParser_SemanticQuery_*` | type/call/reference/LSP 查询 |
| `writer.h` | `ZrParser_Writer_*` | `.zro/.zri/.zrs` 和 AOT 输出 |
| `writer.h` / `artifact_projection.h` | `ZrParser_Writer_*`, `ZrParser_Artifact*` | binary/intermediate/AOT 输出与 canonical metadata projection，详见 [Artifact Writer/Loader API](../05-interop/artifact-writer-loader-api.md) |
| `test_contract.h` | `ZrParser_TestManifest_*` | Test phase manifest encode/decode |
| `compile_tool.h` | `ZrParser_CompileTool_*`, `ZrParser_CompileToolArtifact_*` | compile-only provider descriptor、contract hash、`.zrm` build dependency |
| `comptime_contract.h` | `ZrParser_ComptimeEffect_IsAllowed`、`ZrParser_ComptimeBudget_Init`、`ZrParser_ComptimeBudget_TryConsume` | effect policy、fuel/heap/diagnostic budget |
| `declaration_transform_contract.h` | `ZrParser_DeclarationPatch_*` | immutable view、Patch 校验和错误名 |

## Library/provider

| 头文件/模块 | API 族 | 说明 |
| --- | --- | --- |
| `native_registry.h` | `ZrLibrary_NativeRegistry_*` | descriptor attach/register/find/phase |
| `call_binding.h` / `typed_call_binding.h` | `ZrCore_CallBinding_*` | contract/hash/relocation、typed call preparation 和 native resolver 见 [Native Registry 调用绑定](../03-modules/native-registry-call-binding.md) |
| `project.h` | `ZrLibrary_Project_*`, `ModuleSpecifier_*` | `.zrp`、module identity、依赖和路径 |
| `file.h` | `ZrLibrary_File_*` | 文件、目录、stream handle、source loader |
| `zrm.h` | `ZrLibrary_Zrm_*` | package/archive entry |
| `project.h` / `file.h` / `zrm.h` | `ZrLibrary_Project_*`, `File_*`, `Zrm_*` | project/import resolver、文件系统和 package 所有权，详见 [项目、文件与 ZRM API](../05-interop/project-file-zrm-api.md) |
| provider `module.h` | `ZrVmLib*_*` | descriptor 和注册入口 |
| `aot_runtime.h` | `ZrLibrary_AotRuntime_*` | generated function/runtime helpers |
| `aot_runtime.h` / `zr_aot_abi.h` | `ZrLibrary_AotRuntime_*`, `SZrAotCodeRegistration` | generated frame、root map、direct call/deopt、cleanup 和 module exports，详见 [AOT lowering 参考](../10-aot-lowering-registration-reference.md) |
| `task_runtime.h` | `ZrLibrary_TaskRuntime_*` | Job prepare/execute/await/fault |
| `task_runtime.h` / `zr.task` | `ZrLibrary_TaskRuntime_*`, `ZrVmTask_*` | Job/Task 状态机、root、scheduler bridge 和 provider await hook，详见 [Task Runtime 深度参考](../03-modules/task-scheduler-runtime-reference.md) |
| `zr.ffi` provider | `ZrVmLibFfi_*` | 动态库、symbol、pointer、buffer、callback handle 与 native-import contract 验证，详见 [FFI Runtime Handle 深度参考](../03-modules/ffi-runtime-handle-reference.md) |
| `native_binding.h` / `native_registry.h` | `ZrLib*Descriptor`, `ZrLibrary_NativeRegistry_*` | provider descriptor、阶段、插件 generation 和 descriptor-derived call binding，详见 [Native Provider Descriptor 深度参考](../03-modules/native-provider-descriptor-reference.md) |
| `native_binding.h` | `ZrLib_CallContext_*`, `ZrLib_TempValueRoot_*`, `ZrLib_Value_*` | native callback 的参数/结果、root、inline view、write-back 和嵌套调用，详见 [Native Call Context 参考](../05-interop/native-call-context-reference.md) |

## Language server

| 头文件 | API 族 | 说明 |
| --- | --- | --- |
| `lsp_interface.h` | `ZrLanguageServer_LspContext_*`, `ZrLanguageServer_Lsp_*` | document revision、diagnostic、completion、hover、navigation、edit、semantic token 与 hierarchy payload |
| `lsp_capability_registry.h` | `ZrLanguageServer_LspCapabilityRegistry_*` | native/WASM capability 元数据、最低 LSP 版本、resolve/runtime 边界 |
| `lsp_diagnostic_store.h` | `ZrLanguageServer_LspDiagnosticStore_BuildResultId` | pull-diagnostic result id 和 immutable snapshot identity |
| `stdio_server.h` / `wasm_exports.h` | `ZrLanguageServer_StdioServer_*`, `wasm_ZrLsp*` | JSON-RPC framing 与 WASM buffer/JSON 所有权；详见 [LSP 协议参考](../04-tools/language-server-protocol-reference.md) |

## Rust binding

`ZrRustBinding_Runtime_*`、`Project_*`、`ProjectSession_*`、`NativeModuleBuilder_*`、
`NativeCallContext_*`、`NativeArgumentView_*` 和 `Value_*` 构成稳定 opaque-handle 层；完整
状态码和参数组见 [Rust binding](../05-interop/rust-binding.md)。
