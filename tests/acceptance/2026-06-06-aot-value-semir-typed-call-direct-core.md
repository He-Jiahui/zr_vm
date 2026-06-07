# AOT Value SemIR Typed Call Direct Core

## Scope

- Generated AOT C `CALL_TYPED` value-SemIR executable lowering now uses direct core call setup/post-call instead of the old static direct-call helper pair.
- Affected layers: `backend_aot_c_value_semir.c`, the value-SemIR source contract, and the generated call shared-library smoke.
- Non-POD typed returns, hidden-return ownership, user-constructor executable lowering, generated meta-call execution, and LLVM parity remain separate future slices.

## Baseline

- RED: `zr_vm_aot_c_source_contracts_test` first failed because `test_aot_c_source_lowers_value_semir_with_frame_layout` expected `SZrFunction *zr_aot_metadata_function;` while `backend_aot_try_write_c_value_call_typed_exec()` still emitted `ZrAotGeneratedDirectCall zr_aot_direct_call` plus `ZrLibrary_AotRuntime_PrepareStaticDirectCall` / `ZrLibrary_AotRuntime_FinishDirectCall`.
- The repository baseline remains dirty with unrelated work; this slice only claims the focused AOT C value-SemIR typed-call/test/docs changes.

## Test Inventory

- Focused source contract: `tests/parser/test_aot_c_source_contracts.c`.
- Generated-C smoke: `tests/parser/test_aot_c_call_shared_library_smoke.c` still executes the source-level int call fixture and now also compiles a generated POD struct return fixture for the value typed-call C surface.
- Production scan: `backend_aot_c_value_semir.c` contains no `ZrLibrary_AotRuntime_PrepareStaticDirectCall`, `ZrLibrary_AotRuntime_FinishDirectCall`, or `ZrAotGeneratedDirectCall zr_aot_direct_call` emission.

## Tooling Evidence

- RED command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test"`
- GCC command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_call_shared_library_smoke_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_call_shared_library_smoke_test"`
- Clang command:
  `wsl bash -lc "cd /mnt/e/Git/zr_vm && CC=clang cmake -S . -B build-wsl-clang -DCMAKE_BUILD_TYPE=Debug -DBUILD_SHARED_LIB=ON && cmake --build build-wsl-clang --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_call_shared_library_smoke_test -j2 && ./build-wsl-clang/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-clang/bin/zr_vm_aot_c_call_shared_library_smoke_test"`
- MSVC command:
  `Import-VsDevCmdEnvironment.ps1; cmake --build build-msvc --target zr_vm_aot_c_source_contracts_test --config Debug; .\build-msvc\bin\Debug\zr_vm_aot_c_source_contracts_test.exe; cmake --build build-msvc --target zr_vm_aot_c_call_shared_library_smoke_test --config Debug; .\build-msvc\bin\Debug\zr_vm_aot_c_call_shared_library_smoke_test.exe`

## Results

- RED result: `test_aot_c_source_lowers_value_semir_with_frame_layout` failed on missing `SZrFunction *zr_aot_metadata_function;`.
- GCC `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- GCC `zr_vm_aot_c_call_shared_library_smoke_test`: 2 tests, 0 failures.
- Clang `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- Clang `zr_vm_aot_c_call_shared_library_smoke_test`: 2 tests, 0 failures.
- MSVC `zr_vm_aot_c_source_contracts_test`: 17 tests, 0 failures.
- MSVC `zr_vm_aot_c_call_shared_library_smoke_test`: target built; 2 Unix-only checks ignored.
- Production scan of `backend_aot_c_value_semir.c` found no old typed-call helper pair emission.

## Acceptance Decision

- Accepted for the AOT C value-SemIR typed-call helper-removal slice.
- `CALL_TYPED` now resolves callable metadata from the callee slot, prepares the resolved VM frame with `ZrCore_Function_PreCallPreparedResolvedVmFunction`, invokes the known generated thunk, posts with `ZrCore_Function_PostCall`, refreshes the caller frame, and relies on core inline-return movement through `ZrCore_Function_TryCopyInlineFrameReturnValue`.
- The generated POD struct return smoke is compile-only because source-level user constructor executable lowering remains a separate AOT boundary; the fixture uses zero-arg struct initialization plus field stores to keep this slice focused on typed call/return generated C.
- Large-file note: `backend_aot_c_value_semir.c` is above the 1100-line warning threshold. The clean follow-up boundary is extracting typed call/return emitters into a dedicated value-SemIR call-return module with a narrow private helper header.
- Remaining risks: non-POD typed call/return copy/drop policy, hidden return ownership, user constructor executable lowering, generated meta-call execution, and LLVM parity remain future work.
