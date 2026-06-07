# AOT Closure Value Direct Access

## Scope

- Generated AOT C `GET_CLOSURE`, `SET_CLOSURE`, `GETUPVAL`, and `SETUPVAL` lowering.
- Affected layers: AOT C backend value lowering, source contracts, generated-C compile smoke.

## Baseline

- Before this change, generated C emitted `ZrLibrary_AotRuntime_GetClosureValue(state, &frame, ...)` and `ZrLibrary_AotRuntime_SetClosureValue(state, &frame, ...)`.
- The RED contract failed on the missing `zr_aot_value_exec_get_closure_value` marker.
- The repository already had unrelated dirty state; this record claims only the focused AOT validation below.

## Test Inventory

- `test_aot_c_source_lowers_closure_value_access_to_direct_core_copy` in `tests/parser/test_aot_c_constant_contracts.c`.
- `test_aot_c_generated_shared_library_compiles_closure_value_direct_access` in `tests/parser/test_aot_c_global_shared_library_smoke.c`.
- Boundary coverage: native closure capture access, VM closure capture access, null/invalid/out-of-range failure paths through `ZR_AOT_C_FAIL()`, setter barrier owner handling, and all four opcode routes.
- Negative coverage: `ZrLibrary_AotRuntime_GetClosureValue` and `ZrLibrary_AotRuntime_SetClosureValue` are absent from the AOT C value/function-body lowering path.

## Tooling Evidence

- RED:
  - `wsl sh -lc 'cmake --build /mnt/e/Git/zr_vm/build-wsl-gcc --target zr_vm_aot_c_constant_contracts_test -j 4 && /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_aot_c_constant_contracts_test'`
  - Result: `test_aot_c_source_lowers_closure_value_access_to_direct_core_copy` failed on missing `zr_aot_value_exec_get_closure_value`.
- GCC:
  - `wsl sh -lc 'cmake --build /mnt/e/Git/zr_vm/build-wsl-gcc --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_constant_contracts_test zr_vm_aot_c_global_contracts_test zr_vm_aot_c_global_shared_library_smoke_test zr_vm_aot_c_shared_library_smoke_test -j 4 && /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_aot_c_constant_contracts_test && /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_aot_c_global_contracts_test && /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_aot_c_global_shared_library_smoke_test && /mnt/e/Git/zr_vm/build-wsl-gcc/bin/zr_vm_aot_c_shared_library_smoke_test'`
- Clang:
  - `wsl sh -lc 'cmake --build /mnt/e/Git/zr_vm/build-wsl-clang --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_constant_contracts_test zr_vm_aot_c_global_contracts_test zr_vm_aot_c_global_shared_library_smoke_test zr_vm_aot_c_shared_library_smoke_test -j 4 && /mnt/e/Git/zr_vm/build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test && /mnt/e/Git/zr_vm/build-wsl-clang/bin/zr_vm_aot_c_constant_contracts_test && /mnt/e/Git/zr_vm/build-wsl-clang/bin/zr_vm_aot_c_global_contracts_test && /mnt/e/Git/zr_vm/build-wsl-clang/bin/zr_vm_aot_c_global_shared_library_smoke_test && /mnt/e/Git/zr_vm/build-wsl-clang/bin/zr_vm_aot_c_shared_library_smoke_test'`
- MSVC:
  - `. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; cmake --build build\codex-msvc-aot-constant-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_constant_contracts_test zr_vm_aot_c_global_contracts_test zr_vm_aot_c_global_shared_library_smoke_test --config Debug --parallel 4`
  - Executed `zr_vm_aot_c_source_contracts_test.exe`, `zr_vm_aot_c_constant_contracts_test.exe`, `zr_vm_aot_c_global_contracts_test.exe`, and `zr_vm_aot_c_global_shared_library_smoke_test.exe`.
- Source scan:
  - `Select-String -Path 'zr_vm_aot\zr_vm_parser\src\zr_vm_parser\backend_aot\backend_aot_c_function_body.c','zr_vm_aot\zr_vm_parser\src\zr_vm_parser\backend_aot\backend_aot_c_lowering_values.c' -Pattern 'ZrLibrary_AotRuntime_GetClosureValue|ZrLibrary_AotRuntime_SetClosureValue|zr_aot_value_exec_get_closure_value|zr_aot_value_exec_set_closure_value'`

## Results

- GCC: source contracts 17 tests, constant contracts 4 tests, global contracts 3 tests, global shared-library smoke 4 tests, shared-library smoke 6 tests; all 0 failures.
- Clang: source contracts 17 tests, constant contracts 4 tests, global contracts 3 tests, global shared-library smoke 4 tests, shared-library smoke 6 tests; all 0 failures.
- MSVC: source contracts 17 tests, constant contracts 4 tests, global contracts 3 tests, global shared-library smoke 4 tests; all 0 failures, with 4 Unix-only shared-library smoke tests ignored.
- Source scan found only the new `zr_aot_value_exec_get_closure_value` / `zr_aot_value_exec_set_closure_value` markers in `backend_aot_c_lowering_values.c`; the old closure-value helper calls are absent from the checked AOT C function-body/value-lowering sources.
- The generated-C closure smoke is compile-only because the hand-built opcode fixture has no valid current closure frame to execute.

## Acceptance Decision

Accepted for generated AOT C closure value access lowering. Remaining work includes LLVM parity, executing a real captured-closure generated native path, and captured-closure materialization itself.
