# AOT 10-S4Z25 / 11-S4BK FieldInfo nested primitive POD path read/write

## Scope

- Added the first representative multi-level nested inline primitive raw child read/write boundary for retained FieldInfo inline aggregates.
- Affected layers: runtime reflection, metadata runtime consumers, focused module tests, AOT plan documentation, and module documentation.
- The new boundary reads and writes only a primitive POD raw child selected by a `nestedFieldIndices[] + count` path.
- This does not implement the full primitive width/signature-derived nested matrix, managed `FieldInfo.GetValue/SetValue`, cross-module provider loading, dataflow analysis, DESCRIPTION promotion, or metadata sweep completion.

## Baseline

- Before this slice, S4Z23/S4Z24 supported multi-level nested `VALUE_SLOT` path read/write, and S4Z6..S4Z12 supported top-level primitive POD raw inline read/write guards.
- The RED test called `ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue()` and `ZrCore_Reflection_WriteFieldInfoObjectNestedPathPrimitiveValue()` before the APIs existed.
- Windows MSVC Debug focused build failed with C4013 undefined-function warnings and LNK2019 unresolved externals for both missing APIs.

## Test Inventory

- Focused subsystem test: `tests/module/test_reflection_token_resolve.c`.
- New case: `test_reflection_reads_and_writes_field_info_object_nested_path_primitive_pod_from_inline_struct()`.
- Positive boundary: valid FieldInfo object, retained `FIELD_SIG(TYPE_DEF)` inline aggregate, outer child layout id, inner child raw INT32 layout, path `{0u, 0u}`, read of `-12345`, write of `2048`, and readback of `2048`.
- Negative boundaries: short inline storage, zero-length path, final child out-of-range, missing intermediate registered layout, intermediate VALUE_SLOT/GC/ownership flags, leaf VALUE_SLOT flag rejection, primitive byte-size mismatch, bool write type mismatch, and rejected-write byte preservation.
- Regression companions: `zr_vm_metadata_runtime_query_test` and `zr_vm_metadata_runtime_typespec_layout_test`.

## Tooling Evidence

- RED command:
  `cmake --build build\codex-msvc-debug --config Debug --target zr_vm_reflection_token_resolve_test --parallel 8`
- RED observed output:
  undefined `ZrCore_Reflection_ReadFieldInfoObjectNestedPathPrimitiveValue` and `ZrCore_Reflection_WriteFieldInfoObjectNestedPathPrimitiveValue`, plus LNK2019 unresolved externals.
- Focused build matrix:
  WSL GCC, WSL Clang, and Windows MSVC Debug all built `zr_vm_reflection_token_resolve_test`, `zr_vm_metadata_runtime_query_test`, and `zr_vm_metadata_runtime_typespec_layout_test` after implementation.
- Direct test matrix:
  WSL GCC, WSL Clang, and Windows MSVC Debug all ran the three focused binaries directly.
- CTest matrix:
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` passed on WSL GCC, WSL Clang, and Windows MSVC Debug.
- Existing warnings observed:
  Known warnings in `reflection.c`, `runtime_decorator.c`, `execution_dispatch.c`, and `object_super_array_internal.h` remain unrelated to this slice.

## Results

- Windows MSVC Debug direct: `reflection_token_resolve` 30/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- WSL GCC direct: `reflection_token_resolve` 30/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- WSL Clang direct: `reflection_token_resolve` 30/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- Focused CTest: 3/3 on Windows MSVC Debug, WSL GCC, and WSL Clang.
- Implementation split: shared primitive POD raw load/store guards now live in `reflection_field_value_primitive.{h,c}` so top-level and nested primitive paths reuse the same validation rules.

## Acceptance Decision

- Accepted for 10-S4Z25 / 11-S4BK / 12-S5 support.
- The accepted behavior is the minimum representative INT32 multi-level nested primitive POD raw child path read/write contract for retained FieldInfo inline aggregates.
- Remaining risks: no full nested primitive width matrix, no signature-derived primitive field binding matrix, no managed `FieldInfo.GetValue/SetValue`, no cross-module provider surface, no `@dynamically_accessed` dataflow, no DESCRIPTION promotion, and no full metadata sweep.
