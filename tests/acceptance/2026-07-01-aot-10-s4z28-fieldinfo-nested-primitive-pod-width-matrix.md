# AOT 10-S4Z28 / 11-S4BN FieldInfo nested primitive POD width matrix

## Scope

- Added storage-width nested primitive path matrix coverage for retained FieldInfo inline aggregates.
- Affected layers: focused module tests, AOT plan documentation, and module documentation.
- This is a coverage-only slice: no production code and no public API changed.
- This does not implement signature-derived nested binding, managed `FieldInfo.GetValue/SetValue`, cross-module provider loading, dataflow analysis, DESCRIPTION promotion, or metadata sweep completion.

## Baseline

- S4Z25 added the first INT32 nested primitive POD raw child path read/write boundary.
- S4Z26 added the nested primitive leaf layout identity guard.
- S4Z27 extended the same path to representative bool, uint32, and double raw children.
- This slice checks whether the shared primitive POD guard already handles storage-width scalar families on the same nested path.

## Test Inventory

- Focused subsystem test: `tests/module/test_reflection_token_resolve.c`.
- Extended case: `test_reflection_reads_and_writes_field_info_object_nested_path_primitive_pod_from_inline_struct()`.
- Positive matrix: int8, int16, int64, uint8, uint16, uint64, and float32 raw child path read/write through the same two-level `{0u, 0u}` FieldInfo object path.
- Existing INT32, bool, uint32, double positive coverage and S4Z25/S4Z26 negative coverage remain in the same fixture.
- Regression companions: `zr_vm_metadata_runtime_query_test` and `zr_vm_metadata_runtime_typespec_layout_test`.

## Tooling Evidence

- Coverage GREEN command:
  `.\build\codex-msvc-debug\bin\Debug\zr_vm_reflection_token_resolve_test.exe`
- Coverage GREEN observed output:
  `30 Tests 0 Failures 0 Ignored OK`.
- Focused build matrix:
  WSL GCC, WSL Clang, and Windows MSVC Debug all built `zr_vm_reflection_token_resolve_test`, `zr_vm_metadata_runtime_query_test`, and `zr_vm_metadata_runtime_typespec_layout_test`.
- Build commands:
  `cmake --build build\codex-msvc-debug --config Debug --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test --parallel 8`
  `wsl bash -lc "cmake --build /mnt/e/Git/zr_vm/build/codex-wsl-gcc-reflection-debug --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test --parallel 8"`
  `wsl bash -lc "cmake --build /mnt/e/Git/zr_vm/build/codex-wsl-clang-debug --target zr_vm_reflection_token_resolve_test zr_vm_metadata_runtime_query_test zr_vm_metadata_runtime_typespec_layout_test --parallel 8"`
- CTest matrix:
  `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` passed on WSL GCC, WSL Clang, and Windows MSVC Debug.
- CTest commands:
  `ctest --test-dir build\codex-msvc-debug -C Debug -R "reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout" --output-on-failure`
  `wsl bash -lc "ctest --test-dir /mnt/e/Git/zr_vm/build/codex-wsl-gcc-reflection-debug -R 'reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout' --output-on-failure"`
  `wsl bash -lc "ctest --test-dir /mnt/e/Git/zr_vm/build/codex-wsl-clang-debug -R 'reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout' --output-on-failure"`

## Results

- Windows MSVC Debug focused direct: `reflection_token_resolve` 30/0.
- Focused CTest: 3/3 on Windows MSVC Debug, WSL GCC, and WSL Clang.
- The existing nested primitive path implementation reused the shared primitive POD guard for storage-width raw children without production changes.
- One post-doc validation command used the stale path `/mnt/e/Git/zr_vm/build/codex-wsl-clang-reflection-debug` and failed before running tests because the directory did not exist; the same focused CTest matrix was rerun successfully in `/mnt/e/Git/zr_vm/build/codex-wsl-clang-debug`.

## Acceptance Decision

- Accepted for 10-S4Z28 / 11-S4BN / 12-S5 support.
- The accepted behavior is storage-width nested primitive path coverage for int8, int16, int64, uint8, uint16, uint64, and float32 raw children.
- Remaining risks: no signature-derived primitive field binding matrix, no managed `FieldInfo.GetValue/SetValue`, no cross-module provider surface, no `@dynamically_accessed` dataflow, no DESCRIPTION promotion, and no full metadata sweep.
