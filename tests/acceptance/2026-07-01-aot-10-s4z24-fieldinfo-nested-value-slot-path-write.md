# AOT 10-S4Z24 / 11-S4BJ FieldInfo nested VALUE_SLOT path write

## Scope

- Added the first multi-level nested inline field write boundary for retained FieldInfo inline aggregates.
- Affected layers: runtime reflection, metadata runtime consumers, focused module tests, AOT plan documentation, and module documentation.
- The new boundary writes only a leaf `VALUE_SLOT` child selected by a `nestedFieldIndices[] + count` path.
- This does not implement primitive raw child marshaling, managed `FieldInfo.GetValue/SetValue`, cross-module provider loading, dataflow analysis, DESCRIPTION promotion, or metadata sweep completion.

## Baseline

- Before this slice, S4Z23 supported multi-level nested `VALUE_SLOT` path reads and S4Z22 supported only one layout-indexed nested `VALUE_SLOT` child write.
- The RED test called `ZrCore_Reflection_WriteFieldInfoObjectNestedPathValue()` before the API existed.
- Windows MSVC Debug focused build failed with an undefined function warning and LNK2019 unresolved external for `ZrCore_Reflection_WriteFieldInfoObjectNestedPathValue`.

## Test Inventory

- Focused subsystem test: `tests/module/test_reflection_token_resolve.c`.
- New case: `test_reflection_writes_field_info_object_nested_path_value_slot_from_inline_struct()`.
- Positive boundary: valid FieldInfo object, retained `FIELD_SIG(TYPE_DEF)` inline aggregate, outer child layout id, inner child layout id, path `{0u, 0u}`, and leaf `VALUE_SLOT | GC_VALUE | OWNERSHIP_VALUE` replaced with a caller `SZrTypeValue`.
- Ownership boundary: replacing a unique-owned old string drops the old owner strong ref from 1 to 0; the plain new string is copied without gaining ownership metadata.
- Negative boundaries: short inline storage, zero-length path, final child out-of-range, missing intermediate registered layout, and intermediate child with GC/ownership flags.
- Regression companions: `zr_vm_metadata_runtime_query_test` and `zr_vm_metadata_runtime_typespec_layout_test`.

## Tooling Evidence

- RED command:
  `cmake --build build\codex-msvc-debug --config Debug --target zr_vm_reflection_token_resolve_test --parallel 8`
- RED observed output:
  undefined `ZrCore_Reflection_WriteFieldInfoObjectNestedPathValue` plus LNK2019 unresolved external.
- Focused build matrix:
  WSL GCC, WSL Clang, and Windows MSVC Debug all built `zr_vm_reflection_token_resolve_test`, `zr_vm_metadata_runtime_query_test`, and `zr_vm_metadata_runtime_typespec_layout_test` after implementation.
- Direct test matrix:
  WSL GCC, WSL Clang, and Windows MSVC Debug all ran the three focused binaries directly.
- CTest matrix:
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` passed on WSL GCC, WSL Clang, and Windows MSVC Debug.
- Existing warnings observed:
  Known warnings in `reflection.c`, `runtime_decorator.c`, `execution_dispatch.c`, and `object_super_array_internal.h` remain unrelated to this slice.

## Results

- Windows MSVC Debug direct: `reflection_token_resolve` 29/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- WSL GCC direct: `reflection_token_resolve` 29/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- WSL Clang direct: `reflection_token_resolve` 29/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- Focused CTest: 3/3 on Windows MSVC Debug, WSL GCC, and WSL Clang.
- Line-count check after the slice: `reflection_field_value.c` is about 999 lines by the project skill command, `reflection_field_value_nested.c` is about 171 lines, and `reflection_field_value_nested.h` is about 37 lines.

## Acceptance Decision

- Accepted for 10-S4Z24 / 11-S4BJ / 12-S5 support.
- The accepted behavior is the minimum multi-level nested `VALUE_SLOT` path write contract for retained FieldInfo inline aggregates.
- Remaining risks: no primitive raw child marshaling, no managed `FieldInfo.GetValue/SetValue`, no cross-module provider surface, no `@dynamically_accessed` dataflow, no DESCRIPTION promotion, and no full metadata sweep.
- Large-file note: `reflection_field_value.c` is at the next split threshold and `tests/module/test_reflection_token_resolve.c` remains oversized. The smallest follow-up split is extracting FieldInfo inline-storage adapters and fixtures into narrower files or a focused test target.
