# AOT 10-S4Z26 / 11-S4BL FieldInfo nested primitive POD leaf layout guard

## Scope

- Added the nested primitive raw child leaf layout identity guard for retained FieldInfo inline aggregates.
- Affected layers: runtime reflection nested primitive traversal, focused module tests, AOT plan documentation, and module documentation.
- This does not add a public API or change top-level primitive FieldInfo behavior.
- This does not implement the full primitive width/signature-derived nested matrix, managed `FieldInfo.GetValue/SetValue`, cross-module provider loading, dataflow analysis, DESCRIPTION promotion, or metadata sweep completion.

## Baseline

- S4Z25 added the first representative multi-level nested primitive POD raw child path read/write boundary.
- Before this guard, a nested primitive leaf whose `SZrTypeLayoutField.typeLayoutIndex` still pointed at a registered child layout could be treated as raw primitive storage.
- The RED test set the leaf `typeLayoutIndex` to `42u` and expected both read and write to reject it.
- Windows MSVC Debug focused run failed with `Expected FALSE Was TRUE`.

## Test Inventory

- Focused subsystem test: `tests/module/test_reflection_token_resolve.c`.
- Extended case: `test_reflection_reads_and_writes_field_info_object_nested_path_primitive_pod_from_inline_struct()`.
- Negative boundary: leaf raw primitive child with `typeLayoutIndex = 42u` rejects both read and write.
- Byte preservation: rejected writes leave the raw INT32 bytes unchanged at `-12345`.
- Regression companions: `zr_vm_metadata_runtime_query_test` and `zr_vm_metadata_runtime_typespec_layout_test`.

## Tooling Evidence

- RED command:
  `.\build\codex-msvc-debug\bin\Debug\zr_vm_reflection_token_resolve_test.exe`
- RED observed output:
  `30 Tests 1 Failures 0 Ignored FAIL`, failing with `Expected FALSE Was TRUE`.
- Focused build matrix:
  WSL GCC, WSL Clang, and Windows MSVC Debug all built `zr_vm_reflection_token_resolve_test`, `zr_vm_metadata_runtime_query_test`, and `zr_vm_metadata_runtime_typespec_layout_test` after implementation.
- CTest matrix:
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` passed on WSL GCC, WSL Clang, and Windows MSVC Debug.

## Results

- Windows MSVC Debug focused direct: `reflection_token_resolve` 30/0 after the guard.
- Focused CTest: 3/3 on Windows MSVC Debug, WSL GCC, and WSL Clang.
- Implementation guard: `reflection_field_value_nested.c` now requires leaf `typeLayoutIndex == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE` before delegating nested primitive read/write to the shared primitive POD guard.

## Acceptance Decision

- Accepted for 10-S4Z26 / 11-S4BL / 12-S5 support.
- The accepted behavior is the minimum nested primitive raw child leaf layout identity guard for retained FieldInfo inline aggregates.
- Remaining risks: no full nested primitive width matrix, no signature-derived primitive field binding matrix, no managed `FieldInfo.GetValue/SetValue`, no cross-module provider surface, no `@dynamically_accessed` dataflow, no DESCRIPTION promotion, and no full metadata sweep.
