# AOT 10-S4Z22 / 11-S4BH FieldInfo nested VALUE_SLOT write

## Scope

- Added the first layout-indexed nested inline field write boundary for retained FieldInfo inline aggregates.
- Affected layers: runtime reflection, metadata runtime consumers, focused module tests, AOT plan documentation, and module documentation.
- The new boundary writes only a nested `VALUE_SLOT` child selected by `SZrTypeLayoutField` index.
- This does not implement multi-level recursive paths, primitive raw child writes, managed `FieldInfo.GetValue/SetValue`, cross-module provider loading, or metadata sweep completion.

## Baseline

- Before this slice, S4Z21 supported layout-indexed nested `VALUE_SLOT` child reads only.
- The RED test called `ZrCore_Reflection_WriteFieldInfoObjectNestedValue()` before the API existed.
- Windows MSVC Debug focused build failed with an undefined function warning and LNK2019 unresolved external for `ZrCore_Reflection_WriteFieldInfoObjectNestedValue`.

## Test Inventory

- Focused subsystem test: `tests/module/test_reflection_token_resolve.c`.
- New case: `test_reflection_writes_field_info_object_nested_value_slot_from_inline_struct()`.
- Positive boundary: valid FieldInfo object, retained `FIELD_SIG(TYPE_DEF)` inline aggregate, resolved field type layout, nested `VALUE_SLOT | GC_VALUE | OWNERSHIP_VALUE` child at layout index `0`, and full inline storage.
- Negative boundaries: short inline storage, out-of-range nested field index, and outer inline aggregate field with GC/ownership flags.
- Ownership boundary: destination starts with a unique-owned old string; the write copies a plain new string, drops the old owner strong ref to `0`, and normalizes destination ownership metadata to none.
- Regression companions: `zr_vm_metadata_runtime_query_test` and `zr_vm_metadata_runtime_typespec_layout_test`.

## Tooling Evidence

- RED command:
  `cmake --build build\codex-msvc-debug --config Debug --target zr_vm_reflection_token_resolve_test --parallel 8`
- RED observed output:
  undefined `ZrCore_Reflection_WriteFieldInfoObjectNestedValue` plus LNK2019 unresolved external.
- GREEN focused command:
  `cmake --build build\codex-msvc-debug --config Debug --target zr_vm_reflection_token_resolve_test --parallel 8; .\build\codex-msvc-debug\bin\Debug\zr_vm_reflection_token_resolve_test.exe`
- Build matrix:
  WSL GCC, WSL Clang, and Windows MSVC Debug all built `zr_vm_reflection_token_resolve_test`, `zr_vm_metadata_runtime_query_test`, and `zr_vm_metadata_runtime_typespec_layout_test`.
- Direct test matrix:
  WSL GCC, WSL Clang, and Windows MSVC Debug all ran the three focused binaries directly.
- CTest matrix:
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` passed on WSL GCC, WSL Clang, and Windows MSVC Debug.
- Existing warnings observed:
  `reflection.c` unused `callerName`, `runtime_decorator.c` unused helpers, `execution_dispatch.c` GNU label/indirect goto warnings under WSL, and existing MSVC warnings in `execution_dispatch.c` / `object_super_array_internal.h`.

## Results

- Windows focused GREEN: `reflection_token_resolve` 27/0.
- WSL GCC direct: `reflection_token_resolve` 27/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- WSL Clang direct: `reflection_token_resolve` 27/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- Windows MSVC Debug direct: `reflection_token_resolve` 27/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- Focused CTest: 3/3 on WSL GCC, WSL Clang, and Windows MSVC Debug.
- One WSL direct-run attempt used a bad shell loop and tried to execute the build `bin/` directory; it was discarded and replaced with explicit binary paths before acceptance.

## Acceptance Decision

- Accepted for 10-S4Z22 / 11-S4BH / 12-S5 support.
- The accepted behavior is the minimum nested `VALUE_SLOT` child write contract for retained FieldInfo inline aggregates.
- Remaining risks: no multi-level path API, no primitive raw child marshaling, no managed `FieldInfo.GetValue/SetValue`, no cross-module provider surface, no `@dynamically_accessed` dataflow, no DESCRIPTION promotion, and no full metadata sweep.
- Production modularization note: `zr_vm_core/src/zr_vm_core/reflection_field_value.c` is now about 915 lines. This slice kept it whole because the file still owns one FieldInfo value-boundary responsibility; the smallest follow-up production split is extracting nested field layout read/write helpers.
- Large test-file note: `tests/module/test_reflection_token_resolve.c` is oversized. This slice stayed in the existing FieldInfo inline-storage fixture to avoid unrelated test-target churn; the smallest follow-up split is extracting FieldInfo inline-storage fixtures into a focused test target.
