# AOT M1.5 Pure Scalar Minimal Prologue

## Scope
- Continued AOT 07-S4 after frame descriptor elision by removing full module-context and stack-frame setup from a descriptor-free, zero-byte pure scalar generated C function.
- Affected layers: AOT runtime entry execution marker, generated C frame setup gating, frame setup source contracts, focused typed scalar generated-product contract, and semantic/plan records.

## Baseline
- The focused pure scalar generated C no longer declared `ZrAotGeneratedFrame frame` and no longer contained `frame.*` references.
- It still emitted full setup context:
  `ZrAotGeneratedModuleContext zr_aot_context`, `ZrLibrary_AotRuntime_ResolveGeneratedModuleContext`, stack frame setup locals, `ZrCore_Function_CheckStackAndGc`, and native-frame null fill.
- Removing context resolution without replacing its execution marker regressed `ZrLibrary_AotRuntime_GetExecutedVia()` from `ZR_LIBRARY_EXECUTED_VIA_AOT_C` to `ZR_LIBRARY_EXECUTED_VIA_NONE`.

## Test Inventory
- `tests/parser/test_aot_c_frame_setup_contracts.c`
  - RED source-contract checks require `includeStackFrameSetup`.
  - RED source-contract checks require `includeStackFrameSetup = (TZrBool)(includeFrameDescriptor || frameByteSize > 0u);`.
  - RED source-contract checks require the minimal early-return path.
  - Source contracts require private runtime-boundary `aot_runtime_mark_record_executed()` and forbid public `ZrLibrary_AotRuntime_MarkGeneratedFunctionExecuted`.
- `tests/parser/test_aot_c_typed_scalar.c`
  - RED generated-product checks forbid full module context resolution.
  - RED generated-product checks forbid stack setup locals, stack/GC checking, stack anchors, and frame null-fill setup in the focused pure scalar output.
  - Generated-product checks forbid the generated lightweight marker call after the marker moved to the runtime entry boundary.
- `tests/parser/test_aot_c_source_contracts.c` and `tests/parser/test_aot_c_return_contracts.c`
  - Regression coverage for existing AOT source and return contracts while the prologue path is split.

## Tooling Evidence
- RED focused tests:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_typed_scalar_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: frame setup contracts failed 1/1 because `TZrBool includeStackFrameSetup` was missing.
- First functional regression after setup elision:
  - Same command after adding the early return.
  - Result: frame setup contracts passed 1/0, but typed scalar failed because `ZrLibrary_AotRuntime_GetExecutedVia(aotState->global)` returned `ZR_LIBRARY_EXECUTED_VIA_NONE` instead of `ZR_LIBRARY_EXECUTED_VIA_AOT_C`.
- GREEN focused tests:
  - Same command after adding `ZrLibrary_AotRuntime_MarkGeneratedFunctionExecuted()`.
  - Result: frame setup contracts 1 test, 0 failures; typed scalar 1 test, 0 failures.
- Final tightened GREEN focused tests:
  - Same command after migrating execution marking to private `aot_runtime_mark_record_executed()` at runtime record-entry/context/shim boundaries and forbidding public/generated markers.
  - Result: frame setup contracts 1 test, 0 failures; typed scalar 1 test, 0 failures.
- WSL GCC contract suite:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_typed_scalar_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: source contracts 19 tests, 0 failures; return contracts 1 test, 0 failures; frame setup contracts 1 test, 0 failures; typed scalar 1 test, 0 failures.
- CTest:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && ctest --test-dir build/codex-aot-07-wsl-gcc-debug -R 'aot_c_typed_scalar|frame_setup|return_contracts|source_contracts' --output-on-failure"`
  - Result: registered `aot_c_typed_scalar` matched and passed 1/1. Frame setup, source-contract, and return-contract binaries are not registered as CTests in this build.
- Generated C inspection:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && grep -nE 'ZrAotGeneratedFrame frame|ZrAotGeneratedModuleContext|ResolveGeneratedModuleContext|ZrLibrary_AotRuntime_MarkGeneratedFunctionExecuted|TZrStackValuePointer zr_aot_(function_base|slot_base|frame_top)|TZrSize zr_aot_(argument_count|frame_slot_count)|ZrCore_Function_CheckStackAndGc|ZrCore_Value_ResetAsNull|frame\\.' build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c || true"`
  - Result: no matches.
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && grep -nE 'static TZrInt64 zr_aot_fn_0|zr_aot_generated_frame_setup|SZrCallInfo \\*zr_aot_call_info|scalar_locals_begin|return_i64_from_local' build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_typed_scalar/project/bin/aot_c/src/main.c | head -40"`
  - Result: function setup shows `zr_aot_generated_frame_setup`, `SZrCallInfo *zr_aot_call_info = state->callInfoList;`, then `zr_aot_scalar_locals_begin`.

## Results
- Private `aot_runtime_mark_record_executed()` records backend execution mode at AOT record-entry/context/shim boundaries without putting a public marker call into generated pure scalar functions.
- `backend_aot_write_c_frame_setup()` computes `includeStackFrameSetup = includeFrameDescriptor || frameByteSize > 0u`.
- This intermediate slice accepted a descriptor-free pure scalar setup block containing only the call-info local before scalar locals.
- Full context and stack setup remain available for descriptor users and nonzero byte-frame functions.

## Acceptance Decision
- Accepted as a 07-S4 sub-slice.
- 07-S4 remains partial. This accepts minimal entry prologue for the focused pure scalar product only; return boundary marshaling, call/cleanup/export/value-frame fallbacks, and broader SZrValue boundary templates remain future 07 work.
- Superseded by `2026-06-21-aot-m1-5-empty-pure-scalar-entry-setup.md`, which removes the remaining focused pure scalar entry setup block and moves `callInfo` lookup into the return boundary.
