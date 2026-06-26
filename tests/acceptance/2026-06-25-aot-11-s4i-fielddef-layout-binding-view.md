# AOT 11-S4I FieldDef layout binding view

## Scope

- Slice: 11-S4I, supporting 10-S4 field offset reflection data path.
- Goal: expose a read-only FieldDef token -> zrp FieldDef row -> owner/field layout binding view through `SZrMetadataRuntime`.
- Non-goals: no TypeSpec/generic layout materialization, no ownership offset table emission, no runtime layout construction, no complete token/cTypeId/layout cache, and no public reflection `FieldInfo` entity materialization.

## Baseline

Before this slice, 11-S4 exposed TypeDef layout binding and registry-backed layout resolvers, and reflection/GC/generic consumers could use the code-registration layout registry. There was still no metadata runtime view that started from a FieldDef token and returned the FieldDef row byte offset, field type layout id, owner TypeDef binding, and registry-backed owner/field layouts.

## Test Inventory

- `tests/module/test_metadata_runtime_query.c`
  - `test_metadata_runtime_reads_fielddef_layout_binding_view`
  - `test_metadata_runtime_fielddef_layout_binding_does_not_fallback_to_prototype_cache`
- Related regression coverage:
  - `tests/module/test_metadata_runtime_type_layout.c`

## RED

The first RED was the focused metadata runtime query target after adding the FieldDef binding tests. The build failed because `SZrMetadataRuntimeFieldDefLayoutBindingView` and `ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView()` did not exist yet.

## GREEN

Implemented:

- `SZrMetadataRuntimeFieldDefLayoutBindingView`
- `ZrCore_MetadataRuntime_ReadFieldDefLayoutBindingView(runtime, fieldDefToken, outView)`
- internal FieldDef row lookup over the attached zrp `FIELD_DEFS` section
- owner TypeDef range validation through `firstFieldDefIndex/fieldDefCount`
- owner/field layout resolution through `ZrCore_MetadataRuntime_ResolveTypeLayout()`
- explicit no-prototype-fallback behavior when a stale `prototypeFrameTypeLayouts[typeLayoutId]` entry exists but the code-registration layout registry lacks the field layout

## Tooling Evidence

- WSL GCC: `zr_vm_metadata_runtime_query_test` = 22 tests, 0 failures, 0 ignored.
- WSL Clang: `zr_vm_metadata_runtime_query_test` = 22 tests, 0 failures, 0 ignored.
- Windows MSVC Debug: `zr_vm_metadata_runtime_query_test.exe` = 22 tests, 0 failures, 0 ignored.
- WSL GCC: `zr_vm_metadata_runtime_type_layout_test` = 10 tests, 0 failures, 0 ignored.
- WSL Clang: `zr_vm_metadata_runtime_type_layout_test` = 10 tests, 0 failures, 0 ignored.
- Windows MSVC Debug: `zr_vm_metadata_runtime_type_layout_test.exe` = 10 tests, 0 failures, 0 ignored.

## Acceptance Decision

Accepted for 11-S4I. The FieldDef binding view now exposes the token/row/offset/layout data path needed by later token-driven field reflection, while preserving the 11-S4 rule that layout pointers come from the code-registration registry and do not fall back to prototype layout cache.

Remaining work stays open in the plan: TypeSpec/generic layout materialization, ownership offset emission, runtime layout construction, complete token/cTypeId/layout cache, and public reflection field entity materialization.
