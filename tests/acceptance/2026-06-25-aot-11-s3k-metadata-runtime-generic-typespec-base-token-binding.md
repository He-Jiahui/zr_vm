# AOT 11-S3K · Metadata Runtime Generic TypeSpec Base-Token Binding View

时间：2026-06-25 15:48:39 +08:00

## 状态

11-S3 子切片完成。完整 11-S3 仍未关闭：generic argument semantic binding、MethodSpec runtime
binding、row-to-runtime entity materialization、token→运行期实体物化和完整缓存仍待后续。

## 完成项目

- 新增 `SZrMetadataRuntimeTypeSpecGenericBindingView`。
- 新增 `ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView(runtime, typeSpecToken, outView)`。
- 该 view 复用 11-S3J 的 TypeSpec signature view，并把 root `GENERIC_INST` 的 base
  `TYPE_REF/TYPE_DEF` node 与现有 type record 的 signature blob 匹配。
- 暴露 base token、base record 以及原 TypeSpec signature view。
- 覆盖 module `TYPE_REF` base record 与本地 `TYPE_DEF` base record 两条路径。

## RED / GREEN

- RED：`zr_vm_metadata_runtime_query_test` 新增 base-token binding view 用例后编译失败，缺少
  `SZrMetadataRuntimeTypeSpecGenericBindingView` 与
  `ZrCore_MetadataRuntime_ReadTypeSpecGenericBindingView()`。
- GREEN：新增 view carrier、reader 和 base record 匹配 helper 后，空 out view、非
  `TYPE_SPEC` token、未 attached zrp metadata 均拒绝，合法 `GENERIC_INST(TYPE_REF, INT64)` 与
  `GENERIC_INST(TYPE_DEF, INT64)` 分别绑定到 module `TYPE_REF` record 和本地 `TYPE_DEF` record。

## 验证

- WSL gcc：
  - `zr_vm_metadata_runtime_query_test` 16/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
- WSL clang：
  - `zr_vm_metadata_runtime_query_test` 16/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
  - 仍有既有 generated-C `!zr_aot_b2 != 0u` clang warning。
- Windows MSVC Debug：
  - `zr_vm_metadata_runtime_query_test` 16/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0，8 ignored Unix-only 分支
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0，2 ignored Unix-only 分支

## 边界

本切片只关闭 TypeSpec generic base node 到现有 type record 的只读绑定 view；不声明 baseToken
编码标准化、generic argument 语义绑定、layout/type entity 物化、generic dictionary 解析、
MethodSpec 解析、row-to-runtime entity materialization、反射实体构造或 code stripping metadata
policy 完成。
