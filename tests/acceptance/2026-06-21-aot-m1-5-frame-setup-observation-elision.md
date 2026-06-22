# AOT M1.5 Frame Setup Observation Elision

## Scope
- Began AOT 07-S3 by removing default per-function observation/debug field initialization from generated frame setup.
- Affected layers: frame setup source contract, focused typed scalar generated-product test, and generated C frame setup emitter.

## Baseline
- 07-S1 had already stopped emitting `zr_aot_begin_instruction` in typed function bodies.
- Despite that, `backend_aot_write_c_frame_setup()` still initialized observation state for every generated function:
  - `frame.currentInstructionIndex = 0;`
  - `frame.lastObservedInstructionIndex = UINT32_MAX;`
  - `frame.lastObservedLine = ZR_RUNTIME_DEBUG_HOOK_LINE_NONE;`
  - `frame.observationMask = state->hasAotObservationPolicyOverride ? ...`
  - `frame.publishAllInstructions = state->hasAotObservationPolicyOverride ? ...`
  - `state->debugHookSignal` / `ZR_DEBUG_HOOK_MASK_LINE` line-hook override.

## Test Inventory
- `tests/parser/test_aot_c_frame_setup_contracts.c`
  - RED source-contract change moves the frame setup observation/default-debug initialization strings from required text to forbidden text.
- `tests/parser/test_aot_c_typed_scalar.c`
  - RED generated-product assertions forbid the same frame setup observation initialization strings in focused typed scalar generated C.
  - Runtime path still compiles generated C into a shared library and compares AOT execution against the interpreter result.
- `tests/parser/test_aot_c_source_contracts.c`
  - Regression coverage for existing AOT source contracts.
- Generated C inspection:
  - Confirms the focused typed scalar generated C no longer contains frame setup observation/default-debug initialization strings.
  - The generated macro header still contains `frame.currentInstructionIndex` for failure reporting; this slice only removes the setup-time observation initialization.

## Tooling Evidence
- RED frame setup contract:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_frame_setup_contracts_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test"`
  - Result: failed 1/1 because `frame.currentInstructionIndex = 0;` was still present in `backend_aot_c_frame_setup.c`.
- RED typed scalar generated product:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_scalar_test -j2 && timeout 90s ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test"`
  - Result: failed 1/1 because generated C still contained `frame.currentInstructionIndex = 0;`.
- GREEN focused tests:
  - Same commands after implementation.
  - Results: frame setup contracts 1 test, 0 failures; typed scalar 1 test, 0 failures.
- WSL GCC source contracts:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_frame_setup_contracts_test -j2 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test"`
  - Result: source contracts 19 tests, 0 failures; frame setup contracts 1 test, 0 failures.
- CTest:
  - `wsl bash -lc "cd /mnt/e/Git/zr_vm/build/codex-aot-07-wsl-gcc-debug && ctest -R 'aot_c_typed_scalar|frame_setup' --output-on-failure"`
  - Result: registered `aot_c_typed_scalar` matched and passed 1/1. Frame setup contracts are not registered as a CTest in this build.

## Results
- `backend_aot_write_c_frame_setup()` no longer emits setup-time observation/default-debug initialization.
- Focused typed scalar generated C no longer contains setup assignments for current instruction, last observed instruction/line, observation mask, publish-all flag, or debug-hook line override.
- This is a prologue shrink step toward 07-S3, not full frame removal. The focused generated C still has frame setup, slot-base setup, stack-top maintenance, and return-boundary `SZrTypeValue` marshaling.

## Modularization Note
- `backend_aot_c_frame_setup.c` is 86 lines after this slice and remains a focused frame setup emitter.
- `tests/parser/test_aot_c_frame_setup_contracts.c` is 222 lines and remains focused.
- `tests/parser/test_aot_c_typed_scalar.c` is 1031 lines. This slice adds only six generated-product negative assertions to the existing focused fixture. The smallest follow-up split remains extracting typed-scalar generated-C marker/forbidden-token assertions once the remaining 07-S2/07-S3 checks stabilize.

## Acceptance Decision
- Accepted as the first 07-S3 sub-slice.
- 07-S3 remains partial. Remaining 07-S3 work includes MethodInfo emission, fat-frame field removal from typed scalar prologue, and preserving needed dynamic/boundary fallback paths.
