# AOT 07-S5 Full-AOT One-Arg Typed Direct-Call Boundary

Timestamp: 2026-07-06 01:46:11 +08:00

## Scope

- Threaded `requireFullAot` into regular typed direct-call lowering.
- Full-AOT bool/i64/u64/f64 one-argument typed direct calls now emit direct scalar thunk assignment with
  `zr_aot_static_*_one_arg_direct_call_full_aot`.
- Non-full-AOT typed direct calls keep metadata guard and deopt fallback behavior.
- The u64 typed direct-call shared-library smoke now locks the one-arg identity path without
  typed-destination materialization or local-sync fallback.

## Assertions

- Present: `/* zr_aot_static_u64_one_arg_direct_call_full_aot */`
- Present: `zr_aot_u4 = zr_aot_typed_u64_fn_1(zr_aot_u5);`
- Absent: `ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame,`
- Absent: `ZrLibrary_AotRuntime_DeoptTypedDirectCall(state,`
- Absent: `ZrLibrary_AotRuntime_SyncUnsignedIntLocal(state, &frame,`
- Absent: `SZrTypeValue *zr_aot_typed_destination`
- Absent: `ZR_VALUE_FAST_SET(zr_aot_typed_destination,`

## RED/GREEN

- RED: WSL GCC `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test` failed 25/1 because generated C did not
  include `/* zr_aot_static_u64_one_arg_direct_call_full_aot */`.
- GREEN: the full-AOT typed direct-call dispatcher emits direct scalar one-arg thunk calls for proven scalar kinds and
  the u64 smoke passes without guard/deopt/sync strings.

## Validation

- WSL GCC u64 typed-direct smoke:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`
  - Result: 25 tests, 0 failures, 0 ignored.
- WSL Clang u64 typed-direct smoke:
  `cmake --build build-wsl-clang --target zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test -j 1 && ./build-wsl-clang/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`
  - Result: 25 tests, 0 failures, 0 ignored.
- WSL GCC typed-call contracts:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test`
  - Result: 4 tests, 0 failures, 0 ignored.
- WSL Clang typed-call contracts:
  `cmake --build build-wsl-clang --target zr_vm_aot_c_typed_call_contracts_test -j 1 && ./build-wsl-clang/bin/zr_vm_aot_c_typed_call_contracts_test`
  - Result: 4 tests, 0 failures, 0 ignored.
- MSVC Debug typed-call contracts:
  `cmake --build E:\Git\zr_vm\build-msvc-aot-stack-copy --config Debug --target zr_vm_aot_c_typed_call_contracts_test --parallel 1 && E:\Git\zr_vm\build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_typed_call_contracts_test.exe`
  - Result: 4 tests, 0 failures, 0 ignored.
- MSVC Debug u64 typed-direct smoke:
  `cmake --build E:\Git\zr_vm\build-msvc-aot-stack-copy --config Debug --target zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test --parallel 1 && E:\Git\zr_vm\build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test.exe`
  - Result: 25 tests, 0 failures, 25 expected Unix-only ignores.
