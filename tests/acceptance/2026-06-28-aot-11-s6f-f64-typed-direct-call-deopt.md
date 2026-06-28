# 2026-06-28 AOT 11-S6F f64 Typed Direct-Call Metadata Guard/Deopt

Status: completed support sub-slice for 11-S6. Full 11-S6 remains open for bool typed-boundary drift deopt, inline-struct writeback, cross-module token resolve integration, and broader no-crash ABI drift injection.

## Scope

- Extend the typed direct-call metadata guard/deopt path from i64/u64 to f64 scalar direct calls.
- Keep compatible f64 direct thunks on the pure generated-C path, including stateful divide/modulo thunks that still receive `state`.
- On caller/callee binding drift, fall back through `ZrLibrary_AotRuntime_DeoptTypedDirectCall()` and synchronize the float scalar local with `ZrLibrary_AotRuntime_SyncFloatLocal()`.
- Cover parser AOT lowering, typed-direct dispatch function-slot threading, runtime guard reuse, source contract, shared-library smoke, and documentation/status records.

## Baseline

- 11-S6D and 11-S6E covered i64 and u64 scalar typed direct-call guard/deopt. f64 writers still emitted unconditional direct thunk calls and did not carry the caller function slot needed by the deopt helper.
- The first RED source contract required f64 metadata guard markers and failed because the generated-source writer did not emit them.
- Existing broad call-lowering source contracts treated f64 scalar sync helper text as globally forbidden; f64 deopt fallback now legitimately emits float sync.

## RED

- `tests/parser/test_aot_c_call_contracts.c` added `test_aot_c_source_wraps_f64_typed_direct_calls_with_metadata_guard`.
- WSL gcc direct run failed: `7 Tests 1 Failures 0 Ignored`.
- Failure detail: missing source contract text `zr_aot_static_f64_one_arg_direct_call_metadata_guard`.

## GREEN

- `backend_aot_c_emitter.h` gives f64 no/one/two/three-arg direct-call writers a `functionSlot` parameter.
- `backend_aot_c_typed_direct_calls.c` threads the caller function slot through all f64 typed direct-call writer call sites, including the no-arg helper path.
- `backend_aot_c_lowering_calls.c` emits `zr_aot_static_f64_*_direct_call_metadata_guard`, calls `ZrLibrary_AotRuntime_CanUseTypedDirectCall(state, &frame, calleeFlatIndex)`, preserves the compatible direct thunk body, and emits the float deopt fallback.
- The source contract now locks the f64 guard/deopt shape while keeping the existing runtime guard test and f64 shared-library smoke as behavioral coverage.

## Test Inventory

- `tests/module/test_aot_runtime_typed_direct_call_compatibility.c`: 3 runtime guard cases for empty compatible bindings, caller drift deopt, and callee drift deopt.
- `tests/parser/test_aot_c_call_contracts.c`: 7 source contract tests, including i64/u64/f64 typed direct-call guard/deopt generated-source checks.
- `tests/parser/test_aot_c_typed_direct_call_f64_shared_library_smoke.c`: 19 compatible-path f64 direct-call smoke cases on Unix shared-library runners.
- Focused CTest: `aot_runtime_typed_direct_call_compatibility`.

## Tooling Evidence

- WSL gcc:
  - `cmake --build build-wsl-gcc --target zr_vm_aot_runtime_typed_direct_call_compatibility_test zr_vm_aot_c_call_contracts_test zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test -j 2`
  - `./build-wsl-gcc/bin/zr_vm_aot_runtime_typed_direct_call_compatibility_test`
  - `./build-wsl-gcc/bin/zr_vm_aot_c_call_contracts_test`
  - `./build-wsl-gcc/bin/zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test`
  - `ctest --test-dir build-wsl-gcc -R aot_runtime_typed_direct_call_compatibility --output-on-failure`
- WSL clang:
  - `cmake --build build-wsl-clang --target zr_vm_aot_runtime_typed_direct_call_compatibility_test zr_vm_aot_c_call_contracts_test zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test -j 2`
  - `./build-wsl-clang/bin/zr_vm_aot_runtime_typed_direct_call_compatibility_test`
  - `./build-wsl-clang/bin/zr_vm_aot_c_call_contracts_test`
  - `./build-wsl-clang/bin/zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test`
  - `ctest --test-dir build-wsl-clang -R aot_runtime_typed_direct_call_compatibility --output-on-failure`
- Windows MSVC Debug:
  - `cmake --build E:\Git\zr_vm\build-msvc --config Debug --target zr_vm_aot_runtime_typed_direct_call_compatibility_test zr_vm_aot_c_call_contracts_test zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test -- /m:2`
  - `E:\Git\zr_vm\build-msvc\bin\Debug\zr_vm_aot_runtime_typed_direct_call_compatibility_test.exe`
  - `E:\Git\zr_vm\build-msvc\bin\Debug\zr_vm_aot_c_call_contracts_test.exe`
  - `E:\Git\zr_vm\build-msvc\bin\Debug\zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test.exe`
  - `ctest --test-dir E:\Git\zr_vm\build-msvc -C Debug -R aot_runtime_typed_direct_call_compatibility --output-on-failure`

## Results

- WSL gcc: runtime guard 3/0; call contracts 7/0; f64 typed direct-call smoke 19/0; focused CTest 1/1.
- WSL clang: runtime guard 3/0; call contracts 7/0; f64 typed direct-call smoke 19/0; focused CTest 1/1.
- Windows MSVC Debug: runtime guard 3/0; call contracts 7/0; f64 typed direct-call smoke 0 failures / 19 ignored through the existing Unix shared-library branch; focused CTest 1/1.

## Acceptance Decision

Accepted for 11-S6F f64 scalar typed direct-call metadata guard/deopt scope. The generated f64 direct-call path now checks caller/callee metadata binding compatibility before invoking direct thunks and deopts safely through the stack-call path on drift. bool typed-boundary deopt, inline-struct writeback, cross-module token resolve integration, and broader no-crash ABI drift injection remain open 11-S6 work.
