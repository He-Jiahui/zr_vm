# AOT M1.5 I64 Return Boundary Helper

## Scope
- Continued AOT 07-S5 by moving the focused i64 native-to-VM return packaging out of generated C and into a runtime boundary helper.
- Affected layers: AOT runtime public boundary API, generated C direct i64 return emitter, return source contracts, focused typed scalar generated-product contract, and semantic/plan records.

## Baseline
- Descriptor-free zero-byte pure scalar generated C entered scalar locals directly.
- `zr_aot_direct_return_i64_local` still inlined VM-boundary work in generated C: `SZrCallInfo`, caller `SZrTypeValue`, exception-handler discard, stack-top restoration, closure close, constructor receiver copy-back, ownership release, and caller result field writes.

## Test Inventory
- `tests/parser/test_aot_c_return_contracts.c`
  - RED source-contract checks require `ZrLibrary_AotRuntime_ReturnI64(struct SZrState *state,`.
  - Source contracts require the runtime implementation to load `state->callInfoList` and write through `ZrCore_Value_InitAsInt(state, callerResultValue, value);`.
  - Source contracts require the direct i64 return emitter to call `ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_s%u)`.
- `tests/parser/test_aot_c_typed_scalar.c`
  - RED generated-product checks require `ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_s23/s48)`.
  - Generated-product checks forbid inline `SZrCallInfo`, caller `SZrTypeValue`, closure close, constructor receiver copy-back, ownership release, and direct caller result field writes in the focused generated C.
- `tests/parser/test_aot_c_source_contracts.c` and `tests/parser/test_aot_c_frame_setup_contracts.c`
  - Regression coverage for existing AOT source and frame setup contracts while the return boundary changes.

## Tooling Evidence
- RED focused tests:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_return_contracts_test zr_vm_aot_c_typed_scalar_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: return contracts failed 1/1 because `ZrLibrary_AotRuntime_ReturnI64(struct SZrState *state,` was missing.
- GREEN focused tests:
  - Same command after adding the runtime helper and replacing inline generated return packaging.
  - Result: return contracts 1 test, 0 failures; typed scalar 1 test, 0 failures.
- WSL GCC contract suite:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_typed_scalar_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: source contracts 19 tests, 0 failures; return contracts 1 test, 0 failures; frame setup contracts 1 test, 0 failures; typed scalar 1 test, 0 failures.
- CTest:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-aot-07-wsl-gcc-debug -R 'aot_c_typed_scalar|frame_setup|return_contracts|source_contracts' --output-on-failure"`
  - Result: registered `aot_c_typed_scalar` matched and passed 1/1. Frame setup, source-contract, and return-contract binaries are not registered as CTests in this build.
- Generated C inspection:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && grep -nE 'zr_aot_generated_frame_setup|ZrAotGeneratedFrame frame|ZrAotGeneratedModuleContext|ResolveGeneratedModuleContext|ZrLibrary_AotRuntime_MarkGeneratedFunctionExecuted|SZrCallInfo \\*zr_aot_call_info|SZrTypeValue \\*zr_aot_caller_result_value|execution_discard_exception_handlers_for_callinfo|ZrCore_Closure_CloseClosure|ZrCore_Function_TryCopyInlineConstructorReceiverBack|ZrCore_Ownership_ReleaseValue|frame\\.' build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c || true"`
  - Result: no matches.
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && grep -nE 'static TZrInt64 zr_aot_fn_0|zr_aot_scalar_locals_begin|zr_aot_direct_return_i64_local|ZrLibrary_AotRuntime_ReturnI64|ZR_AOT_C_RETURN' build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c | head -60"`
  - Result: `zr_aot_fn_0` enters scalar locals directly; each i64 return block calls `ZrLibrary_AotRuntime_ReturnI64(state, zr_aot_s23/s48)` then `ZR_AOT_C_RETURN(1)`.

## Results
- `ZrLibrary_AotRuntime_ReturnI64()` is the focused native-to-VM i64 return boundary helper.
- Direct i64 local return generated C no longer inlines VM state maintenance or caller `SZrTypeValue` field writes.
- The pure scalar function body remains frame/context/setup free; only the explicit boundary helper call remains at return.

## Acceptance Decision
- Accepted as a 07-S5 sub-slice.
- 07-S5 remains partial. This accepts the focused i64 native-to-VM return boundary only; typed-to-typed direct return, argument unpacking, in/out writeback, deopt/dynamic bridges, and value-frame fallback boundaries remain future 07 work.
