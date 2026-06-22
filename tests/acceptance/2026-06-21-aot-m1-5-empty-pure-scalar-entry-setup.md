# AOT M1.5 Empty Pure Scalar Entry Setup

## Scope
- Continued AOT 07-S4 after descriptor-free minimal prologue by removing the remaining focused pure scalar entry setup block.
- Affected layers: generated C frame setup gating, direct i64 return boundary packaging, frame setup source contracts, focused typed scalar generated-product contract, and semantic/plan records.

## Baseline
- The descriptor-free zero-byte pure scalar generated C no longer declared `ZrAotGeneratedFrame frame`, no longer resolved full generated module context, and no longer called a generated runtime marker.
- It still emitted `/* zr_aot_generated_frame_setup */` plus a function-scope `SZrCallInfo *zr_aot_call_info = state->callInfoList;` before scalar locals, only because direct return later used that local.

## Test Inventory
- `tests/parser/test_aot_c_typed_scalar.c`
  - RED generated-product check forbids `/* zr_aot_generated_frame_setup */`.
  - Generated-product checks require `SZrCallInfo *zr_aot_call_info = ZR_NULL;` and `zr_aot_call_info = state->callInfoList;` inside `zr_aot_direct_return_i64_local`.
- `tests/parser/test_aot_c_frame_setup_contracts.c`
  - Source contract locks `if (!includeStackFrameSetup) { return; }` before the frame setup `fprintf`.
- `tests/parser/test_aot_c_source_contracts.c` and `tests/parser/test_aot_c_return_contracts.c`
  - Regression coverage for existing AOT source and return contracts while the entry setup path is made empty.

## Tooling Evidence
- RED focused test:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_scalar_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: typed scalar failed 1/1 because generated C still contained forbidden token `/* zr_aot_generated_frame_setup */`.
- GREEN focused tests:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_typed_scalar_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: frame setup contracts 1 test, 0 failures; typed scalar 1 test, 0 failures.
- WSL GCC contract suite:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_typed_scalar_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: source contracts 19 tests, 0 failures; return contracts 1 test, 0 failures; frame setup contracts 1 test, 0 failures; typed scalar 1 test, 0 failures.
- CTest:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-aot-07-wsl-gcc-debug -R 'aot_c_typed_scalar|frame_setup|return_contracts|source_contracts' --output-on-failure"`
  - Result: registered `aot_c_typed_scalar` matched and passed 1/1. Frame setup, source-contract, and return-contract binaries are not registered as CTests in this build.
- Generated C inspection:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && grep -nE 'zr_aot_generated_frame_setup|ZrAotGeneratedFrame frame|ZrAotGeneratedModuleContext|ResolveGeneratedModuleContext|ZrLibrary_AotRuntime_MarkGeneratedFunctionExecuted|TZrStackValuePointer zr_aot_(function_base|slot_base|frame_top)|TZrSize zr_aot_(argument_count|frame_slot_count)|ZrCore_Function_CheckStackAndGc|ZrCore_Value_ResetAsNull|frame\\.' build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c || true"`
  - Result: no matches.
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && grep -nE 'static TZrInt64 zr_aot_fn_0|zr_aot_scalar_locals_begin|zr_aot_direct_return_i64_local|SZrCallInfo \\*zr_aot_call_info|zr_aot_call_info = state->callInfoList|return_i64_from_local' build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c | head -50"`
  - Result: `zr_aot_fn_0` enters `zr_aot_scalar_locals_begin` directly; `SZrCallInfo *zr_aot_call_info = ZR_NULL;` and `zr_aot_call_info = state->callInfoList;` appear inside each `zr_aot_direct_return_i64_local` block.

## Results
- `backend_aot_write_c_frame_setup()` emits no generated-C setup when `includeStackFrameSetup` is false.
- `backend_aot_write_c_direct_return_i64_local()` owns the remaining VM-boundary `callInfo` lookup used for native-to-VM return packaging.
- Descriptor-free zero-byte pure scalar generated C has an empty entry setup path before scalar locals.
- Full context and stack setup remain available for descriptor users and nonzero byte-frame functions.

## Acceptance Decision
- Accepted as a 07-S4 sub-slice.
- 07-S4 remains partial. This accepts empty entry setup for the focused pure scalar product only; return boundary marshaling, typed-to-typed direct return, call/cleanup/export/value-frame fallbacks, and broader SZrValue boundary templates remain future 07 work.
- Extended by `2026-06-21-aot-m1-5-i64-return-boundary-helper.md`, which moves the focused i64 native-to-VM return packaging out of generated C and into a runtime boundary helper.
