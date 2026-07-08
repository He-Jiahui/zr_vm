# AOT 07-S5 Typed Direct-Call Deopt Sync Compact Guard

Timestamp: 2026-07-06 03:31:13 +08:00

## Scope

- Compact non-full-AOT scalar typed direct-call deopt fallback from two guard statements into one guarded expression.
- Preserve the metadata guard path for non-full-AOT typed direct calls.
- Preserve result local synchronization after the deopt fallback for i64, u64, f64, and bool typed direct-call routes.

## Assertions

- Source contracts require `zr_aot_static_typed_direct_call_deopt_sync_compact` in i64/u64/f64/bool typed direct-call
  fallback writers.
- Source contracts reject separate generated `ZR_AOT_C_GUARD(Sync*Local(...));` statements in those fallback writers.
- Generated i64/f64 non-full-AOT smoke output contains a single
  `DeoptTypedDirectCall(...) && SyncSignedIntLocal/SyncFloatLocal(...)` guard.

## RED/GREEN

- RED: WSL GCC `zr_vm_aot_c_call_contracts_test` failed 8 tests / 4 failures after the contracts required the compact
  deopt-sync marker.
- GREEN: `backend_aot_c_lowering_calls.c` and `backend_aot_c_lowering_typed_bool_calls.c` now emit the compact marker
  and combine `DeoptTypedDirectCall(...)` with the typed result sync helper in one `ZR_AOT_C_GUARD(... && ...)`.

## Validation

- WSL GCC call contracts:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_call_contracts_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_call_contracts_test`
  - Result: 8 tests, 0 failures, 0 ignored.
- WSL Clang call contracts:
  `cmake --build build-wsl-clang --target zr_vm_aot_c_call_contracts_test -j 1 && ./build-wsl-clang/bin/zr_vm_aot_c_call_contracts_test`
  - Result: 8 tests, 0 failures, 0 ignored.
- WSL GCC/Clang typed-call contracts:
  `cmake --build <build> --target zr_vm_aot_c_typed_call_contracts_test -j 1 && ./<build>/bin/zr_vm_aot_c_typed_call_contracts_test`
  - Result: 4 tests, 0 failures, 0 ignored on both GCC and Clang.
- WSL GCC/Clang i64 typed direct-call shared-library smoke:
  `cmake --build <build> --target zr_vm_aot_c_typed_direct_call_shared_library_smoke_test -j 1 && ./<build>/bin/zr_vm_aot_c_typed_direct_call_shared_library_smoke_test`
  - Result: 5 tests, 0 failures, 0 ignored on both GCC and Clang.
- WSL GCC/Clang f64 typed direct-call shared-library smoke:
  `cmake --build <build> --target zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test -j 1 && ./<build>/bin/zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test`
  - Result: 19 tests, 0 failures, 0 ignored on both GCC and Clang.
- WSL GCC/Clang source contracts:
  `cmake --build <build> --target zr_vm_aot_c_source_contracts_test -j 1 && ./<build>/bin/zr_vm_aot_c_source_contracts_test`
  - Result: 24 tests, 0 failures, 0 ignored on both GCC and Clang.
- MSVC Debug call contracts and typed-call contracts:
  `cmake --build E:\Git\zr_vm\build-msvc-aot-stack-copy --config Debug --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_typed_call_contracts_test -- /m:1`
  - Result: call contracts 8/0; typed-call contracts 4/0.
- MSVC Debug i64/f64 typed direct-call shared-library smoke and source contracts:
  - Result: i64 smoke 5 tests, 0 failures, 5 expected Unix-only ignores; f64 smoke 19 tests, 0 failures,
    19 expected Unix-only ignores; source contracts 24/0.
