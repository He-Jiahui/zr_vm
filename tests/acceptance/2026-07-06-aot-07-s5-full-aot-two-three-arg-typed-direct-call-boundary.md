# AOT 07-S5 Full-AOT Two/Three-Arg Typed Direct-Call Boundary

Timestamp: 2026-07-06 02:07:36 +08:00

## Scope

- Extended full-AOT typed direct-call lowering from no/one-argument calls to proven same-kind two/three-argument
  scalar calls.
- Full-AOT bool/i64/u64/f64 two/three-argument typed direct calls now emit direct scalar thunk assignment with
  `zr_aot_static_*_two_arg_direct_call_full_aot` or `zr_aot_static_*_three_arg_direct_call_full_aot`.
- The full-AOT writer preserves stateful thunk shape when the typed thunk needs `state` for runtime error reporting.
- Mixed-argument bool-return direct-call routes remain on the existing metadata-guarded path.

## Assertions

- Present: `/* zr_aot_static_u64_two_arg_direct_call_full_aot */`
- Present: `/* zr_aot_static_u64_three_arg_direct_call_full_aot */`
- Present: `zr_aot_u5 = zr_aot_typed_u64_fn_1(zr_aot_u6, zr_aot_u7);`
- Present: `zr_aot_u5 = zr_aot_typed_u64_fn_1(state, zr_aot_u6, zr_aot_u7);`
- Present: `zr_aot_u6 = zr_aot_typed_u64_fn_1(zr_aot_u7, zr_aot_u8, zr_aot_u9);`
- Present: `zr_aot_u6 = zr_aot_typed_u64_fn_1(state, zr_aot_u7, zr_aot_u8, zr_aot_u9);`
- Absent in the targeted generated bodies: `ZrLibrary_AotRuntime_CanUseTypedDirectCall`, `DeoptTypedDirectCall`,
  `SyncUnsignedIntLocal`, and `zr_aot_typed_destination`.

## RED/GREEN

- RED: WSL GCC `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test` failed 25/1 because generated C did not
  include `/* zr_aot_static_u64_two_arg_direct_call_full_aot */`.
- GREEN: the full-AOT typed direct-call dispatcher emits direct scalar two/three-arg thunk calls after existing
  can-write proof succeeds. The first post-implementation GCC rerun timed out at 180 seconds; the longer rerun passed.

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
