# AOT 10-S4Z23 / 11-S4BI FieldInfo nested VALUE_SLOT path read

## Scope

- Added the first multi-level nested inline field read boundary for retained FieldInfo inline aggregates.
- Affected layers: runtime reflection, metadata runtime consumers, focused module tests, AOT plan documentation, module documentation, and production file modularization.
- The new boundary reads only a leaf `VALUE_SLOT` child selected by a `nestedFieldIndices[] + count` path.
- This does not implement nested path writes, primitive raw child marshaling, managed `FieldInfo.GetValue/SetValue`, cross-module provider loading, dataflow analysis, DESCRIPTION promotion, or metadata sweep completion.

## Baseline

- Before this slice, S4Z21/S4Z22 supported only one layout-indexed nested `VALUE_SLOT` child read/write.
- The RED test called `ZrCore_Reflection_ReadFieldInfoObjectNestedPathValue()` before the API existed.
- Windows MSVC Debug focused build failed with an undefined function warning and LNK2019 unresolved external for `ZrCore_Reflection_ReadFieldInfoObjectNestedPathValue`.

## Test Inventory

- Focused subsystem test: `tests/module/test_reflection_token_resolve.c`.
- New case: `test_reflection_reads_field_info_object_nested_path_value_slot_from_inline_struct()`.
- Positive boundary: valid FieldInfo object, retained `FIELD_SIG(TYPE_DEF)` inline aggregate, outer child layout id, inner child layout id, path `{0u, 0u}`, and leaf `VALUE_SLOT | GC_VALUE | OWNERSHIP_VALUE` copied to a caller `SZrTypeValue`.
- Negative boundaries: zero-length path, final child out-of-range, missing intermediate registered layout, and intermediate child with GC/ownership flags.
- Regression companions: `zr_vm_metadata_runtime_query_test` and `zr_vm_metadata_runtime_typespec_layout_test`.
- Modularization boundary: recursive nested inline layout traversal was split into `reflection_field_value_nested.{h,c}` after `reflection_field_value.c` crossed 1100 lines.

## Tooling Evidence

- RED command:
  `cmake --build build\codex-msvc-debug --config Debug --target zr_vm_reflection_token_resolve_test --parallel 8`
- RED observed output:
  undefined `ZrCore_Reflection_ReadFieldInfoObjectNestedPathValue` plus LNK2019 unresolved external.
- Focused build matrix:
  WSL GCC, WSL Clang, and Windows MSVC Debug all built `zr_vm_reflection_token_resolve_test`, `zr_vm_metadata_runtime_query_test`, and `zr_vm_metadata_runtime_typespec_layout_test` after implementation and helper split.
- Direct test matrix:
  WSL GCC, WSL Clang, and Windows MSVC Debug all ran the three focused binaries directly.
- CTest matrix:
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` passed on WSL GCC, WSL Clang, and Windows MSVC Debug.
- Existing warnings observed:
  Known warnings in `reflection.c`, `runtime_decorator.c`, `execution_dispatch.c`, and `object_super_array_internal.h` remain unrelated to this slice.

## Results

- Windows MSVC Debug direct: `reflection_token_resolve` 28/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- WSL GCC direct: `reflection_token_resolve` 28/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- WSL Clang direct: `reflection_token_resolve` 28/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- Focused CTest: 3/3 on Windows MSVC Debug, WSL GCC, and WSL Clang.
- Line-count outcome after modularization: `reflection_field_value.c` is about 935 lines, `reflection_field_value_nested.c` is about 124 lines, and `reflection_field_value_nested.h` is about 29 lines.

## Acceptance Decision

- Accepted for 10-S4Z23 / 11-S4BI / 12-S5 support.
- The accepted behavior is the minimum multi-level nested `VALUE_SLOT` path read contract for retained FieldInfo inline aggregates.
- Remaining risks: no nested path write, no primitive raw child marshaling, no managed `FieldInfo.GetValue/SetValue`, no cross-module provider surface, no `@dynamically_accessed` dataflow, no DESCRIPTION promotion, and no full metadata sweep.
- Large test-file note: `tests/module/test_reflection_token_resolve.c` remains oversized. This slice stayed in the existing FieldInfo inline-storage fixture to avoid unrelated test-target churn; the smallest follow-up split is extracting FieldInfo inline-storage fixtures into a focused test target.
