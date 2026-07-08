# AOT 07-S5 Full-AOT Scalar Typed Direct-Call Matrix Guardrail

Timestamp: 2026-07-06 02:57:38 +08:00

## Scope

- Hardened the existing bool and u64 typed direct-call shared-library smoke suites so every case now runs with
  `requireFullAot = ZR_TRUE`.
- Every bool/u64 case now requires the matching `*_direct_call_full_aot` marker and forbids typed direct-call
  metadata guard/deopt/sync runtime helpers.
- This is a coverage and acceptance hardening slice. The production generator was already capable of emitting these
  full-AOT paths after the earlier scalar direct-call work.

## Assertions

- Bool matrix: 28 cases cover no-arg, result-boundary, one-arg, two/three-arg bool, and i64/u64/f64 bool-return
  comparison direct calls.
- U64 matrix: 25 cases cover no-arg, result-boundary, one-arg arithmetic/bitwise const, two-arg arithmetic/bitwise,
  and three-arg arithmetic/bitwise direct calls.
- All targeted cases reject `ZrLibrary_AotRuntime_CanUseTypedDirectCall`, `ZrLibrary_AotRuntime_DeoptTypedDirectCall`,
  and their scalar `Sync*Local` helper.

## RED/GREEN

- RED: not applicable for this guardrail-only slice; the widened full-AOT matrix was immediately GREEN against the
  current generator.
- GREEN: bool/u64 smoke suites now fail future regressions that reintroduce metadata guard/deopt/sync into any covered
  full-AOT scalar typed direct call.

## Validation

- WSL GCC bool typed-direct smoke:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`
  - Result: 28 tests, 0 failures, 0 ignored.
- WSL GCC u64 typed-direct smoke:
  `cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test -j 1 && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`
  - Result: 25 tests, 0 failures, 0 ignored.
- WSL Clang bool/u64 typed-direct smoke:
  `cmake --build build-wsl-clang --target zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test -j 1`
  - Result: bool 28/0; u64 25/0.
- MSVC Debug bool/u64 typed-direct smoke and typed-call contracts:
  `cmake --build E:\Git\zr_vm\build-msvc-aot-stack-copy --target zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test zr_vm_aot_c_typed_call_contracts_test --config Debug -- /m:1`
  - Result: bool 28 tests, 0 failures, 28 expected Unix-only ignores; u64 25 tests, 0 failures,
    25 expected Unix-only ignores; typed-call contracts 4/0.
