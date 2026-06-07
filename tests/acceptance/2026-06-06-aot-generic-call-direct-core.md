# AOT Generic Call Direct Core

## Scope

- Generated AOT C generic direct function calls now use the same stack-anchor/core-call template as dynamic calls.
- Affected layers: AOT C call lowering and focused source-contract tests.
- Static resolved direct calls were handled by the follow-up `2026-06-06-aot-static-call-direct-core.md` slice. Meta calls still use their dedicated preparation boundary.

## Baseline

- RED: `zr_vm_aot_c_call_contracts_test` first failed because `backend_aot_write_c_direct_function_call()` still emitted the `ZrLibrary_AotRuntime_PrepareDirectCall` helper and did not contain the `zr_aot_direct_function_call` core-call marker.
- The repository baseline remains dirty with unrelated work; this slice only claims the focused AOT C call lowering/test changes.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_call_contracts.c`.
- Regression smoke: `tests/parser/test_aot_c_call_shared_library_smoke.c` keeps recompiling and executing the generated dynamic-call shared-library path, which now shares the same core-call template.
- Source scan: `backend_aot_c_lowering_calls.c` contains `zr_aot_direct_function_call`, `zr_aot_direct_dynamic_function_call`, and `ZrCore_Function_CallAndRestoreAnchor`, with no `ZrLibrary_AotRuntime_PrepareDirectCall` emission.

## Tooling Evidence

- GCC command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_call_shared_library_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_call_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_call_shared_library_smoke_test"`
- Clang command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && CC=clang cmake -S . -B build-wsl-clang -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIB=ON && cmake --build build-wsl-clang --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_call_shared_library_smoke_test -j2 && ./build-wsl-clang/bin/zr_vm_aot_c_call_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_call_shared_library_smoke_test"`
- MSVC command:
  `Import-VsDevCmdEnvironment.ps1; cmake --build build-msvc --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_call_shared_library_smoke_test --config Debug; .\build-msvc\bin\Debug\zr_vm_aot_c_call_contracts_test.exe; .\build-msvc\bin\Debug\zr_vm_aot_c_call_shared_library_smoke_test.exe`

## Results

- GCC `zr_vm_aot_c_call_contracts_test`: 2 tests, 0 failures.
- GCC `zr_vm_aot_c_call_shared_library_smoke_test`: 1 test, 0 failures.
- Clang `zr_vm_aot_c_call_contracts_test`: 2 tests, 0 failures.
- Clang `zr_vm_aot_c_call_shared_library_smoke_test`: 1 test, 0 failures.
- MSVC `zr_vm_aot_c_call_contracts_test`: 2 tests, 0 failures.
- MSVC `zr_vm_aot_c_call_shared_library_smoke_test`: target built; 1 Unix-only runtime test ignored.
- GCC/Clang generated call-smoke C still contains `zr_aot_direct_dynamic_function_call` and `ZrCore_Function_CallAndRestoreAnchor`, with no `ZrLibrary_AotRuntime_Call(state, &frame` or `ZrLibrary_AotRuntime_PrepareDirectCall`.

## Acceptance Decision

- Accepted for the AOT C generic direct-call helper-removal slice.
- Generic direct calls now emit generated stack-slot validation, call/destination anchors, `ZrCore_Function_CallAndRestoreAnchor`, direct result copy, and active-frame refresh instead of preparing a `ZrAotGeneratedDirectCall` through the AOT runtime helper.
- Remaining risks: source-level coverage for a non-static non-dynamic generic callsite is still source-contract-only; meta-call execution and LLVM parity remain future work.
