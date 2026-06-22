# AOT M1.5 Return Boundary Module Split

## Scope
- Continued AOT 07-S5 by splitting the focused i64 return marshaling helper out of the monolithic AOT runtime source.
- Affected layers: AOT runtime private module boundary, public i64 return boundary API implementation, return source contracts, focused typed scalar generated-product regression, and AOT plan records.

## Baseline
- `ZrLibrary_AotRuntime_ReturnI64()` already removed inline i64 native-to-VM return packaging from generated C.
- The helper implementation still lived inside `zr_vm_library/src/zr_vm_library/aot_runtime.c`, which is a large runtime-loader and execution-boundary file. That made each new 07-S5 marshaling template add responsibility to the same oversized source file.

## Test Inventory
- `tests/parser/test_aot_c_return_contracts.c`
  - RED source-contract checks require `aot_runtime/aot_runtime_internal.h` and `aot_runtime/aot_runtime_return.c`.
  - Source contracts require the internal header to expose only the opaque runtime-state type plus `aot_runtime_get_state_from_global()` and `aot_runtime_fail()`.
  - Source contracts require `ZrLibrary_AotRuntime_ReturnI64()` to live in the return boundary source and not in `aot_runtime.c`.
- `tests/parser/test_aot_c_typed_scalar.c`
  - Regression coverage keeps the focused generated C return blocks calling `ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_s23/s48)` without inline VM state or `SZrTypeValue` work.
- `tests/parser/test_aot_c_source_contracts.c` and `tests/parser/test_aot_c_frame_setup_contracts.c`
  - Regression coverage for existing AOT source and frame setup contracts while the runtime source layout changes.

## Tooling Evidence
- RED focused test:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_return_contracts_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test"`
  - Result: return contracts failed 1/1 because the new private internal header and return boundary source file were missing.
- GREEN focused test:
  - Same command after adding `aot_runtime_internal.h`, moving `ZrLibrary_AotRuntime_ReturnI64()` to `aot_runtime_return.c`, and making the shared runtime state/error helpers available to internal runtime modules.
  - Result: return contracts 1 test, 0 failures.
- WSL GCC contract suite:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_typed_scalar_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: source contracts 19 tests, 0 failures; return contracts 1 test, 0 failures; frame setup contracts 1 test, 0 failures; typed scalar 1 test, 0 failures.
- CTest:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-aot-07-wsl-gcc-debug -R 'aot_c_typed_scalar|frame_setup|return_contracts|source_contracts' --output-on-failure"`
  - Result: registered `aot_c_typed_scalar` matched and passed 1/1. Frame setup, source-contract, and return-contract binaries are not registered as CTests in this build.
- Generated C inspection:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && grep -nE 'zr_aot_generated_frame_setup|ZrAotGeneratedFrame frame|ZrAotGeneratedModuleContext|ResolveGeneratedModuleContext|ZrLibrary_AotRuntime_MarkGeneratedFunctionExecuted|SZrCallInfo \\*zr_aot_call_info|SZrTypeValue \\*zr_aot_caller_result_value|execution_discard_exception_handlers_for_callinfo|ZrCore_Closure_CloseClosure|ZrCore_Function_TryCopyInlineConstructorReceiverBack|ZrCore_Ownership_ReleaseValue|frame\\.' build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c || true"`
  - Result: no matches.
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && grep -nE 'ZrLibrary_AotRuntime_ReturnI64|static TZrInt64 zr_aot_fn_0|zr_aot_scalar_locals_begin|zr_aot_direct_return_i64_local|ZR_AOT_C_RETURN' build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c | head -80"`
  - Result: `zr_aot_fn_0` enters scalar locals directly; each i64 return block calls `ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_s23/s48)` then `ZR_AOT_C_RETURN(1)`.

## Results
- `ZrLibrary_AotRuntime_ReturnI64()` now lives in a dedicated return boundary module.
- `aot_runtime_internal.h` is the narrow private bridge for boundary modules that need runtime-state lookup and shared diagnostics.
- `aot_runtime.c` no longer contains the focused i64 return helper implementation, while retaining the legacy frame-backed return helper and shared runtime state/error helpers.
- Generated C behavior did not change in this slice; the split preserves the prior helper call shape.

## Acceptance Decision
- Accepted as a 07-S5 sub-slice.
- 07-S5 remains partial. This accepts the module boundary for focused i64 native-to-VM return marshaling only; typed-to-typed direct return, argument unpacking, in/out writeback, deopt/dynamic bridges, and value-frame fallback boundaries remain future 07 work.
