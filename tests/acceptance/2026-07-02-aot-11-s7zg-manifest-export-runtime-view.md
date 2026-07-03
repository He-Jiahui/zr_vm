# AOT 11-S7ZG / 12-S7 Manifest Export Runtime View

时间：2026-07-02 04:23:15 +08:00

## 范围

- 计划切片：11-S7ZG / 12-S7 manifest export runtime view。
- 目标：在 11-S7ZF 已把 generated-C manifest export table 镜像到 attached `SZrMetadataRuntime` 后，增加稳定 runtime 查询 API，而不是让后续 consumer 直接读裸 pointer/count。
- 非目标：不实现 cross-module provider loading/version binding、standalone provider import-path wiring、完整 metadata sweep/pruning、完整 trim analyzer 或完整 runtime ABI drift/deopt 闭环。

## 完成项

- 新增 `SZrMetadataRuntimeManifestExportView`。
- 新增 `ZrCore_MetadataRuntime_ReadManifestExportView()`，按 `kind + target` 唯一读取 attached manifest export table。
- 返回 view 包含只读 entry pointer、entry index、kind、target、`typeToken` 和 `memberToken`。
- 对重复 target/kind、缺 required token flag、type/member token shape mismatch、空 runtime/table/output fail closed，并清空输出 view。
- 实现拆入 `zr_vm_core/src/zr_vm_core/metadata_runtime_manifest_exports.c`，避免继续扩大接近阈值的 `metadata_runtime.c`。
- `tests/module/test_metadata_runtime_manifest_exports.c` 从 mirror-only 扩展到 mirror + view 查询 + duplicate reject + missing-token reject。

## RED/GREEN

- RED：新增 runtime view 测试后，WSL GCC focused build 失败；错误集中在缺少 `SZrMetadataRuntimeManifestExportView` type 和 `ZrCore_MetadataRuntime_ReadManifestExportView()` API。
- GREEN：实现 public view/API 与 fail-closed lookup 后，`zr_vm_metadata_runtime_manifest_exports_test` 通过 4/0。

## 验证

- WSL GCC direct：
  - `zr_vm_metadata_runtime_manifest_exports_test` 4/0
  - `zr_vm_metadata_runtime_query_test` 25/0
  - `zr_vm_metadata_runtime_binding_compatibility_test` 15/0
- WSL GCC CTest：
  - `metadata_runtime_manifest_exports` 1/1
  - `metadata_runtime_query` 1/1
  - `metadata_runtime_binding_compatibility` 1/1
- WSL clang direct：
  - `zr_vm_metadata_runtime_manifest_exports_test` 4/0
  - `zr_vm_metadata_runtime_query_test` 25/0
  - `zr_vm_metadata_runtime_binding_compatibility_test` 15/0
- WSL clang CTest：
  - `metadata_runtime_manifest_exports` 1/1
  - `metadata_runtime_query` 1/1
  - `metadata_runtime_binding_compatibility` 1/1
- Windows MSVC Debug direct：
  - `zr_vm_metadata_runtime_manifest_exports_test.exe` 4/0
  - `zr_vm_metadata_runtime_query_test.exe` 25/0
  - `zr_vm_metadata_runtime_binding_compatibility_test.exe` 15/0
- Windows MSVC Debug CTest：
  - `metadata_runtime_manifest_exports` 1/1
  - `metadata_runtime_query` 1/1
  - `metadata_runtime_binding_compatibility` 1/1

## 剩余缺口

- Cross-module provider loading/version binding 未完成。
- Standalone provider import-path wiring 未完成。
- 完整 metadata sweep/pruning 未完成。
- 完整 trim analyzer 和 annotation/dataflow policy 未完成。
- 更完整的 ABI drift/deopt coverage 未完成。
