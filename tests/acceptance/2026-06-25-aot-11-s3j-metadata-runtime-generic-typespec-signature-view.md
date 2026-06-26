# AOT 11-S3J · Metadata Runtime Generic TypeSpec Signature View

时间：2026-06-25 15:34:40 +08:00

## 状态

11-S3 子切片完成。完整 11-S3 仍未关闭：generic semantic binding、row-to-runtime entity
materialization、token→运行期实体物化和完整缓存仍待后续。

## 完成项目

- 新增 `SZrMetadataRuntimeTypeSpecSignatureView`。
- 新增 `ZrCore_MetadataRuntime_ReadTypeSpecSignatureView(runtime, typeSpecToken, outView)`。
- 该 view 从 `TYPE_SPEC` token 串起 type record、paired `SIGNATURE` record、validated signature
  blob、root `GENERIC_INST` type-node 和 base `TYPE_REF/TYPE_DEF` node。
- 暴露 TypeSpec token、signature token/hash、blob slice、generic root node、base node、
  argument count 和 argument-list blob offset。
- 失败路径覆盖空 runtime/out view、非 `TYPE_SPEC` token、未 attached zrp metadata、非
  `GENERIC_INST` 根节点或非法 base node。

## RED / GREEN

- RED：`zr_vm_metadata_runtime_query_test` 新增
  `test_metadata_runtime_reads_generic_type_spec_signature_view` 后编译失败，缺少
  `SZrMetadataRuntimeTypeSpecSignatureView` 与
  `ZrCore_MetadataRuntime_ReadTypeSpecSignatureView()`。
- GREEN：新增 view carrier 和 reader 后，合法 `GENERIC_INST(TYPE_REF, INT64)` TypeSpec 签名
  view 通过，非法输入均被拒绝。

## 验证

- WSL gcc：
  - `zr_vm_metadata_runtime_query_test` 14/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
- WSL clang：
  - `zr_vm_metadata_runtime_query_test` 14/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
  - 仍有既有 generated-C `!zr_aot_b2 != 0u` clang warning。
- Windows MSVC Debug：
  - `zr_vm_metadata_runtime_query_test` 14/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0，8 ignored Unix-only 分支
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0，2 ignored Unix-only 分支

## 边界

本切片只关闭 TypeSpec generic signature 的只读身份/结构 view；不声明 base token 标准化、
layout/type entity 物化、generic dictionary 解析、MethodSpec 解析、row-to-runtime entity
materialization、反射实体构造或 code stripping metadata policy 完成。
