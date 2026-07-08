# AOT 07-S5 Full-AOT No-Arg Typed Direct-Call Boundary

Timestamp: 2026-07-06 01:28:16 +08:00

## Scope

- Threaded `requireFullAot` into no-argument typed direct-call lowering.
- Full-AOT bool/i64/u64/f64 no-arg typed direct calls now emit direct scalar thunk assignment with
  `zr_aot_static_*_no_arg_direct_call_full_aot`.
- Non-full-AOT typed direct calls keep metadata guard and deopt fallback behavior.
- The static numeric call shared-library smoke now locks u64/f64 call results flowing through scalar stack copies
  without typed-destination materialization.

## Assertions

- Present: `/* zr_aot_static_u64_no_arg_direct_call_full_aot */`
- Present: `/* zr_aot_static_f64_no_arg_direct_call_full_aot */`
- Present: `zr_aot_u5 = zr_aot_typed_u64_fn_1();`
- Present: `zr_aot_f6 = zr_aot_typed_f64_fn_2();`
- Present: scalar u64/f64 stack-copy markers and assignments.
- Absent: `ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, 1)`
- Absent: `ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, 2)`
- Absent: `ZrLibrary_AotRuntime_DeoptTypedDirectCall(state,`
- Absent: `ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame, 5, &zr_aot_u5)`
- Absent: `ZrLibrary_AotRuntime_SyncFloatLocal(state, &frame, 6, &zr_aot_f6)`
- Absent: typed-destination `SZrTypeValue` writes for the u64/f64 call results.

## RED/GREEN

- RED: WSL GCC `zr_vm_aot_c_call_shared_library_smoke_test` failed 5/1 because generated C did not include
  `/* zr_aot_static_u64_no_arg_direct_call_full_aot */`.
- GREEN: the full-AOT no-arg dispatcher emits direct scalar thunk calls for the covered scalar kinds and the smoke
  passes without guard/deopt/sync strings.

## Validation

- WSL GCC:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_call_shared_library_smoke_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_call_shared_library_smoke_test`
  - Result: 5 tests, 0 failures, 0 ignored.
- WSL Clang:
  `cmake --build build-wsl-clang --target zr_vm_aot_c_call_shared_library_smoke_test -j 1 && ./build-wsl-clang/bin/zr_vm_aot_c_call_shared_library_smoke_test`
  - Result: 5 tests, 0 failures, 0 ignored.
- WSL GCC typed-call contracts:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test`
  - Result: 4 tests, 0 failures, 0 ignored.
- WSL Clang typed-call contracts:
  `cmake --build build-wsl-clang --target zr_vm_aot_c_typed_call_contracts_test -j 1 && ./build-wsl-clang/bin/zr_vm_aot_c_typed_call_contracts_test`
  - Result: 4 tests, 0 failures, 0 ignored.
- MSVC Debug call smoke:
  `cmake --build E:\Git\zr_vm\build-msvc-aot-stack-copy --config Debug --target zr_vm_aot_c_call_shared_library_smoke_test --parallel 1 && E:\Git\zr_vm\build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_call_shared_library_smoke_test.exe`
  - Result: 5 tests, 0 failures, 5 expected Unix-only ignores.
- MSVC Debug typed-call contracts:
  `cmake --build E:\Git\zr_vm\build-msvc-aot-stack-copy --config Debug --target zr_vm_aot_c_typed_call_contracts_test --parallel 1 && E:\Git\zr_vm\build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_typed_call_contracts_test.exe`
  - Result: 4 tests, 0 failures, 0 ignored.
