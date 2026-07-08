# AOT 07-S5 Full-AOT Mixed Bool-Return Typed Direct-Call Boundary

Timestamp: 2026-07-06 02:21:09 +08:00

## Scope

- Extended full-AOT typed direct-call lowering to mixed-argument bool-return two-argument scalar calls.
- Full-AOT i64/u64/f64 argument pairs returning bool now emit direct scalar thunk assignment with
  `zr_aot_static_i64_bool_two_arg_direct_call_full_aot`,
  `zr_aot_static_u64_bool_two_arg_direct_call_full_aot`, or
  `zr_aot_static_f64_bool_two_arg_direct_call_full_aot`.
- The full-AOT mixed bool writer keeps bool destination locals (`zr_aot_b*`) separate from signed/unsigned/float
  argument locals (`zr_aot_s*`, `zr_aot_u*`, `zr_aot_f*`).

## Assertions

- Present: `/* zr_aot_static_i64_bool_two_arg_direct_call_full_aot */`
- Present: `/* zr_aot_static_u64_bool_two_arg_direct_call_full_aot */`
- Present: `/* zr_aot_static_f64_bool_two_arg_direct_call_full_aot */`
- Present: `zr_aot_b5 = zr_aot_typed_bool_fn_1(zr_aot_s6, zr_aot_s7);`
- Present: `zr_aot_b5 = zr_aot_typed_bool_fn_1(zr_aot_u6, zr_aot_u7);`
- Present: `zr_aot_b5 = zr_aot_typed_bool_fn_1(zr_aot_f6, zr_aot_f7);`
- Absent in the targeted generated bodies: `ZrLibrary_AotRuntime_CanUseTypedDirectCall`, `DeoptTypedDirectCall`,
  `SyncBoolLocal`, and `zr_aot_typed_destination`.

## RED/GREEN

- RED: WSL GCC `zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test` failed 28/1 because generated C did not
  include `/* zr_aot_static_i64_bool_two_arg_direct_call_full_aot */`.
- GREEN: the full-AOT typed direct-call dispatcher emits direct scalar mixed bool-return calls after existing
  i64/u64/f64 bool can-write proof succeeds.

## Validation

- WSL GCC bool typed-direct smoke:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`
  - Result: 28 tests, 0 failures, 0 ignored.
- WSL Clang bool typed-direct smoke:
  `cmake --build build-wsl-clang --target zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test -j 1 && ./build-wsl-clang/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`
  - Result: 28 tests, 0 failures, 0 ignored.
- WSL GCC typed-call contracts:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test`
  - Result: 4 tests, 0 failures, 0 ignored.
- WSL Clang typed-call contracts:
  `cmake --build build-wsl-clang --target zr_vm_aot_c_typed_call_contracts_test -j 1 && ./build-wsl-clang/bin/zr_vm_aot_c_typed_call_contracts_test`
  - Result: 4 tests, 0 failures, 0 ignored.
- MSVC Debug typed-call contracts:
  `cmake --build E:\Git\zr_vm\build-msvc-aot-stack-copy --config Debug --target zr_vm_aot_c_typed_call_contracts_test --parallel 1 && E:\Git\zr_vm\build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_typed_call_contracts_test.exe`
  - Result: 4 tests, 0 failures, 0 ignored.
- MSVC Debug bool typed-direct smoke:
  `cmake --build E:\Git\zr_vm\build-msvc-aot-stack-copy --config Debug --target zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test --parallel 1 && E:\Git\zr_vm\build-msvc-aot-stack-copy\bin\Debug\zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test.exe`
  - Result: 28 tests, 0 failures, 28 expected Unix-only ignores.
