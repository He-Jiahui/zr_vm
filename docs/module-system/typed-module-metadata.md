---
related_code:
  - zr_vm_core/include/zr_vm_core/metadata_token.h
  - zr_vm_core/include/zr_vm_core/hash.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_metadata_binding.c
  - zr_vm_core/src/zr_vm_core/function_metadata_query.c
  - zr_vm_core/src/zr_vm_core/hash.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/src/zr_vm_core/module/module_internal.h
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
  - zr_vm_core/src/zr_vm_core/module/module_import_signature.c
  - zr_vm_core/src/zr_vm_core/module/module_import_signature.h
  - zr_vm_core/src/zr_vm_core/module/module_import_signature_binding.c
  - zr_vm_core/src/zr_vm_core/module/module_import_signature_binding.h
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_internal.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_union.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_state.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_record.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_record.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_hash.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_hash.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_def.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_def.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_spec.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_spec.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/module_init_analysis.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_ffi.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/metadata_token.h
  - zr_vm_core/include/zr_vm_core/hash.h
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_common/include/zr_vm_common/zr_io_conf.h
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/function_metadata_binding.c
  - zr_vm_core/src/zr_vm_core/function_metadata_query.c
  - zr_vm_core/src/zr_vm_core/hash.c
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/src/zr_vm_core/module/module_internal.h
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
  - zr_vm_core/src/zr_vm_core/module/module_import_signature.c
  - zr_vm_core/src/zr_vm_core/module/module_import_signature.h
  - zr_vm_core/src/zr_vm_core/module/module_import_signature_binding.c
  - zr_vm_core/src/zr_vm_core/module/module_import_signature_binding.h
  - zr_vm_core/src/zr_vm_core/module/module_prototype.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_internal.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/module.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_callback.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_union.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_internal.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_record.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_record.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_hash.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_hash.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_def.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_def.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_spec.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_spec.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/compiler/module_init_analysis.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_ffi.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
plan_sources:
  - user: 2026-03-31 实现 “M6 强类型推断完整闭环计划”
  - .codex/plans/M6 强类型推断完整闭环计划.md
  - .codex/plans/zr_vm阶段化总计划.md
  - docs/plans/using/02-using-scopes-and-plugin-guards.md
  - docs/plans/using/03-metadata-and-token-model.md
  - docs/plans/using/07-implementation-blueprint.md
  - user: 2026-04-06 struct 值类型与 native wrapper 分层方案
  - user: 2026-04-06 新的 source-level wrapper decorator surface 和具体 handle_id lowering runtime完善
tests:
  - tests/parser/test_type_inference.c
  - tests/parser/test_compiler_regressions.c
  - tests/parser/test_compiler_call_lowering_focus_main.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_prototype.c
  - tests/parser/test_instruction_execution.c
  - tests/ffi/test_ffi_module.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/scripts/test_artifact_golden.c
  - tests/cmake/run_projects_suite.cmake
  - tests/fixtures/projects/import_binary/fixtures/greet_binary_source.zr
  - tests/fixtures/projects/import_binary/bin/greet.zro
  - tests/module/test_metadata_token_model.c
  - tests/module/test_metadata_type_ref_binding.c
  - tests/module/test_metadata_runtime_query.c
  - tests/module/test_metadata_module_hash_golden.c
  - tests/parser/test_project_import_canonicalization.c
  - tests/acceptance/2026-06-17-using-import-guard.md
  - tests/module/test_module_system.c
doc_type: module-detail
---

# Typed Module Metadata

## Purpose

M6 的目标不是把局部推断再补几条特判，而是把“编译期知道的类型”真正落成模块产物的一部分，让三条链路用同一份事实源工作：

1. 编译器推断和 overload 选择
2. import 时的 compile-time signature / member inference
3. `.zro` 持久化正式 typed metadata，`.zri` 保留 debug / intermediate 镜像给后续运行时与 AOT 验证使用

这轮实现直接升级 `.zro` schema，不保留旧格式双读兼容。

## Related Files

- runtime function metadata
  - `SZrFunction` 在 [zr_vm_core/include/zr_vm_core/function.h](../../zr_vm_core/include/zr_vm_core/function.h) 新增 `typedLocalBindings` 和 `typedExportedSymbols`
  - `function.c` 负责这些数组的生命周期
- binary IO schema
  - `ZrCore_Hash_CreateStable64WithPrefix` 在 [zr_vm_core/include/zr_vm_core/hash.h](../../zr_vm_core/include/zr_vm_core/hash.h) 提供不依赖进程随机 seed 的稳定 `XXH3_64bits` hashing，用于 metadata signature fingerprint
  - `TZrMetadataToken`、`EZrMetadataTableTag`、`SZrMetadataTokenRecord` 在 [zr_vm_core/include/zr_vm_core/metadata_token.h](../../zr_vm_core/include/zr_vm_core/metadata_token.h) 定义
  - `SZrIoFunctionTypedTypeRef`、`SZrIoFunctionTypedLocalBinding`、`SZrIoFunctionTypedExportSymbol` 在 [zr_vm_core/include/zr_vm_core/io.h](../../zr_vm_core/include/zr_vm_core/io.h) 定义
  - `io.c` 负责 `.zro` 读写
- compile-time metadata construction
  - [zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c](../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c)
  - [zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c](../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_token.c)
  - [zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_record.c](../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_record.c)
  - [zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_record.h](../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_record.h)
  - [zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_hash.c](../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_hash.c)
  - [zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_hash.h](../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_module_hash.h)
  - [zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.c](../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.c)
  - [zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.h](../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_signature.h)
  - [zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_ref.c](../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_ref.c)
  - [zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_ref.h](../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_metadata_type_ref.h)
- import normalization
  - [zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c](../../zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c)
  - [zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c](../../zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c)
  - [zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c](../../zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_native.c)
- runtime import signature verification
  - [zr_vm_core/src/zr_vm_core/module/module_import_signature.c](../../zr_vm_core/src/zr_vm_core/module/module_import_signature.c)
  - [zr_vm_core/src/zr_vm_core/module/module_import_signature.h](../../zr_vm_core/src/zr_vm_core/module/module_import_signature.h)
  - [zr_vm_core/src/zr_vm_core/module/module_import_signature_binding.c](../../zr_vm_core/src/zr_vm_core/module/module_import_signature_binding.c)
  - [zr_vm_core/src/zr_vm_core/module/module_import_signature_binding.h](../../zr_vm_core/src/zr_vm_core/module/module_import_signature_binding.h)
  - [zr_vm_core/src/zr_vm_core/module/module_loader.c](../../zr_vm_core/src/zr_vm_core/module/module_loader.c)
- opcode selection and conversions
  - [zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c](../../zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c)
  - [zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c](../../zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c)
  - [zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c](../../zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c)
- artifact writers
  - [zr_vm_parser/src/zr_vm_parser/writer.c](../../zr_vm_parser/src/zr_vm_parser/writer.c)
  - [zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c](../../zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c)

## Behavior Model

函数级 typed metadata 统一使用三类记录：

- `SZrFunctionTypedTypeRef`
  - 表示基础值类型、nullable、ownership、数组标记、用户类型名，以及数组元素类型
- `SZrFunctionTypedLocalBinding`
  - 记录局部名、stack slot 和推断出的类型
- `SZrFunctionTypedExportSymbol`
  - 记录导出名、stack slot、access modifier、symbol kind、值类型或返回类型，以及函数参数类型列表
  - 记录导出声明的源码行列范围，供 `.zro` 消费侧做 symbol-level definition / references / document highlight

这里没有把类型信息散落进原有运行时字段。`prototypeData` 仍负责 prototype/member 布局，typed metadata 负责“导出的值和局部绑定在编译期是什么类型”。当前 union 前端切片也遵守这个分工：typed metadata 的 signature blob 表达 `Option<int>` 这类导出签名，`prototypeData` 则记录 `ZR_OBJECT_PROTOTYPE_TYPE_UNION`、每个 `ZR_AST_UNION_VARIANT` 的 tag/kind/field-count、payload 字段声明名、类型名、运行时 storage name、位置、passing mode、ownership qualifier，以及 tag/payload byte layout metadata；运行时的 typed union local materialization 直接消费这份 metadata，把构造器 carrier 写成 inline `[tag][payload]` bytes，并由 type layout 在 active variant 上执行 value/owner payload copy、drop 和 GC visitor。

## Metadata Token Model

Typed metadata 现在有一层稳定的 token / signature 表示，用来给后续跨模块签名匹配提供落盘身份：

- `TZrMetadataToken` 使用 `table_tag << 24 | rid` 的 32-bit 编码，table tag 定义在 [metadata_token.h](../../zr_vm_core/include/zr_vm_core/metadata_token.h)。
- 每个 `SZrFunctionTypedExportSymbol` 可携带 `metadataToken`、`signatureToken`、`signatureBlobOffset`、`signatureBlobLength`。
- 每个 `SZrFunctionTypedExportSymbol` 和 `SZrMetadataTokenRecord` 还携带 `signatureHash`，当前由 `zr.md.sig.v1\0` 前缀 + signature blob 的稳定 `XXH3_64bits` 计算。
- 每个 `SZrMetadataTokenRecord` 还携带 `ownerToken`，用于表达 ref 层级和配对签名归属。
- import effect 和 `SZrMetadataTokenRecord` 还携带 target signature identity：`targetMetadataToken`、`targetSignatureToken`、`targetSignatureHash` 用来保存 ref 期望绑定到的 provider def/signature；`targetModuleSignatureHash` 保存编译期看到的 provider module ABI fingerprint；`requestedModuleVersion`、`minModuleVersionInclusive`、`maxModuleVersionExclusive` 保存 `AssemblyRef` 的 semantic version range。
- Provider token stream 含真实 `MODULE` RID 1 记录和 paired `SIGNATURE` record；`MODULE` signature blob 使用 `ZR_METADATA_SIGNATURE_NODE_MODULE`，编码 heap-indexed entry name 与 module version。
- `SZrFunction` / `SZrIoFunction` 持有函数级 `metadataTokenRecords`、entry-function 级 `moduleMetadataTokenRecords`、`signatureBlobHeap` 与 `metadataStringHeap`。
- `SZrFunction` / `SZrIoFunction` 还持有 `moduleVersion` 和 `moduleSignatureHash`，当前由 `compiler_metadata_module_hash.c` 以 `zr.md.mod.v2\0` 输入域计算稳定 module ABI fingerprint：provider module version、public typed exports 的导出身份/`signatureHash`/canonical signature blob bytes，以及本模块 `TYPE_DEF` / `TYPE_SPEC` 实体 record identity 都会参与。
- 运行期 `SZrFunction` 还持有 `moduleMetadataBindings`，这是成功 import verification 后产生的 ref→def binding result sidecar；它记录 caller `MEMBER_REF` token 到 provider `MEMBER_DEF` / `SIGNATURE` token、hash 和 module hash 的解析结果，也记录 caller `ASSEMBLY_REF` token 到 provider `MODULE` RID 1 及其 paired `SIGNATURE` token/hash 的解析结果，还记录 matching `TYPE_SPEC` / `TYPE_DEF` binding 的 expected/resolved layout identity，并能记录已经携带 stable provider TypeDef target identity 的 caller `TYPE_REF -> provider TYPE_DEF / SIGNATURE` binding。`ZrCore_Function_FindModuleMetadataBinding()` 可按 caller ref token 只读查询该 sidecar；`ZrCore_Function_FindMetadataTokenRecord()` / `ZrCore_Function_FindMetadataSignatureRecord()` 查询函数级 record stream，`ZrCore_Function_FindModuleMetadataTokenRecord()` / `ZrCore_Function_FindModuleMetadataSignatureRecord()` 查询 entry-function 级 module ref table。signature 查询要求 entity `relatedToken` 指向 `SIGNATURE`，且 signature record 的 `relatedToken` / `ownerToken` 都回指 entity token。`.zro` patch 28 会通过 `SZrIoFunction.moduleMetadataBindings` 持久化基础 binding，patch 32 会额外持久化 binding-level layout fields 并在 runtime load 时恢复；record 查询 API 不新增 schema patch。
- TypeSpec 和 TypeRef status helpers 会在 definition/layout drift 时拒绝 partial binding，并分别通过 `type_spec_mismatch` / `type_ref_mismatch` module-load diagnostic 暴露，不改变成功 import verification 后 sidecar 的 best-effort 语义。
- `.zro` patch 20 在 typed export symbol 后追加 token 字段，并在 typed export 区之后、static import 区之前写入函数级 metadata token block；patch 21 继续追加 `signatureHash`；patch 22 继续追加 metadata record 的 `ownerToken`；patch 23 继续追加 metadata record 的 `targetSignatureHash` 以及 module effect 的 `targetMetadataToken` / `targetSignatureToken` / `targetSignatureHash`；patch 24 继续追加 metadata record 的 `targetMetadataToken` / `targetSignatureToken`；patch 25 继续追加 entry-function 级 `moduleMetadataTokenRecords` import ref 聚合表；patch 26 继续追加 `moduleSignatureHash`；patch 27 继续追加 metadata record 与 module effect 的 `targetModuleSignatureHash`；patch 28 继续追加 runtime ref→def binding result list；patch 29 继续追加 provider `moduleVersion` 以及 metadata record / module effect 的 AssemblyRef requested/min/max version 字段；patch 30 继续追加 metadata string heap；patch 31 继续追加 record-level `layoutVersion` / `layoutHash`；patch 32 继续追加 binding-level expected/resolved `layoutVersion` / `layoutHash`。reader 对旧 patch 缺失字段兼容置 0、null 或空表；future patch 会在 IO header 阶段以 actual/supported patch 诊断拒绝。

第一阶段锚定本模块导出符号，并为 import/module effects 建立函数级 ref 前置记录和 entry-function 级 module ref 聚合表：编译末期为 provider 写入 `MODULE` RID 1 和对应 `SIGNATURE` token，为每个 typed export 分配 `MEMBER_DEF` token 和对应 `SIGNATURE` token。`MODULE` signature blob 使用 `MODULE <entryName> <moduleVersion>`，不包含 `moduleSignatureHash`；typed export 签名 blob 使用 canonical `METHOD_SIG` / `FIELD_SIG`，类型子节点当前覆盖 `PRIMITIVE`、`TYPE_REF`、`ARRAY`、`OWNERSHIP`、`UNION`、`NULLABLE`、`GENERIC_INST`。`compiler_metadata_signature.c` 负责这些 method/field/type signature size/write、`UNION` 节点识别、非 union 泛型闭型 `GENERIC_INST` 编码和 `zr.md.sig.v1\0` stable hash；字符串引用写为 `metadataStringHeap` 的固定 `u32` content-stable index。`compiler_metadata_module_record.c` 负责 provider `MODULE` record/signature；`compiler_metadata_token.c` 负责 RID、record pair、owner-chain、import effect 编排和 metadata string heap 收集；`compiler_metadata_module_hash.c` 负责 `zr.md.mod.v2\0` module ABI hash；`compiler_metadata_type_spec.c` 负责从导出 typed signature 扫描闭型/复合类型并生成基础 `TYPE_SPEC` record；`compiler_metadata_type_def.c` 负责本地 union `TYPE_DEF` record 与逻辑契约 signature；`compiler_metadata_type_def_layout.c` 负责 record-level union layout identity；`compiler_metadata_ref.c` 负责从函数级 import refs 派生 `moduleMetadataTokenRecords`。`UNION` 节点会在当前脚本 AST 中识别已知 union 类型，并编码 union 基名与递归泛型实参，例如 `Option<int>`；普通泛型闭型如 `Box<int>` 编码为 `GENERIC_INST(TYPE_REF("Box"), int)`。导出签名中出现的 nullable、ownership、array 和带泛型实参的闭型会生成 `TYPE_SPEC` 实体 record 与配对 `SIGNATURE` record，复用同一 `signatureBlobHeap` 和 metadata string heap。导出签名中引用的本地 union 会生成 `TYPE_DEF` + paired `SIGNATURE`，signature blob 包含 heap-indexed type name、generic arity、variant name/kind/default flag/field count、payload field name/passing mode/TypeSig；物理布局不进入该 blob。`TYPE_DEF` record 的 `layoutVersion` / `layoutHash` 单独保存 inline value ABI 布局 identity，当前输入为 tag size、payload offset/size/align、整体 size/align 和各 variant payload field 的 offset/size/align；未知/泛型 payload slot 与标量 fallback alignment 在 metadata fingerprint 中规范化为 64-bit reference size / max-8 alignment，避免宿主 `ZR_ALIGN_SIZE` 差异导致 module ABI hash 漂移。`moduleSignatureHash` 按 provider `moduleVersion`、public typed export name 排序后的 export identity/`signatureHash`/canonical signature blob bytes，以及排序后的 `TYPE_DEF` / `TYPE_SPEC` 实体 record identity 计算 module ABI fingerprint；同签名同版本重建保持稳定，返回类型、module version、TypeSpec identity 或本地 TypeDef contract/layout 等 ABI 身份变化会造成 hash 漂移。

`moduleEntryEffects` / exported callable summary effects 里可识别的 import read/call/ref 会生成函数级 `ASSEMBLY_REF`、`TYPE_REF`、`MEMBER_REF` token 与各自的 signature record。实体记录通过 `ownerToken` 串成 `AssemblyRef <- TypeRef <- MemberRef`；配对 `SIGNATURE.ownerToken` 指回被签名实体。`MEMBER_REF` signature blob 以 `ZR_METADATA_SIGNATURE_NODE_MEMBER_REF` 开头，并写入 canonical module name、symbol name 和 effect kind 的 string heap index；当 provider export 可解析时，blob 后续追加目标 `METHOD_SIG` / `FIELD_SIG` 子签名。typed export def 与 import ref target 复用同一套 encoder；当前 `METHOD_SIG` v1 写入 signature version、call convention、generic arity、return type、parameter count，以及每个参数的 mode 和 type；`FIELD_SIG` v1 写入 signature version 和字段 type。record 的 `targetMetadataToken` / `targetSignatureToken` / `targetSignatureHash` 保存期望 provider def、signature token 和签名 hash；`AssemblyRef` record 与 import effect 的 `targetModuleSignatureHash` 保存期望 provider module ABI hash，`requestedModuleVersion` / `minModuleVersionInclusive` / `maxModuleVersionExclusive` 保存默认 semver range。当 source summary 尚未提供 provider hash 时，会从追加的目标子签名计算稳定 hash。每对实体/`SIGNATURE` 共享同一个 `signatureHash`，便于后续 loader 在做完整 ref→def 绑定前先进行快速 fingerprint 判断。`layoutHash` 不参与 `signatureHash`，按值共享布局检查需显式读取 record-level layout identity。`moduleMetadataTokenRecords` 复用同一 signature heap，并按 table、owner、hash、target token/hash/module hash/version range 和 signature blob 字节去重重复 entry/callable import refs。source/binary import 的 compile-time member view 会保留 provider module hash、provider token/hash、provider version 和同名函数候选；member call 通过 `type_inference_member_resolution.c` 按实参类型选择 exact/compatible 候选并拒绝不匹配签名，module-init import-call effect 同样按实参类型选择 source/binary export identity。source summary 里的 primitive annotation 会规范化成 `PRIMITIVE` signature 节点，避免 `bool` / `int` 被编码为命名 `TYPE_REF` 而与 provider typed export hash 漂移。native import 的 module-level members 现在会由 `type_inference_native.c` 分配 synthetic `MEMBER_DEF` / `SIGNATURE` token 和稳定 signature hash，不改变 native descriptor ABI；native module 没有 source/binary summary 时，`module_init_analysis.c` 会走 native compile-info fallback，把这些身份字段写入 guarded import effect；registered native provider 运行期也复用同一 verifier，因此 target hash drift 会让 guard 进入 `else`。现有 import owner-chain 的 `TYPE_REF` 仍主要是 module-member owner 占位；provider summary backed target signature 类型现在会由 `compiler_metadata_type_ref.c` 额外生成 stable provider `TYPE_REF` records，覆盖返回值、参数和嵌套 generic argument 中可解析到 provider `TYPE_DEF` 的类型。module-qualified typed local annotations such as `provider.Option<int>` and destructured/unqualified alias annotations such as `Option<int>` backed by `typeValueAliases` use the same producer and can create an explicit `ASSEMBLY_REF` even when there are no import member effects. `ZrCore_Function_BindMatchingTypeRefMetadata()` 会把这些 caller `TYPE_REF` 绑定到 provider `TYPE_DEF` / paired `SIGNATURE` 和 module/layout identity。

`ZrCore_Function_BindMatchingTypeRefMetadataWithStatus()` 会在 stable provider TypeRef binding 失败时报告 unmatched、definition mismatch 和 layout mismatch 计数及首个 expected/actual token、signature hash、layout version/hash context；runtime 成功 import 后会把这些 drift 作为 `type_ref_mismatch` module-load diagnostic 暴露且不写 partial binding。Module-qualified typed local annotations and destructured/unqualified alias annotations reuse the same producer/binder/status path.

`io_runtime.c` 在 `.zro` 加载到 runtime function 时复制 token records、module ref table、签名堆、metadata string heap、hash、module version、module ABI hash、owner chain、AssemblyRef version range、record-level layout identity 和 target signature identity；exported callable summary effects 会在 module init finalize 后附着到对应 child function；`module_import_signature.c` 的 import verification 会先从 `moduleMetadataTokenRecords` 中匹配 caller 的 `MEMBER_REF` record，用它补齐缺失的 effect version range、target module hash、target token 和 target signature hash，再检查 provider `moduleVersion` 是否落入 AssemblyRef range。通过版本检查后，verifier 在 expected `targetModuleSignatureHash` 非零时与 provider entry function `moduleSignatureHash` 比对，随后在 provider 同名 typed exports 中按 target hash、target signature blob 和 target token 选择匹配候选，而不是按名字取第一个导出。verifier 还会直接遍历 `moduleMetadataTokenRecords`，把匹配当前 import module 的 `MEMBER_REF` record 解码为临时 effect 并校验 `AssemblyRef <- TypeRef <- MemberRef` owner-chain；patch 30+ 的 module/symbol/type string ref 通过 `metadataStringHeap` 解码，旧 patch 仍回退 inline string，因此 `moduleEntryEffects` 缺失或被瘦身时，module ref table 中的 target version/token/hash/bytes drift 仍会被拒绝。选中候选后，verifier 在 effect/provider 两侧 token 均非零时绑定 `targetMetadataToken` / `targetSignatureToken` 到 provider typed export `metadataToken` / `signatureToken`；若双方 token 都存在且不一致，会直接 mismatch，只有旧产物缺 token 时才继续 hash/blob fallback。随后 verifier 把实际 caller function 的 import effects 或 module ref record 的 `targetSignatureHash` 与 provider typed export `signatureHash` 比对，并在存在追加 target `METHOD_SIG` / `FIELD_SIG` 时优先从 `moduleMetadataTokenRecords` 截取 expected signature bytes，再回退函数级 `metadataTokenRecords`，最后与 provider signature blob 做长度和字节确认。版本不兼容、target module hash、token、签名 hash、签名字节不一致或 provider 不可用时，guard import 返回 `null` 并让 `using` 走 `else`；普通 required import 遇到 provider 不可用时抛出 `import_load_unavailable`，遇到版本不兼容时抛出 `assembly_version_mismatch`，遇到 target module ABI hash drift 时抛出 `assembly_signature_mismatch`，member token/hash/bytes drift 仍抛出 `import signature mismatch`。这些诊断包含 canonical module/member 和相应 expected/actual 或 min/max/actual context，避免静默接受 ABI 漂移。

当前新增的运行期 binding sidecar 会在 import verifier 成功完成 target module hash、target token、target signature hash 和 target signature bytes 校验后写入或更新 `SZrFunction.moduleMetadataBindings`。这条 sidecar 同时覆盖 `moduleEntryEffects` 校验路径和 direct `moduleMetadataTokenRecords` 解码路径：`MEMBER_REF` binding 保留原始 caller member ref token，并记录 expected 与 resolved member metadata/signature token、signature hash、module ABI hash；`ASSEMBLY_REF` binding 保留 caller assembly ref token、paired `SIGNATURE` token/hash 和 expected module ABI hash，并把 resolved metadata token 记录为 provider `ZR_METADATA_TABLE_MODULE` RID 1、resolved signature token/hash 记录为 provider `MODULE` record 的 paired `SIGNATURE`、resolved module hash 记录为 provider entry function `moduleSignatureHash`。旧 provider 产物没有 `MODULE` record 时，AssemblyRef binding 仍可兼容保留 0 resolved signature identity。TypeSpec/TypeDef sidecar 继续记录 expected/resolved layout identity。调用者可用 `ZrCore_Function_FindModuleMetadataBinding(function, refToken)` 查询成功绑定结果；`.zro` patch 28 会逐字段保存基础 binding，patch 32 会保存 layout fields，并在 `io_runtime.c` 加载时恢复。source module summary 也会在 metadata tokens 构建后回填最终 typed export token/hash/value type/参数类型，避免正常 guard 因 stale summary 产生 target identity 与 appended signature bytes 分裂。`function_metadata_binding.c` 还提供 `ZrCore_Function_UpsertModuleMetadataBinding()`、`ZrCore_Function_BindMatchingTypeSpecMetadata()`、`ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus()` 和 `ZrCore_Function_BindMatchingTypeRefMetadata()`：upsert helper 集中维护 binding sidecar 容量和复用逻辑，TypeSpec helpers 按 caller/provider `TYPE_SPEC` 的 canonical signature hash 与 signature blob 字节相等记录跨模块 TypeSpec baseline binding；对于 union TypeSpec，还会定位对应 caller/provider `TYPE_DEF`，要求 TypeDef signature hash/blob 和 record-level layoutVersion/layoutHash 都一致，再写入 TypeSpec binding 与 TypeDef layout binding。TypeRef helper 只处理 caller `TYPE_REF.targetMetadataToken` 指向 `TYPE_DEF` 的记录，并要求 target TypeDef token、signature hash/token、module hash、layout identity 和 base name 匹配后写入 TypeRef binding。status helper 可报告 caller/matched/unmatched、definition mismatch、layout mismatch 计数和首个 expected/actual context。成功 member import verification 后，`module_import_signature.c` 会 opportunistically 调用 status helper；匹配项继续写入 provider `TYPE_SPEC` / paired `SIGNATURE` binding，unmatched 或 definition/layout mismatch 通过 module-load diagnostic 暴露为 `type_spec_mismatch`，但不改变 import 成功语义；definition/layout mismatch 不会留下 partial sidecar。

TypeRef binding uses the same status/diagnostic pattern: `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus()` reports unmatched, definition mismatch, and layout mismatch for stable `TYPE_REF -> TYPE_DEF` records, and successful member import verification records `type_ref_mismatch` when those records drift while leaving import success best-effort.

共享 string heap 索引化、`.zro` patch 30 roundtrip 及 project-import metadata blob 验证读取已同步；descriptor plugin safe unload/cache invalidation baseline 已接入 native registry owner refcount；loader-facing TypeSpec mismatch diagnostics 已接入 module-load diagnostic channel；AOT descriptor 更细 loader diagnostics 已接入 AOT runtime 和 core module-load diagnostic channel。当前实现先保证 token 结构、函数级 ref owner-chain、entry-function 级 module ref 聚合表、导出签名中闭型/复合类型的基础 `TYPE_SPEC` records、TypeSpec canonical signature 去重、TypeSpec canonical signature 跨模块 baseline binding、union TypeSpec→TypeDef definition/layout binding、TypeRef→TypeDef consumer-side binding、provider summary backed stable TypeRef producer（含嵌套泛型实参、module-qualified typed local annotations 和 destructured/unqualified alias annotations）、TypeSpec mismatch status 和 loader-facing diagnostic、runtime 对 module ref table 的优先消费/直连扫描/旧 records 回退、runtime ref→def binding sidecar、AssemblyRef→MODULE binding sidecar、binding 查询 API、函数级与 module ref table 的 token/signature record 查询 API 和 `.zro` patch 28 roundtrip、`.zro` patch 29 provider module version / AssemblyRef semver range roundtrip、`.zro` patch 30 metadata string heap roundtrip、`.zro` patch 31 record layout identity roundtrip、`.zro` patch 32 binding layout identity roundtrip、dependency manifest 显式 AssemblyRef min/max range、future patch actual/supported 诊断、target module hash 绑定、target token 绑定、target token mismatch diagnostics、target signature hash、def/ref canonical MethodSig/FieldSig、非 union 泛型闭型 `GENERIC_INST` 签名编码、签名 hash 和 module ABI hash 基础、native/source/binary compile-time member identity、registered native verifier 覆盖、required provider unavailable `import_load_unavailable`、project source loader/native descriptor plugin load-error detail、AOT descriptor field-level loader detail、registry owner refcount API、descriptor plugin live-owner invalidation/reload guard、同名 provider export 运行期候选绑定、二进制 roundtrip 稳定，以及 required/guard import 的运行期 version/module/token/hash-first + signature-confirmed 消费路径（含 required member-level `import signature mismatch`、required module-level `assembly_signature_mismatch`、required version-level `assembly_version_mismatch`、guard version mismatch、guard target module hash mismatch、guard target token mismatch、guard target bytes mismatch、entry effects 缺失时的 module ref table drift、provider unavailable 与 nested callable caller）。

TypeRef mismatch status and loader-facing `type_ref_mismatch` diagnostics are also covered for provider-summary backed stable TypeRefs; module-qualified explicit typed local annotations and destructured/unqualified alias mapping are covered.

TypeRef producer update (2026-06-18 20:44:02 +08:00): producer-side stable provider TypeRef extraction is now covered for import target signature return/parameter types and their nested generic arguments when the provider summary exposes matching `TYPE_DEF` records. Module-qualified typed local annotation support now reuses this helper.

TypeRef mismatch status update (2026-06-18 21:22:00 +08:00): `SZrMetadataTypeRefBindStatus` and `ZrCore_Function_BindMatchingTypeRefMetadataWithStatus()` now expose stable TypeRef binding counts plus unmatched, definition mismatch, and layout mismatch context. Successful member import verification records `type_ref_mismatch` in the module-load diagnostic channel when stable provider TypeRef binding drifts, while preserving best-effort import success and avoiding partial sidecar writes.

TypeRef explicit annotation update (2026-06-18 22:35:57 +08:00): `compiler_metadata_type_ref.c/.h` now splits top-level module-qualified type names and scans `typedLocalBindings`, so a local annotation such as `provider.Option<int>` produces a stable provider `TYPE_REF` and paired `SIGNATURE` even when the function has no import member effects. `compiler_metadata_token.c` collects those explicit modules, writes the needed metadata string heap entries, emits an explicit `ASSEMBLY_REF`, and resolves the TypeRef owner through the AssemblyRef RID resolver. Verification covered WSL GCC/clang `zr_vm_metadata_type_ref_binding_test` 6/0, `zr_vm_metadata_token_model_test` 21/0, `zr_vm_project_import_canonicalization_test` 31/0, plus MSVC Debug `zr_vm_metadata_type_ref_binding_test` 6/0.

TypeRef alias annotation update (2026-06-18 23:07:30 +08:00): `compiler_metadata_type_ref.c/.h` now resolves unqualified typed local aliases through `typeValueAliases`, so destructuring import bindings such as `Option -> provider.Option` let local annotations `Option<int>` and nested forms like `Box<Option<int>>` produce stable provider `TYPE_REF` records and explicit `ASSEMBLY_REF` owners without import member effects. The token string heap and explicit module collector reuse the same resolver. Verification covered WSL GCC/clang `zr_vm_metadata_type_ref_binding_test` 8/0, `zr_vm_metadata_token_model_test` 21/0, `zr_vm_project_import_canonicalization_test` 31/0, plus MSVC Debug `zr_vm_metadata_type_ref_binding_test` 8/0.

TypeSpec status update (2026-06-18 10:52:36 +08:00): canonical signature deduplication is implemented for exported-signature TypeSpec records, and cross-module baseline binding now records matching caller/provider `TYPE_SPEC` records in `moduleMetadataBindings` by signature hash plus blob equality. Follow-up TypeSpec mismatch status and loader-facing diagnostics are now covered; union TypeSpec→TypeDef definition/layout binding is covered by the 2026-06-18 16:11:43 update.

TypeSpec mismatch status update (2026-06-18 12:16:33 +08:00): `SZrMetadataTypeSpecBindStatus` and `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus()` now expose core-level mismatch status for caller/provider TypeSpec binding. The status reports caller, matched, and unmatched TypeSpec counts plus the first unmatched caller token/hash; the old binding wrapper keeps best-effort import behavior. Loader-facing diagnostics are now covered by the 2026-06-18 13:00:54 update.

TypeSpec loader diagnostic update (2026-06-18 13:00:54 +08:00): successful member import verification now calls the TypeSpec status helper and records a module-load diagnostic `type_spec_mismatch` when caller TypeSpec records do not match provider TypeSpec records. The diagnostic includes canonical module/member context, caller/matched/unmatched counts, and the first unmatched TypeSpec token/hash; import verification remains successful so TypeSpec sidecars stay opportunistic.

Descriptor plugin invalidation update (2026-06-18 12:49:27 +08:00): native registry invalidation and descriptor plugin reload now reject live descriptor-plugin modules before clearing module cache, closing plugin handles, or replacing descriptor records. `ZR_LIB_NATIVE_REGISTRY_ERROR_MODULE_IN_USE` reports the module/source/refcount context; tests cover a real descriptor plugin fixture with a simulated live owner ref and the ref-zero success path. AOT descriptor diagnostics are covered by the 2026-06-18 13:33:34 update.

AOT descriptor diagnostic update (2026-06-18 13:33:34 +08:00): `aot_runtime.c` validates descriptors field by field and records the first bad ABI/backend/module/entry/blob/thunk field in `lastError`, for example `abiVersion expected=2 actual=1`. `ZrLibrary_AotRuntime_ModuleLoader()` forwards that runtime detail to the core module-load diagnostic as `loader=aot-runtime ... result=descriptor-load-failed ... detail=...`, so required import/load diagnostics no longer collapse AOT descriptor problems into a generic load failure. WSL GCC verification covered `zr_vm_project_import_canonicalization_test` 31/0, `zr_vm_metadata_token_model_test` 15/0, `zr_vm_native_closure_value_test` 3/0, `zr_vm_aot_c_descriptor_diagnostics_test` 1/0, and CTest `aot_c_descriptor_diagnostics|metadata_token_model` 2/2.

TypeDef baseline update (2026-06-18 14:09:55 +08:00): local union definitions referenced by exported typed signatures now receive `TYPE_DEF` metadata token records. `compiler_metadata_type_def.c/.h` scans exported return/parameter types, resolves current-script union declarations through the shared signature helper, deduplicates by union base name and generic arity, and emits a `TYPE_DEF` + paired `SIGNATURE` record whose blob is `TYPE_DEF <metadataStringHeap name index> <genericArity>`. This reuses the existing signature blob heap, `signatureHash`, shared metadata string heap, `.zro` patch 30 persistence, and runtime copy path; no new patch was required. Verification covered WSL clang/gcc `zr_vm_metadata_token_model_test` 16/0, `zr_vm_project_import_canonicalization_test` 31/0, and metadata CTest 1/1.

TypeDef variant contract update (2026-06-18 14:35:52 +08:00): local union `TYPE_DEF` signature blobs now include the public variant and payload field contract. The blob shape is `TYPE_DEF <metadataStringHeap name index> <genericArity> <variantCount> ...`; each variant writes heap-indexed variant name, variant kind, default-using flag, and field count; each payload field writes heap-indexed field name, passing mode, and canonical TypeSig. `compiler_metadata_type_def_collect_strings()` feeds those names and referenced type names into the shared metadata string heap, so this remains a patch 30 heap-indexed encoding with no new `.zro` patch. Verification covered WSL clang/gcc `zr_vm_metadata_token_model_test` 16/0, `zr_vm_project_import_canonicalization_test` 31/0, and metadata CTest 1/1. Windows Debug smoke exposed and fixed the MSVC DLL export boundary for `compiler_build_function_metadata_tokens` by adding `ZR_PARSER_API`; after that, MSVC `zr_vm_metadata_token_model_test` also passed 16/0, and WSL clang/gcc metadata quick rebuilds remained 16/0.

TypeDef layout identity update (2026-06-18 15:28:05 +08:00): local union `TYPE_DEF` records now carry record-level `layoutVersion` / `layoutHash` for inline value ABI layout checks. The physical layout identity is not written into the `TYPE_DEF` signature blob and therefore does not change `signatureHash`; it is persisted separately in `.zro` patch 31 after each record's `signatureHash`, with older patches reading zero. `compiler_metadata_type_def_layout.c/.h` computes the fingerprint from tag size, payload offset/size/align, overall size/align, and each variant payload field's offset/size/align. Verification covered WSL clang/gcc `zr_vm_metadata_token_model_test` 17/0, `zr_vm_project_import_canonicalization_test` 31/0, metadata CTest 1/1, and MSVC Debug `zr_vm_metadata_token_model_test` 17/0. The layout helper is split out so `compiler_metadata_type_def.c` stays under the large-file threshold.

TypeSpec layout binding update (2026-06-18 16:11:43 +08:00): matching union `TYPE_SPEC` records now also bind their corresponding provider `TYPE_DEF` definition and layout identity. `SZrMetadataTokenBinding` carries expected/resolved layout version/hash fields, persisted in `.zro` patch 32. `ZrCore_Function_BindMatchingTypeSpecMetadataWithStatus()` rejects TypeDef contract drift and layout drift without writing partial TypeSpec/TypeDef bindings, and loader `type_spec_mismatch` diagnostics include definition/layout mismatch counts plus expected/actual signature/layout context. Verification covered WSL clang/gcc `zr_vm_metadata_token_model_test` 20/0, `zr_vm_project_import_canonicalization_test` 31/0, metadata CTest 1/1, and MSVC Debug `zr_vm_metadata_token_model_test` 20/0.

Module ABI TypeDef/TypeSpec identity hash update (2026-06-18 17:08:15 +08:00): `compiler_metadata_module_hash.c/.h` now owns module ABI fingerprint calculation. `moduleSignatureHash` uses `zr.md.mod.v2\0` and includes provider module version, sorted public typed export identity/signature blobs, and sorted `TYPE_DEF` / `TYPE_SPEC` entity record identity. This makes local TypeDef contract/layout or TypeSpec identity drift change the module ABI hash even when the exported function signature blob is unchanged. Verification covered WSL clang/gcc `zr_vm_metadata_token_model_test` 21/0, `zr_vm_project_import_canonicalization_test` 31/0, metadata CTest 1/1, and MSVC Debug `zr_vm_metadata_token_model_test` 21/0. No new `.zro` patch was required because patch 26 already persists `moduleSignatureHash`.

Module ABI golden update (2026-06-18 23:45:06 +08:00): `tests/module/test_metadata_module_hash_golden.c` now fixes module-level ABI fingerprints for both a simple public function module and a local generic-union module. The expected hashes are `0xE701BC33ECB6BF89` for `sum(i64,i64)->i64` and `0x485AE44EE06010E4` for `Option<T>` plus `choose(): Option<int>`. During this slice, `compiler_metadata_type_def_layout.c` also stopped feeding host `ZR_ALIGN_SIZE` directly into record-level layout fingerprints for unknown/generic payload slots and scalar fallback alignment; the metadata fingerprint uses canonical 64-bit reference size and max-8 scalar alignment so GCC/Clang and MSVC produce the same module ABI hash. This does not change runtime union layout generation and does not require a `.zro` patch. Verification covered WSL GCC/clang `metadata_token_model` 21/0, `metadata_type_ref_binding` 8/0, `metadata_module_hash_golden` 2/0, direct `zr_vm_project_import_canonicalization_test` 31/0, and MSVC Debug metadata CTest 3/3.

Runtime metadata record query update (2026-06-19 00:12:05 +08:00): `function_metadata_query.c` now exposes read-only lookup helpers for function-level metadata records and entry-function module ref table records: `ZrCore_Function_FindMetadataTokenRecord()`, `ZrCore_Function_FindMetadataSignatureRecord()`, `ZrCore_Function_FindModuleMetadataTokenRecord()`, and `ZrCore_Function_FindModuleMetadataSignatureRecord()`. Signature lookup requires the entity/signature pair to mutually point through `relatedToken` plus `ownerToken`, and does not alter `.zro` schema or binding sidecars. Verification covered WSL GCC/clang `zr_vm_metadata_runtime_query_test` 3/0 and WSL GCC/clang plus MSVC Debug CTest `metadata_token_model|metadata_type_ref_binding|metadata_runtime_query|metadata_module_hash_golden` 4/4.

TypeRef binding update (2026-06-18 19:02:36 +08:00): `ZrCore_Function_BindMatchingTypeRefMetadata()` now records caller `TYPE_REF -> provider TYPE_DEF / SIGNATURE` binding when the caller TypeRef already carries stable provider TypeDef target identity. The matcher checks TypeDef token, paired signature hash/token, module hash, layout identity, and base name before writing the sidecar. Verification covered WSL clang/gcc `zr_vm_metadata_type_ref_binding_test` 1/0, `zr_vm_metadata_token_model_test` 21/0, `zr_vm_project_import_canonicalization_test` 31/0, plus MSVC Debug `zr_vm_metadata_type_ref_binding_test` 1/0. Current import owner-chain TypeRefs remain module-member placeholders until producer-side stable TypeRef extraction is added.

TypeRef producer update (2026-06-18 20:11:43 +08:00; nested generic refresh 2026-06-18 20:44:02 +08:00; explicit annotation refresh 2026-06-18 22:35:57 +08:00): `compiler_metadata_type_ref.c/.h` now emits stable provider `TYPE_REF` records separately from the import owner-chain placeholder. The helper scans import target signature return/parameter types and module-qualified typed local annotations, recursively enters generic arguments, resolves provider `TYPE_DEF` identity from `SZrParserModuleInitSummary.typeDefs`, writes provider TypeDef token/signature/module/layout target fields, and lets the existing TypeRef binding helper record `TYPE_REF -> TYPE_DEF` sidecars. `module_init_analysis.c/.h` refreshes source summaries with TypeDef records after metadata token construction, and `compiler_metadata_type_def.c` recursively discovers provider TypeDefs nested in exported generic signatures. Verification covered WSL GCC/clang `zr_vm_metadata_type_ref_binding_test` 6/0, `zr_vm_metadata_token_model_test` 21/0, `zr_vm_project_import_canonicalization_test` 31/0; MSVC Debug `zr_vm_metadata_type_ref_binding_test` 6/0.

Shared string heap validation update (2026-06-18 11:40:20 +08:00): `tests/parser/test_project_import_canonicalization.c` now resolves signature blob string references through `SZrFunction.metadataStringHeap` when present and keeps the legacy inline string fallback for older blobs. Recursive TypeSig skipping carries the owning `SZrFunction`, so project-import checks no longer misread shared heap indexes as inline lengths. WSL GCC verification: `zr_vm_project_import_canonicalization_test` 29/0, `zr_vm_metadata_token_model_test` 14/0, `zr_vm_gc_test` 66/0; focused compiler integration using/ownership/plugin guard cases PASS, with the full integration target still at the existing 115/23 baseline.

Shared metadata string heap update (2026-06-18 11:42:19 +08:00): `SZrMetadataStringHeapEntry` and `metadataStringHeap` are now part of the function metadata model. `.zro` patch 30 writes the heap after `signatureBlobHeap`; `io_runtime.c` copies it to runtime `SZrFunction`; signature blobs write fixed `u32` string indexes for module/type/symbol names; runtime verification decodes patch 30 heap indexes and falls back to legacy inline strings for older artifacts. WSL clang/gcc focused verification: `zr_vm_metadata_token_model_test` 14/0, `zr_vm_project_import_canonicalization_test` 29/0, metadata CTest 1/1.

## Compile-Time Metadata Build

[compiler_typed_metadata.c](../../zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c) 把本来只存在于短生命周期 `semanticContext` / `typeEnv` 里的结果汇总回 `SZrFunction`：

- 局部变量优先从 `typeEnv` 查推断类型
- 顶层函数声明如果已经离开原始表达式路径，仍会回填为 `function` / `closure`
- 导出变量从 `typeEnv` 查值类型
- 导出函数从 AST declaration 读取返回类型和参数类型；同名 overload 会优先通过 `callableChildIndex` 和 child function 源码范围绑定到正确 declaration，避免复用首个同名函数签名
- inline struct/union frame layout id 写入运行时序列化 `prototypeData` 的 prototype index，而不是编译期临时 type prototype 数组 index；这样运行时 resolver 能在过滤掉非序列化占位 prototype 后仍命中正确 layout

结果是：script entry function 自身就能携带 import 所需的基础签名信息，而不需要保留完整 AST 或执行模块。

## Callable Alias Export Signatures

source module 的 init summary 与最终 `typedExportedSymbols` 现在都把“导出变量别名到 imported callable”的形态保留为 callable signature：

```zr
var b = %import("b");
pub var other = b.pong;
```

这类导出在 runtime 初始化顺序上仍然是普通 `VALUE` export，readiness 不会被提升成 declaration-ready function。但 typed metadata 侧会把 `symbolKind` 标记为 `FUNCTION`，并复制目标 callable 的返回类型与参数类型。

这条规则服务两个路径：

- cyclic source summary：`module_init_analysis.c` 在构建 callable catalog 后修正 public/protected imported callable alias 的签名。
- runtime/binary metadata：`compiler_typed_metadata.c` 在落 `typedExportedSymbols` 时解析一跳 `moduleAlias.member` / `%import("module").member`，避免编译后的 source import 把别名退化成 `object`/`null`。

因此 importer 对 `a.other()` 的类型判断可以继续拿到真实返回类型，同时 module init safety 仍由原来的 export kind/readiness 约束负责。

## Module Entry Effects From Using Guards

`module_init_analysis.c` also treats plugin-guard `using` bodies as scoped import alias regions:

```zr
pub func run(): i32 {
    using (var helper = %import(".helper.math")) {
        return helper.answer;
    } else {
        return 0;
    }
}
```

While scanning the guarded body, `helper` is temporarily bound as a module alias for the canonical import key, so member references such as `helper.answer` are recorded in the callable summary's `moduleEntryEffects` under the canonical project key, for example `feature/app/helper/math` instead of raw `.helper.math`. The alias scope is restored before scanning `else`, so the guard binder does not leak into fallback code. During finalize, exported callable summaries with a resolved `callableChildIndex` copy those effects into the matching child function's `moduleEntryEffects`.

Typed and no-annotation plugin guard imports can also be expressed through the union default validation variant surface:

```zr
union DynamicModule<T> where T: zr.Module {
    Unavailable;
    @Available(m: Module);
}

using (var [m]: DynamicModule<Plugins> = %import("zr.plugins")) {
    return m.answer;
} else {
    return 0;
}

using (var [m] = %import("zr.plugins")) {
    return m.answer;
} else {
    return 0;
}

using (var [m]: PluginLoad.Available = %import("zr.plugins")) {
    return m.answer;
} else {
    return 0;
}
```

`@Available` is the single default variant for `DynamicModule<T>`, so a typed `using` annotation may name only `DynamicModule<Plugins>` and destructure the default tuple payload as `var [m]`; explicitly naming `DynamicModule<Plugins>.Available` remains accepted only because `Available` is marked with `@`. If a `%import` annotation names a non-default variant, the compiler rejects it instead of treating that variant as the import success payload. For `%import` resources with tuple/object destructuring and no annotation, the compiler resolves the current script's `DynamicModule` default variant before using the same guard helper. `PluginLoad.Available` is also accepted as a built-in annotation surface for the same success payload, so it does not require a user-declared `DynamicModule` union. The bare `Module` payload type is parsed as an implicit builtin type for this declaration surface, matching the guarded import helper that binds the imported module object before the block body is scanned. Typed, no-annotation, and `PluginLoad.Available` payload bindings are also registered with the plugin escape scanner before the block is compiled, so returning `m`, passing it out, using it as a switch case expression call argument such as `(sink(m))`, storing it in aggregates or template string interpolation, closure-capturing it, assigning it inside an inner region to an outer local that later escapes, or assigning it through an expression side effect such as `if (alias = m)` / `var ok = (alias = m)` without an explicit `share()` path reports `plugin_type_escape`. The explicit escape path is implemented for guarded module handles: no-arg `m.share()` lowers to `ZR_IO_NATIVE_HELPER_OWNERSHIP_SHARE_PLAIN` / `ZrCore_Ownership_NativeSharePlain`, keeps `m` as the guard-scoped plain module binding, and returns a releasable shared owner handle. Guard success also creates a hidden shared owner for the same plain module handle and registers scope cleanup, so leaving the guard scope emits `OWN_RELEASE` without changing the visible payload. Native registry ownership tracking observes both explicit and hidden shared owners through the core ownership strong-ref observer; `ZrLibrary_NativeRegistry_GetModuleRefCount()` returns to zero after the final shared owner release.

The token builder then promotes import read/ref/call effects with canonical module and symbol names into function-level `ASSEMBLY_REF`, `TYPE_REF`, and `MEMBER_REF` records paired with `SIGNATURE` records. The record ownership chain is explicit: `AssemblyRef.ownerToken = 0`, `TypeRef.ownerToken = AssemblyRef.token`, `MemberRef.ownerToken = TypeRef.token`, and each paired signature record points back to its entity token. Exported signatures now also emit baseline `TYPE_SPEC` records for closed/composite types found in return and parameter types, with paired `SIGNATURE` records over the same canonical type signature blob. Non-union generic instances encode as `GENERIC_INST(TYPE_REF(open), args...)`, while known union instances keep the union-specific `UNION` node. The paired member signature blob starts with `ZR_METADATA_SIGNATURE_NODE_MEMBER_REF`, followed by the canonical module name, symbol name, and effect kind; if the provider export can be resolved from source or binary summary, it appends a target `METHOD_SIG` or `FIELD_SIG` and records the corresponding `targetMetadataToken`, `targetSignatureToken`, and `targetSignatureHash`. String fields in those blobs are now represented through the shared metadata string heap, and project-import verification resolves them through `SZrFunction.metadataStringHeap` with an inline-string fallback for older blobs. The effect and `AssemblyRef` record also carry `targetModuleSignatureHash` when the provider module ABI fingerprint is known, plus requested/min/max semantic module version strings. The default version range is `[compiledVersion,nextMajor)`, and dependency manifest declarations can override min/max with `minVersionInclusive` / `maxVersionExclusive` without adding `%import` parser syntax. The method sub-signature includes version, call convention, generic arity, return type, parameter count, and parameter mode/type pairs. Provider `MEMBER_DEF` signatures and import-ref target signatures now use the same encoder, so the target hash can be compared directly against the appended target bytes and provider export hash. Module-qualified typed local annotations such as `provider.Option<int>` also feed `compiler_metadata_type_ref.c`; when no import effect names that provider, token construction emits an explicit `ASSEMBLY_REF` so the stable provider `TYPE_REF` has a real owner. `compiler_metadata_ref.c` aggregates import-ref records into `moduleMetadataTokenRecords`; runtime verification searches the module ref table for a matching `MEMBER_REF`, uses that record to fill missing effect version/target identity, checks the provider module version against the AssemblyRef range, checks a nonzero target module hash against the provider entry function `moduleSignatureHash`, checks nonzero target metadata/signature tokens against the provider typed export tokens, searches the module ref table first for expected target bytes, and falls back to the function-level records only when needed for older artifacts. Guard-style `%import` lowering uses `ZrCore_Module_ImportGuardNativeEntry`, so runtime guard import now consumes those target identities before entering the block: if the provider module version, module hash, export token/hash, appended target signature bytes, or provider availability no longer matches the compiled target identity, the import guard returns null and execution falls through to `else`. Ordinary required import uses the same verification but distinguishes load, version, module, and member failures: provider unavailability raises `import_load_unavailable`, version range drift raises `assembly_version_mismatch` with min/max/actual version context, module ABI hash drift raises `assembly_signature_mismatch` with `expectedModuleHash` / `actualModuleHash`, while member token/hash/bytes drift raises `import signature mismatch` with module/member/hash context. Registered native providers share the same verifier, so target hash drift is covered for `zr.math`-style modules. Successful verification records a `moduleMetadataBindings` sidecar entry from the original caller `MEMBER_REF` token to the resolved provider `MEMBER_DEF` / `SIGNATURE` tokens and hashes. `ZrCore_Function_FindModuleMetadataBinding()` exposes that sidecar by caller ref token for runtime queries, and `.zro` patch 28 persists it across binary roundtrip. The same sidecar now also records matching caller/provider `TYPE_SPEC` records by canonical signature hash plus blob equality after successful import verification, and unmatched caller TypeSpec records are reported through the module-load diagnostic channel as `type_spec_mismatch` without failing the import. Stable provider `TYPE_REF -> TYPE_DEF` binding now follows the same best-effort pattern through `type_ref_mismatch` diagnostics. When a guarded effect belongs to an exported callable, the compile-time attach step copies it to the child function, so the same actual-caller lookup checks it; runtime does not scan every child function. Project source-loader, native descriptor-plugin load-error, and AOT descriptor validation diagnostics now flow through the core module-load diagnostic channel. Registry owner refcount integration is available through `ownerRefCount` / `ZrLibrary_NativeRegistry_GetModuleRefCount()`, and descriptor plugin invalidation/reload rejects live owner refs before unloading plugin handles or clearing cache. TypeSpec records are deduplicated locally by canonical signature and have a baseline cross-module binding plus loader-facing mismatch diagnostic path. Complete cross-region/global/async plugin type escape semantics remain separate follow-up work.

Scoped plugin release status update (2026-06-18 08:44:43 +08:00): the compiler now creates a hidden shared owner when a plain plugin guard or default `DynamicModule<T>.@Available` guard succeeds, and registers `OWN_RELEASE` scope cleanup for that hidden owner.

PluginLoad surface status update (2026-06-18 09:10:57 +08:00): `PluginLoad.Available` is now accepted as a built-in `%import` guard annotation and lowers to the same available payload path as `DynamicModule<T>.@Available`, including `.share()` and hidden scoped owner cleanup.

Registry owner refcount status update (2026-06-18 09:56:09 +08:00): the native registry now observes ownership strong-ref deltas for module objects and exposes per-module `ownerRefCount` through `ZrLibRegisteredModuleInfo` and `ZrLibrary_NativeRegistry_GetModuleRefCount()`. Descriptor safe unload/cache invalidation remains separate lifecycle work.

## Import Normalization Flow

`type_inference_import_metadata.c` 把三类 import 统一映射到同一套 compile-time prototype/type metadata：

1. native import
   - 继续从 `__zr_native_module_info` 拿原始声明
   - 再归一化成编译器自己的 prototype/member/type 视图
   - module-level members 获得 importer-local synthetic `MEMBER_DEF` / `SIGNATURE` token 与 stable signature hash，供后续 hash-first ref→def 绑定使用
2. source import
   - 走“只编译不执行”的装载路径
   - 直接读取被导入脚本的 `typedExportedSymbols`、`typedLocalBindings` 和 prototype data
3. binary import
   - 通过 `.zro` 读出 `SZrIoFunctionTyped*` 结构
   - 恢复成和 source import 一致的导出符号与 prototype/member 信息

source/binary import 的函数成员不会再按名字去重。若 provider 导出多个同名函数，import view 会保留多个 `SZrTypeMemberInfo` 候选；runtime/IO parameter metadata 复制时会保留 typed export 已经 materialize 的 `parameterTypes`，只补齐名称、default 标记和 default value。member chain 的下一段是 call 时，`type_inference_member_resolution.c` 会整理位置参数、命名参数和 default 参数，按 exact/compatible/incompatible 评分选择最合适的候选。没有 typed 候选时仍回退到既有 native callable lookup；generic callable 没有匹配时继续保留既有 generic 诊断。

统一之后，编译器不再关心模块来自 native、source 还是 binary，只看：

- 导出了哪些变量 / 函数
- 函数参数与返回类型是什么
- prototype 上有哪些字段 / 方法
- 成员调用返回什么类型

## Native FFI Wrapper Metadata

native module `types[]` entry 现在还会额外公开一组 FFI wrapper 字段，用来描述“这个 native type 在 FFI 边界该怎么 lowering”：

- `ffiLoweringKind`
- `ffiViewTypeName`
- `ffiUnderlyingTypeName`
- `ffiOwnerMode`
- `ffiReleaseHook`

这组字段在两个层面同时存在：

- module info object 的公开 metadata，供 `%type` / LSP / 调试器 / 测试读取
- runtime prototype 上的隐藏字段 `__zr_ffi*`，供 marshalling helper 直接读取

当前仓库里已经接入的 built-in wrapper 例子：

- `PointerHandle`
  - `ffiLoweringKind = "pointer"`
  - `ffiOwnerMode = "borrowed"`
- `BufferHandle`
  - `ffiLoweringKind = "pointer"`
  - `ffiOwnerMode = "owned"`

因此 typed metadata 现在不只描述“一个 type 叫什么、有哪些成员”，还会描述“它是不是一个带 native lowering 语义的 wrapper type”。

这条信息目前主要服务两条链路：

- `zr.ffi` runtime 在参数 marshalling 时判定 wrapper 是否允许直接降低为 native pointer
- parser/type inference 在 `%extern` 函数 overload 选择时，只对 FFI/native boundary 放宽 wrapper-compatible 参数检查

同一套 metadata 现在也支持 source-level wrapper class，而不是只覆盖 native registry 内建类型。

## Source Wrapper Metadata Bridge

source module 里的 wrapper class 现在可以直接声明：

```zr
#zr.ffi.lowering("handle_id")#
#zr.ffi.viewType("ModeHandleView")#
#zr.ffi.underlying("i32")#
#zr.ffi.ownerMode("borrowed")#
#zr.ffi.releaseHook("close_mode_handle")#
class ModeHandle {
    var handleId: i32;
}
```

这组 decorator 在 metadata 管道里的流向已经打通成一条闭环：

1. `compiler_class.c`
   - 把 `zr.ffi.*` wrapper decorators 编译成 type decorator metadata object
   - 同时把 decorator 名称写进 prototype decorator name list
2. `prototypeData`
   - 通过 `SZrCompiledPrototypeInfo.hasDecoratorMetadata` / `decoratorMetadataConstantIndex` 持久化
3. `%type(...)` / reflection
   - source type reflection 的 `metadata` 和 `decorators[]` 可以直接读到 `ffiLoweringKind` / `ffiViewTypeName` / `ffiUnderlyingTypeName` / `ffiOwnerMode` / `ffiReleaseHook`
4. `module_prototype.c`
   - 在 runtime materialize prototype 时，再把这组公开 metadata 桥接成隐藏字段：
     - `__zr_ffiLoweringKind`
     - `__zr_ffiViewTypeName`
     - `__zr_ffiUnderlyingTypeName`
     - `__zr_ffiOwnerMode`
     - `__zr_ffiReleaseHook`

这样 source wrapper 和 native descriptor wrapper 在 runtime 上共享同一套 hidden metadata contract。

## `handle_id` In Typed Metadata

`handle_id` 是这轮补全的第三种 lowering kind（除了已有的 `pointer` 与未来的 `value`）：

- compile-time/type inference 层：
  - 当 extern/native boundary 目标参数是整数标量，且 wrapper metadata 的
    - `ffiLoweringKind == "handle_id"`
    - `ffiUnderlyingTypeName` 与目标整数类型同名
    时，overload 选择和参数检查会把 wrapper 视为边界兼容
  - source-level wrapper decorator 当前只接受固定宽度整数名
    `i8/u8/i16/u16/i32/u32/i64/u64` 作为 `ffiUnderlyingTypeName`
  - 若声明了 `ffiViewTypeName`，source wrapper decorator 现在要求它指向同一 source file 中的
    `%extern struct`
  - 普通 zr 函数调用仍然不兼容，不会隐式转成整数
- runtime marshalling 层：
  - `ffi_runtime_callback.c` 在构造 scalar argument 时读取 prototype hidden metadata
  - 若 lowering kind 为 `handle_id` 且目标 ABI 标量类型匹配 underlying 名称，则从对象字段
    - `__zr_ffi_handleId`
    - 或 `handleId`
    取值并降为 ABI 整数

这让 typed metadata 不再只是“类型说明书”，而是 source/native wrapper 共享的 lowering contract。

## Typed Opcode Selection

M6 的 codegen 行为是“先推断，再转化，再选 opcode”。

当前已收口的路径包括：

- 二元算术
- 比较
- 赋值
- 自由函数调用
- imported module 成员调用

具体规则：

- 若两侧都是已知 `int`，生成 `ADD_INT` / 对应整数比较指令
- 若目标签名要求 `float`，先插 `TO_FLOAT`，再生成 typed call
- 若类型未知、动态成员访问、或当前路径无法给出稳定签名，退回现有 generic opcode / meta fallback

这意味着 typed path 只在“真的知道类型”时变精确，不会为了追求闭环而引入兼容分支。

### Native Member Call Fusion Boundary

`KNOWN_NATIVE_CALL` 只有在 callable slot 真的是 receiver member get 的结果时，才允许和前置 `GET_MEMBER_SLOT` 融合为 `KNOWN_NATIVE_MEMBER_CALL`。

static native accessor 是例外：例如 `TypeInfo.box(7)` 会通过 member slot 拿到 callable，但调用帧里没有 receiver 对象需要重写。`compile_expression_support.c` 会把这类 member entry 标成 `ZR_FUNCTION_MEMBER_ENTRY_FLAG_STATIC_ACCESSOR`，`compiler_quickening.c` 据此跳过 receiver-style native member-call fusion。

这条边界保留了实例 native member 的 fast path，也避免 static accessor 被误当成实例调用，导致 runtime 从第一个参数槽读取 receiver。

## Artifact Model

LSP / compiler 侧正式消费的 typed metadata 载体固定为 `.zro`。

`.zro` 通过 `SZrIoFunctionTypedTypeRef` / `SZrIoFunctionTypedLocalBinding` / `SZrIoFunctionTypedExportSymbol` 序列化 typed metadata，
其中 `typedExportedSymbols` 额外持久化 declaration line/column span，供 binary metadata declaration resolver 直接命中导出符号，而不是只能回退到 module entry。

从 patch 20 起，`.zro` 还会持久化 typed export 的 metadata token 与 signature token，并追加函数级 signature blob heap。patch 21 在这些字段后追加 typed export 与 metadata token record 的 `signatureHash`。patch 22 在 metadata token record 中追加 `ownerToken`。patch 23 在 metadata token record 中追加 `targetSignatureHash`，并在 module effect 中追加 `targetMetadataToken`、`targetSignatureToken`、`targetSignatureHash`。patch 24 在 metadata token record 中追加 `targetMetadataToken` 和 `targetSignatureToken`，让 `MEMBER_REF` 及配对 `SIGNATURE` record 也能保留 provider def/signature token。patch 25 在 signature blob heap 后追加 `moduleMetadataTokenRecords`，保存 entry-function 级 import ref 聚合表。patch 26 在 signature blob heap 后、module ref table 前追加 `moduleSignatureHash`。patch 27 在 module effect 和 metadata token record 中追加 `targetModuleSignatureHash`。patch 28 在 module ref table 后追加 runtime `moduleMetadataBindings` binding result list。patch 29 在 metadata token model 中追加 provider `moduleVersion`，并在 module effect 与 metadata token record 中追加 AssemblyRef requested/min/max version 字符串。patch 30 在 signature blob heap 后追加 metadata string heap，signature blob 中的字符串引用改为固定 `u32` heap index。patch 31 在 metadata token record 的 `signatureHash` 后追加 record-level `layoutVersion` / `layoutHash`。patch 32 在 binding result 的 expected/resolved signature/module hash 后追加 expected/resolved `layoutVersion` / `layoutHash`。

旧 patch 产物读取时缺失区会保守置 0、null 或空表：patch 20 产物 token/blob 可用但 hash、owner、target identity、module ABI hash、module ref table、binding result list、version fields、metadata string heap、record layout identity 与 binding layout identity 为空；patch 21/22/23/24/25/26/27/28/29 按各自已存在字段读取，其后新增字段置 0/null/空；patch 30 产物 metadata string heap 可用但 record layout identity 与 binding layout identity 置 0；patch 31 产物 record layout identity 可用但 binding layout identity 置 0。patch 32 写出的产物双写“原 typed metadata + token/signature/hash/owner/target identity + module version + AssemblyRef version range + module ABI hash + target module hash + module ref table + runtime binding result list + metadata string heap + record layout identity + binding layout identity 结构”，保证当前 import 消费路径不被迫一次性迁移。高于 `ZR_IO_SOURCE_PATCH_CURRENT` 的 future patch 会在 header 阶段被拒绝并带出 actual/supported patch 诊断。

`.zri` 继续由 [writer_intermediate.c](../../zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c) 输出，保留：

- `TYPE_METADATA`
- `LOCAL_BINDINGS`
- `EXPORTED_SYMBOLS`
- 函数签名文本，例如 `fn add(int, int): int`

但 `.zri` 的职责已经收窄为 debug / intermediate artifact：

- 便于人读和 golden 回归
- 可承载与 `.zro` 对齐的 debug 线列范围信息
- 不再作为 language server / type inference / import metadata 的正式事实源

这条约束很重要：writer artifact 和 binary schema 可以表达相近信息，但语义入口只能有一条正式机器路径。任何 `.zro` schema 漂移都必须同步更新：

- `tests/golden/intermediate/*.zri`
- `tests/golden/binary/*.zro`
- `tests/fixtures/projects/import_binary/bin/greet.zro`

## Runtime Constraint For Typed Calls

typed opcode 只在运行时也守住调用栈边界时才成立。

本轮暴露出的底层约束是：`TO_INT` / `TO_UINT` / `TO_FLOAT` 走 meta conversion 时，scratch 区不能从 `state->stackTop` 直接复用。因为 `stackTop` 可能仍落在当前帧的 live temp 区间之前，错误复用会覆盖掉 imported callable 所在槽位。

当前修复方式与 `TO_BOOL` 保持一致：

- meta call scratch 上界使用 `callInfo->functionTop.valuePointer`
- 标记使用安全 scratch 区域

这样 `math.add(1, 2.5)` 这类 imported typed call 的 `TO_FLOAT` 不会再把 callable 自己覆盖成 `float`。

## Edge Cases And Constraints

- 没有可恢复类型的导出变量仍会降级为 `object`
- 数组类型元数据当前只记录统一元素类型视图，符合 M6 现阶段的数组元素推断需求
- `typedExportedSymbols` 只描述模块对外可见的符号，不替代 prototypeData
- 本轮不实现 typed IR / AOT backend，只保证元数据稳定落盘并可被 import / writer / runtime 验证
- M6 早期 typed metadata schema 变更仍以重生成仓库内 fixture 为准；metadata token patch 20 对缺少 token 区的旧产物按空 token model 读取，patch 21 对旧 token model 产物把 `signatureHash` 置 0，patch 22 对旧 token/hash 产物把 `ownerToken` 置 0

## Test Coverage

M6 的验证不是“编译成功”级别，而是直接断言 opcode、签名和运行结果。

- [tests/parser/test_type_inference.c](../../tests/parser/test_type_inference.c)
  - source import 签名 pass / fail
  - binary import 签名 pass / fail
  - source/binary import 同名函数候选保留和调用级签名选择
  - imported member chain 返回类型
  - imported array assignment 类型拒绝
- [tests/parser/test_compiler_features.c](../../tests/parser/test_compiler_features.c)
  - mixed-type call 的 `TO_FLOAT`
  - `.zri` `TYPE_METADATA` section
  - opcode 精确选择
- [tests/parser/test_instruction_execution.c](../../tests/parser/test_instruction_execution.c)
  - source import typed call 真实执行结果
  - binary import typed call 真实执行结果
- [tests/scripts/test_artifact_golden.c](../../tests/scripts/test_artifact_golden.c)
  - `.zrs` / `.zri` / `.zro` golden 回归
- [tests/module/test_metadata_token_model.c](../../tests/module/test_metadata_token_model.c)
  - metadata token 分配
  - signature blob heap 编码
  - `signatureHash` 非零、同签名重建稳定、ABI signature 变化时 hash 变化
  - `moduleSignatureHash` 使用 public exports + `TYPE_DEF` / `TYPE_SPEC` identity；同导出函数签名但本地 union TypeDef contract/layout 漂移时 module hash 变化
  - union type signature node 编码
  - TypeSpec records for closed/composite exported signature types, local TypeSpec deduplication, baseline cross-module TypeSpec binding by canonical signature, and consumer-side TypeRef to TypeDef binding/status/diagnostics for stable TypeRef records
  - import ref 的 `ASSEMBLY_REF` / `TYPE_REF` / `MEMBER_REF` owner-chain，以及配对 `SIGNATURE` owner 指回
  - `.zro` 写入/读取 roundtrip 保留 token/hash
  - runtime loader copy 保留 token/hash、owner chain、target metadata/signature token、target signature hash identity 和 module-level import ref 聚合表
- [tests/module/test_metadata_type_ref_binding.c](../../tests/module/test_metadata_type_ref_binding.c)
  - stable provider `TYPE_REF -> TYPE_DEF` binding
  - TypeRef definition/layout mismatch status and `type_ref_mismatch` diagnostic behavior
  - provider summary backed TypeRef production from target signatures, nested generic arguments, module-qualified annotations such as `provider.Option<int>`, and destructured/unqualified alias annotations such as `Option<int>`
- [tests/module/test_metadata_runtime_query.c](../../tests/module/test_metadata_runtime_query.c)
  - function-level and entry-function module ref table token/signature record queries
  - null/zero-token handling and loose signature-pair rejection
- [tests/module/test_metadata_module_hash_golden.c](../../tests/module/test_metadata_module_hash_golden.c)
  - cross-toolchain stable module ABI fingerprints for a simple exported function and a local generic union export
- [tests/parser/test_union.c](../../tests/parser/test_union.c)
  - union prototypeData serialization
  - variant member metadata shell
  - inline union tag/payload materialization, local copy/assignment, and owner payload release on active variant frame drop
  - typed, explicit-variant, and no-annotation `DynamicModule<T>.@Available` `%import` guard positive lowering
- [tests/parser/test_compiler_features.c](../../tests/parser/test_compiler_features.c)
  - typed and no-annotation `DynamicModule<T>.@Available` `%import` payload return escape is rejected with `plugin_type_escape`
  - no-annotation payload assignment through an inner `if`/block into an outer local remains plugin-tainted and is rejected on later return
  - plugin guard assignment expressions inside conditions and variable initializers propagate alias taint and are rejected on later return
  - switch case expressions that pass a guard handle as a call argument are rejected with `plugin_type_escape`
  - template string interpolation that stores a guard handle is rejected with `plugin_type_escape ... through field/container`
  - guarded module `.share()` emits the ownership share helper, keeps the guard module binding live, and returns a releasable shared owner
  - plugin guard scope exit emits hidden shared-owner `OWN_RELEASE` cleanup for both plain guard and default `@Available` payload paths
  - built-in `PluginLoad.Available` `%import` guard annotation lowers to the same available payload path without a user-declared `DynamicModule` union
- [tests/parser/test_project_import_canonicalization.c](../../tests/parser/test_project_import_canonicalization.c)
  - `using (var p = %import(...))` guarded body records canonical `moduleEntryEffects`
  - guarded import member effects generate function-level ref records, member-ref signature blobs, paired `signatureHash`, explicit `AssemblyRef <- TypeRef <- MemberRef` ownership, provider `targetMetadataToken` / `targetSignatureToken` / `targetSignatureHash`, and provider-derived `METHOD_SIG` parameter counts
  - guard-style `%import` uses the import-guard native helper; corrupted provider target signature hash, appended target signature blob mismatch, runtime provider unavailability, and nested callable caller hash mismatch make execution take the `else` fallback
- [tests/cmake/run_projects_suite.cmake](../../tests/cmake/run_projects_suite.cmake)
  - `hello_world`
  - `import_basic`
  - `import_binary`
  - `import_pub_function`

## Plan Sources

本实现直接对应：

- `M6 强类型推断完整闭环计划`
- `zr_vm阶段化总计划` 中关于强类型推断、import 和产物闭环的阶段目标

## Open Follow-Up

- AOT / typed IR 仍未消费这批元数据
- 更细粒度的多元素数组类型表达和 AOT/typed IR 专用消费仍未进入 `.zro` schema；当前 metadata/token 结构已经覆盖非 union 泛型闭型、union TypeDef/TypeSpec identity、stable provider TypeRef、module-qualified typed annotations 和 destructured/unqualified alias annotations
- `import_pub_function` fixture 仍带有与本轮无关的旧语法诊断噪音，当前不影响结果验证，但后续应独立清理
