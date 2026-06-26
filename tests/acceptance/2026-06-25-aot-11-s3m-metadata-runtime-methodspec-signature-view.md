# AOT 11-S3M · Metadata Runtime MethodSpec Signature View

时间：2026-06-25 16:16:39 +08:00

## 状态

11-S3 子切片完成。完整 11-S3 仍未关闭：recursive generic argument binding、method
instantiation materialization、row-to-runtime entity materialization、token→运行期实体物化和完整缓存仍待后续。

## 完成项目

- 新增 `SZrMetadataRuntimeMethodSpecSignatureView`。
- 新增 `ZrCore_MetadataRuntime_ReadMethodSpecSignatureView(runtime, methodSpecToken, outView)`。
- MethodSpec 继续使用 `SIGNATURE` token；runtime 直接读取该 signature record 的 zrp signature blob。
- 要求 MethodSpec record 的 related/owner 指向 method token，并验证签名体为
  `GENERIC_INST(MEMBER_REF methodToken, args...)`。
- 暴露 methodSpec token、method token、method record、signature hash、method node、argument count 和
  argument-list blob offset。

## RED / GREEN

- RED：`zr_vm_metadata_runtime_query_test` 新增 MethodSpec signature view 用例后编译失败，缺少
  `SZrMetadataRuntimeMethodSpecSignatureView` 与
  `ZrCore_MetadataRuntime_ReadMethodSpecSignatureView()`。
- GREEN：新增 view carrier、direct signature-record blob helper 和 method record binding 后，空 out view、
  非 `SIGNATURE` token、未 attached zrp metadata 均拒绝；合法 MethodSpec signature view 可绑定到本地
  `MEMBER_DEF` method record。

## 验证

- WSL gcc：
  - `zr_vm_metadata_runtime_query_test` 18/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
- WSL clang：
  - `zr_vm_metadata_runtime_query_test` 18/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0
  - 仍有既有 generated-C `!zr_aot_b2 != 0u` clang warning。
- Windows MSVC Debug：
  - `zr_vm_metadata_runtime_query_test` 18/0
  - `zr_vm_aot_c_frame_setup_contracts_test` 1/0
  - `zr_vm_aot_c_source_contracts_test` 19/0
  - `zr_vm_aot_c_shared_library_smoke_test` 8/0，8 ignored Unix-only 分支
  - `zr_vm_aot_c_descriptor_diagnostics_test` 2/0，2 ignored Unix-only 分支

## 边界

本切片只关闭 MethodSpec signature 的只读身份/结构 view 与 method record binding；不声明 MethodSpec
token 编码变更、method instantiation 实体、generic dictionary、argument recursive binding、row-to-runtime
entity materialization、反射实体构造或 code stripping metadata policy 完成。
