# AOT 11-S3L · Metadata Runtime Generic TypeSpec Argument Binding View

时间：2026-06-25 16:04:29 +08:00

## 状态

11-S3 子切片完成。完整 11-S3 仍未关闭：nested/recursive generic argument binding、
MethodSpec runtime binding、row-to-runtime entity materialization、token→运行期实体物化和完整缓存仍待后续。

## 完成项目

- 新增 `SZrMetadataRuntimeTypeSpecGenericArgumentView`。
- 新增 `ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView(runtime, typeSpecToken, argumentIndex, outView)`。
- 该 view 复用 11-S3K 的 TypeSpec generic binding view，并按 index 遍历 root `GENERIC_INST`
  argument list。
- 暴露 argument type-node；直接 `TYPE_REF/TYPE_DEF` argument 会匹配现有 type record 并暴露
  argument token/record。
- primitive/复合 argument 节点保留结构视图，不声明递归语义绑定。

## RED / GREEN

- RED：`zr_vm_metadata_runtime_query_test` 新增 indexed generic argument binding 用例后编译失败，
  缺少 `SZrMetadataRuntimeTypeSpecGenericArgumentView` 与
  `ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView()`。
- GREEN：新增 view carrier 和 reader 后，空 out view、非 `TYPE_SPEC` token、未 attached zrp metadata、
  越界 argument index 均拒绝；合法 `GENERIC_INST(TYPE_REF base, INT64, TYPE_REF arg)` 可读取
  primitive argument 节点，并把第二个 direct `TYPE_REF` argument 绑定到 module type record。

## 验证

- WSL gcc：
  - `zr_vm_metadata_runtime_query_test` 17/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
- WSL clang：
  - `zr_vm_metadata_runtime_query_test` 17/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
  - 仍有既有 generated-C `!zr_aot_b2 != 0u` clang warning。
- Windows MSVC Debug：
  - `zr_vm_metadata_runtime_query_test` 17/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0，8 ignored Unix-only 分支
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0，2 ignored Unix-only 分支

## 边界

本切片只关闭 TypeSpec generic indexed argument 节点读取与直接 `TYPE_REF/TYPE_DEF` argument token
绑定；不声明嵌套/递归 argument 语义绑定、baseToken 编码标准化、layout/type entity 物化、
generic dictionary 解析、MethodSpec 解析、row-to-runtime entity materialization、反射实体构造或
code stripping metadata policy 完成。
