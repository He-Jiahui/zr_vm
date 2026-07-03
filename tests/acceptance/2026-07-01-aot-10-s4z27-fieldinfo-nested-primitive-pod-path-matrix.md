# AOT 10-S4Z27 / 11-S4BM FieldInfo nested primitive POD path matrix

## Scope

- Added representative nested primitive path matrix coverage for retained FieldInfo inline aggregates.
- Affected layers: focused module tests, AOT plan documentation, and module documentation.
- This is a coverage-only slice: no production code and no public API changed.
- This does not implement the full primitive width/signature-derived nested matrix, managed `FieldInfo.GetValue/SetValue`, cross-module provider loading, dataflow analysis, DESCRIPTION promotion, or metadata sweep completion.

## Baseline

- S4Z25 added the first INT32 nested primitive POD raw child path read/write boundary.
- S4Z26 added the nested primitive leaf layout identity guard.
- This slice checks whether the shared primitive POD guard already handles representative non-INT32 scalar families on the same nested path.

## Test Inventory

- Focused subsystem test: `tests/module/test_reflection_token_resolve.c`.
- Extended case: `test_reflection_reads_and_writes_field_info_object_nested_path_primitive_pod_from_inline_struct()`.
- Positive matrix: bool, uint32, and double raw child path read/write through the same two-level `{0u, 0u}` FieldInfo object path.
- Existing INT32 positive and negative coverage remains in the same fixture.
- Regression companions: `zr_vm_metadata_runtime_query_test` and `zr_vm_metadata_runtime_typespec_layout_test`.

## Tooling Evidence

- Coverage GREEN command:
  `.\build\codex-msvc-debug\bin\Debug\zr_vm_reflection_token_resolve_test.exe`
- Coverage GREEN observed output:
  `30 Tests 0 Failures 0 Ignored OK`.
- Focused build matrix:
  WSL GCC, WSL Clang, and Windows MSVC Debug all built `zr_vm_reflection_token_resolve_test`, `zr_vm_metadata_runtime_query_test`, and `zr_vm_metadata_runtime_typespec_layout_test`.
- CTest matrix:
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` passed on WSL GCC, WSL Clang, and Windows MSVC Debug.

## Results

- Windows MSVC Debug focused direct: `reflection_token_resolve` 30/0.
- Focused CTest: 3/3 on Windows MSVC Debug, WSL GCC, and WSL Clang.
- The existing nested primitive path implementation reused the shared primitive POD guard for bool, uint32, and double without production changes.

## Acceptance Decision

- Accepted for 10-S4Z27 / 11-S4BM / 12-S5 support.
- The accepted behavior is representative nested primitive path coverage for bool, uint32, and double raw children.
- Remaining risks: no full nested primitive width matrix, no signature-derived primitive field binding matrix, no managed `FieldInfo.GetValue/SetValue`, no cross-module provider surface, no `@dynamically_accessed` dataflow, no DESCRIPTION promotion, and no full metadata sweep.
