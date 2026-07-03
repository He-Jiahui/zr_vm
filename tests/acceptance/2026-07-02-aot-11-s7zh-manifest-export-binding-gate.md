# AOT 11-S7ZH / 12-S7 Manifest Export Binding Gate

时间：2026-07-02 04:46:47 +08:00

## 范围

- 计划切片：11-S7ZH / 12-S7 manifest export binding gate。
- 目标：在 11-S7ZG 的 attached manifest export runtime view 之上，增加 provider binding 前可复用的本地兼容性 gate。
- 非目标：不实现 cross-module provider loading/version binding、standalone provider import-path wiring、完整 metadata sweep/pruning、完整 trim analyzer 或完整 runtime ABI drift/deopt 闭环。

## 完成项

- 新增 `ZR_METADATA_RUNTIME_BINDING_STATUS_MANIFEST_EXPORT_NOT_FOUND`。
- 新增 `ZR_METADATA_RUNTIME_BINDING_STATUS_MANIFEST_EXPORT_TOKEN_MISMATCH`。
- 新增 `ZrCore_MetadataRuntime_CheckManifestExportBindingCompatibility()`。
- 该 API 先按 `kind + target` 读取 manifest export view，再复用既有 token/signature/module/layout compatibility predicate，最后要求 binding 的 `resolvedMetadataToken` 与 export table 发布的 `typeToken` 或 `memberToken` 一致。
- `tests/module/test_metadata_runtime_manifest_exports.c` 扩展到 success、export token mismatch、missing export 和 version drift gate 覆盖。

## RED/GREEN

- RED：新增 binding gate 测试后，WSL GCC focused build 失败；错误集中在缺少 `ZrCore_MetadataRuntime_CheckManifestExportBindingCompatibility()` 和两个 manifest-export binding status。
- GREEN：实现 public API/status 与 export-token identity check 后，`zr_vm_metadata_runtime_manifest_exports_test` 通过 7/0，`zr_vm_metadata_runtime_binding_compatibility_test` 保持 15/0。

## 验证

- WSL GCC direct：
  - `zr_vm_metadata_runtime_manifest_exports_test` 7/0
  - `zr_vm_metadata_runtime_binding_compatibility_test` 15/0
- WSL GCC CTest：
  - `metadata_runtime_manifest_exports` 1/1
  - `metadata_runtime_binding_compatibility` 1/1
- WSL clang direct：
  - `zr_vm_metadata_runtime_manifest_exports_test` 7/0
  - `zr_vm_metadata_runtime_binding_compatibility_test` 15/0
- WSL clang CTest：
  - `metadata_runtime_manifest_exports` 1/1
  - `metadata_runtime_binding_compatibility` 1/1
- Windows MSVC Debug direct：
  - `zr_vm_metadata_runtime_manifest_exports_test.exe` 7/0
  - `zr_vm_metadata_runtime_binding_compatibility_test.exe` 15/0
- Windows MSVC Debug CTest：
  - `metadata_runtime_manifest_exports` 1/1
  - `metadata_runtime_binding_compatibility` 1/1

## 剩余缺口

- Cross-module provider loading/version binding 未完成。
- Standalone provider import-path wiring 未完成。
- 完整 metadata sweep/pruning 未完成。
- 完整 trim analyzer 和 annotation/dataflow policy 未完成。
- 更完整的 ABI drift/deopt coverage 未完成。
