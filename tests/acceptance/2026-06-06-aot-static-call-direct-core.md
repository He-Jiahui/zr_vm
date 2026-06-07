# AOT Static Call Direct Core

## Scope

- Generated AOT C static resolved direct calls now set up and tear down the call through core APIs instead of AOT runtime direct-call helpers.
- Affected layers: AOT C call lowering, call source-contract tests, and the generated call shared-library smoke.
- Meta calls, LLVM parity, and value-SemIR typed call/return hidden-return policy remain separate future slices.

## Baseline

- RED: `zr_vm_aot_c_call_contracts_test` first failed because `backend_aot_write_c_static_direct_function_call()` did not emit `zr_aot_direct_static_function_call` and still used `ZrLibrary_AotRuntime_PrepareStaticDirectCall` / `ZrLibrary_AotRuntime_FinishDirectCall`.
- The repository baseline remains dirty with unrelated work; this slice only claims the focused AOT C call lowering/test/docs changes.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_call_contracts.c`.
- Generated-C runtime smoke: `tests/parser/test_aot_c_call_shared_library_smoke.c` compiles and executes the generated Unix shared library for `apply(addFour, 3)`, which includes both static and dynamic direct-call blocks.
- Source/generated scans: `backend_aot_c_lowering_calls.c` and the GCC/Clang generated call-smoke C contain `zr_aot_direct_static_function_call`, `ZrCore_Function_PreCallPreparedResolvedVmFunction`, and `ZrCore_Function_PostCall`, with no old static direct-call helper emission.

## Tooling Evidence

- RED command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_call_contracts_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_call_contracts_test"`
- GCC command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_call_shared_library_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_call_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_call_shared_library_smoke_test"`
- Clang command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && CC=clang cmake -S . -B build-wsl-clang -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIB=ON && cmake --build build-wsl-clang --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_call_shared_library_smoke_test -j2 && ./build-wsl-clang/bin/zr_vm_aot_c_call_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_call_shared_library_smoke_test"`
- MSVC command:
  `Import-VsDevCmdEnvironment.ps1; cmake --build build-msvc --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_call_shared_library_smoke_test --config Debug; .\build-msvc\bin\Debug\zr_vm_aot_c_call_contracts_test.exe; .\build-msvc\bin\Debug\zr_vm_aot_c_call_shared_library_smoke_test.exe`

## Results

- RED result: `test_aot_c_source_lowers_static_direct_calls_to_direct_core_calls` failed on missing `zr_aot_direct_static_function_call`.
- GCC `zr_vm_aot_c_call_contracts_test`: 3 tests, 0 failures.
- GCC `zr_vm_aot_c_call_shared_library_smoke_test`: 1 test, 0 failures.
- Clang `zr_vm_aot_c_call_contracts_test`: 3 tests, 0 failures.
- Clang `zr_vm_aot_c_call_shared_library_smoke_test`: 1 test, 0 failures.
- MSVC `zr_vm_aot_c_call_contracts_test`: 3 tests, 0 failures.
- MSVC `zr_vm_aot_c_call_shared_library_smoke_test`: target built; 1 Unix-only runtime test ignored.
- Source/generated scans found no `ZrLibrary_AotRuntime_PrepareStaticDirectCall`, `ZrLibrary_AotRuntime_FinishDirectCall`, `ZrLibrary_AotRuntime_PrepareDirectCall`, or `ZrLibrary_AotRuntime_Call(state, &frame` in the checked call lowering source or GCC/Clang generated call-smoke C.

## Acceptance Decision

- Accepted for the AOT C static resolved direct-call helper-removal slice.
- Static resolved direct calls now resolve callee metadata from the callable slot, prepare a resolved VM call with core, invoke the generated thunk directly, post-call with core, and refresh the caller frame without the static direct-call AOT runtime helper pair.
- Remaining risks: generated meta-call execution still uses its runtime boundary; LLVM keeps its current helper routes; value-SemIR typed call/return hidden-return and non-POD policies remain future work.
