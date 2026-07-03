# AOT 11-S7ZI / 12-S7 Provider Manifest Export Binding Gate

时间：2026-07-02 05:34:27 +08:00

## 范围

- 计划切片：11-S7ZI / 12-S7 provider manifest export binding gate。
- 目标：把 11-S7ZH 的 manifest export binding compatibility gate 接入真实 provider import signature verifier。
- 非目标：不实现 standalone provider import-path discovery/loading/version selection、完整 metadata sweep/pruning、完整 trim analyzer 或完整 runtime ABI drift/deopt 闭环。

## 完成项

- 新增 `module_import_signature_manifest_export.c/.h`。
- `module_import_signature.c` 在 provider typed export candidate 完成 token/signature/hash/blob 校验后调用 manifest export gate。
- gate 会把 import effect 映射到 manifest export `TYPE/METHOD/FIELD` kind，构造 `SZrMetadataTokenBinding` snapshot，并调用 `ZrCore_MetadataRuntime_CheckManifestExportBindingCompatibility()`。
- provider module 若 attached `SZrAotCodeRegistration.manifestExports`，则同 `kind + target` manifest entry 发布的 `typeToken/memberToken` 必须与选中的 typed export symbol metadata token 一致。
- provider 没有 manifest export table 时保留旧兼容行为，避免破坏旧产物加载。
- `tests/module/test_metadata_type_ref_binding.c` 新增 manifest export token drift 回归测试。

## RED/GREEN

- RED：新增测试构造 provider typed export `MEMBER_DEF(1)`，但 manifest export table 对 `Factory.make` 发布 `MEMBER_DEF(2)`；WSL GCC direct `zr_vm_metadata_type_ref_binding_test` 报 9 tests / 1 failure，旧 verifier 返回 true。
- GREEN：接入 `module_import_signature_manifest_export.c/.h` 后，同一测试通过 9/0，且 manifest export runtime 与 binding compatibility 相邻测试保持通过。

## 验证

- WSL GCC direct：
  - `zr_vm_metadata_type_ref_binding_test` 9/0
  - `zr_vm_metadata_runtime_manifest_exports_test` 7/0
  - `zr_vm_metadata_runtime_binding_compatibility_test` 15/0
- WSL GCC CTest：
  - `metadata_type_ref_binding` 1/1
  - `metadata_runtime_manifest_exports` 1/1
  - `metadata_runtime_binding_compatibility` 1/1
- WSL clang direct：
  - `zr_vm_metadata_type_ref_binding_test` 9/0
  - `zr_vm_metadata_runtime_manifest_exports_test` 7/0
  - `zr_vm_metadata_runtime_binding_compatibility_test` 15/0
- WSL clang CTest：
  - `metadata_type_ref_binding` 1/1
  - `metadata_runtime_manifest_exports` 1/1
  - `metadata_runtime_binding_compatibility` 1/1
- Windows MSVC Debug direct：
  - `zr_vm_metadata_type_ref_binding_test.exe` 9/0
  - `zr_vm_metadata_runtime_manifest_exports_test.exe` 7/0
  - `zr_vm_metadata_runtime_binding_compatibility_test.exe` 15/0
- Windows MSVC Debug CTest：
  - `metadata_type_ref_binding` 1/1
  - `metadata_runtime_manifest_exports` 1/1
  - `metadata_runtime_binding_compatibility` 1/1

## 剩余缺口

- Standalone provider import-path discovery/loading/version selection 未完成。
- 完整 metadata sweep/pruning 未完成。
- 完整 trim analyzer 和 annotation/dataflow policy 未完成。
- 更完整的 ABI drift/deopt coverage 未完成。
