# AOT 11-S4J TypeSpec Layout Binding View

## Scope

- Slice: 11-S4J, TypeSpec token -> zrp TypeSpec row -> generic binding -> registry layout view.
- Date: 2026-06-25 21:18:46 +08:00.
- Status: completed sub-slice; full 11-S4 remains open.
- This slice does not claim full TypeSpec/generic layout materialization, ownership offset emission, runtime layout construction, generic instantiation runtime materialization, public generic reflection, or complete token/cTypeId/layout cache.

## Completed Items

- Renamed the zrp TypeSpec row reserved slot to `typeLayoutId` without changing row size or zrp metadata version.
- Added `SZrMetadataRuntimeTypeSpecLayoutBindingView`.
- Added `ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView(runtime, typeSpecToken, outView)`.
- The view binds:
  - TypeSpec token and type token record.
  - Paired signature token record.
  - zrp `TYPE_SPECS` row.
  - Existing 11-S3K generic base-token binding view.
  - `typeLayoutId`, `cTypeId`, `signatureHash`, and registry-backed `SZrTypeLayout`.
- The reader rejects mismatched zrp row signature offset, length, or hash, and resolves layout only through `ZrCore_MetadataRuntime_ResolveTypeLayout()`.
- TypeDef/TypeSpec/FieldDef layout-binding row lookup and view reader code was split from `metadata_runtime.c` into `metadata_runtime_layout_binding.c` to keep the main metadata runtime file below the large-file threshold.

## RED

- Added focused test target `zr_vm_metadata_runtime_typespec_layout_test`.
- Initial WSL GCC build failed as expected because the implementation was missing:
  - `SZrZrpMetadataTypeSpecRow.typeLayoutId`.
  - `SZrMetadataRuntimeTypeSpecLayoutBindingView`.
  - `ZrCore_MetadataRuntime_ReadTypeSpecLayoutBindingView()`.
- After the first implementation, the focused target exposed a missing Unity `tearDown()` definition; that was fixed before GREEN validation.

## GREEN

- Positive coverage: a `GENERIC_INST(TYPE_DEF base, PRIMITIVE int64)` TypeSpec signature binds to the zrp TypeSpec row, base TypeDef token record, `typeLayoutId = 42`, and `codeRegistration->typeLayouts[42]`.
- Negative coverage: when the code-registration layout registry is missing but `prototypeFrameTypeLayouts[42]` contains a stale layout, TypeSpec layout binding returns false.

## Validation

- WSL GCC:
  - `zr_vm_metadata_runtime_typespec_layout_test` 2/0.
  - `zr_vm_zrp_metadata_format_test` 11/0.
  - `zr_vm_metadata_runtime_query_test` 22/0.
  - `zr_vm_metadata_runtime_type_layout_test` 10/0.
- WSL Clang:
  - Same four focused binaries passed with the same counts.
- Windows MSVC Debug:
  - Same four focused binaries passed with the same counts.

## Notes

- Existing compiler warnings were observed in generated dispatch labels, generated C source contracts, and MSVC const/unreachable-code paths. They did not fail the focused validation.
- The cross-session helper script failed with a PowerShell Count-property error during coordination, so recent plan/session files were checked manually.
