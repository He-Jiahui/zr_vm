# AOT M1.5 07-S5 END_FINALLY Boundary Helper

## Scope

- `backend_aot_write_c_end_finally()` now emits only the existing `zr_aot_end_finally_direct` marker, initializes `zr_aot_next_instruction`, calls `ZrLibrary_AotRuntime_EndFinally(state, &frame, handlerIndex, &zr_aot_next_instruction)`, and dispatches when the helper returns a resume instruction.
- `ZrLibrary_AotRuntime_EndFinally()` owns handler pop, pending exception unwind, pending return/break/continue resume, pending return-value copy, pending-control cleanup, frame refresh, resume-index calculation, and unhandled exception propagation through `ZrCore_Exception_Throw`.
- Affected layers: AOT C control lowering, AOT runtime helper semantics, source/control contracts, generated control shared-library smoke, and AOT 07 plan/docs.

## Baseline

- RED: `zr_vm_aot_c_control_contracts_test` failed 1/1 after requiring `ZrLibrary_AotRuntime_EndFinally(state, &frame, %u, &zr_aot_next_instruction)` and forbidding the old generated END_FINALLY inline switch/local template.
- RED: after the generated helper shape was introduced, the same focused contract failed 1/1 after requiring `ZrLibrary_AotRuntime_EndFinally()` to perform `execution_unwind_exception_to_handler(state, &resumeCallInfo)` directly so unhandled exceptions still call `ZrCore_Exception_Throw`.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Focused control source contract: `zr_vm_aot_c_control_contracts_test`.
- Aggregate AOT source contract: `zr_vm_aot_c_source_contracts_test`.
- Generated C compile smoke: `zr_vm_aot_c_control_shared_library_smoke_test`.
- Runtime source contract inside the focused control contract: `ZrLibrary_AotRuntime_EndFinally()` must keep direct pending-exception unwind and unhandled-exception propagation instead of delegating to the removed internal exception-resume helper.
- Broader focused AOT group: source, return, frame setup, typed scalar, value SemIR, call, control, and global contract/smoke binaries.
- Generated-product boundary check: control smoke C must contain the END_FINALLY helper and resume dispatch, and must not contain the old generated pending-control switch, `resumeCallInfo`, `handlerState`, `targetSlot`, or pending return-value copy template.

## Tooling Evidence

- Toolchain: WSL Ubuntu-22.04, GCC/Ninja build directory `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_control_contracts_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_control_contracts_test
  ```
- GREEN focused commands:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_control_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_control_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_control_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_control_shared_library_smoke_test
  ```
- Broader focused command:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_typed_scalar_test zr_vm_aot_c_value_semir_contracts_test zr_vm_aot_c_call_contracts_test zr_vm_aot_c_call_shared_library_smoke_test zr_vm_aot_c_control_contracts_test zr_vm_aot_c_control_shared_library_smoke_test zr_vm_aot_c_global_contracts_test zr_vm_aot_c_global_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_semir_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_control_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_control_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_shared_library_smoke_test
  ```
- CTest registration check:
  ```bash
  ctest --test-dir build/codex-aot-07-wsl-gcc-debug -R "aot_c_(typed_scalar|call_shared_library|control_shared_library|global_(contracts|shared_library_smoke))" --output-on-failure
  ```

## Results

- RED observed: `Missing source contract text: ZrLibrary_AotRuntime_EndFinally(state, &frame, %u, &zr_aot_next_instruction)`.
- RED observed: `Missing source contract text: if (!execution_unwind_exception_to_handler(state, &resumeCallInfo))`.
- Focused smoke initially exposed a stale old-shape assertion that still expected `ZrLibrary_AotRuntime_EndFinally` to be absent; the assertion was corrected to require the helper and forbid the old generated switch/local template.
- A follow-up build warned that `aot_runtime_resume_exception_in_current_frame()` was unused after END_FINALLY inlined its exception-resume semantics; the dead internal helper declaration and definition were removed.
- GREEN focused results: control contracts 1/0, source contracts 19/0, control shared-library smoke 1/0.
- GREEN broader results: source contracts 19/0, return contracts 1/0, frame setup contracts 1/0, typed scalar 1/0, value SemIR contracts 4/0, call contracts 4/0, call shared-library smoke 3/0, control contracts 1/0, control shared-library smoke 1/0, global contracts 6/0, global shared-library smoke 7/0.
- Generated check passed: `aot_c_control_smoke.c` contains `ZrLibrary_AotRuntime_EndFinally(state, &frame, 0, &zr_aot_next_instruction)` and the resume-dispatch guard, and does not contain generated `switch (state->pendingControl.kind)`, `SZrCallInfo *resumeCallInfo`, `SZrVmExceptionHandlerState *handlerState`, `TZrStackValuePointer targetSlot`, or `ZrCore_Value_Copy(state, &targetSlot->value, &state->pendingControl.value)`.
- CTest filter matched only the registered `aot_c_typed_scalar` target in this build and passed 1/1; call/control/global shared-library smoke binaries were run directly.

## Acceptance Decision

- Accepted for the END_FINALLY boundary-helper slice.
- 07-S5 remains partial. Pending control, typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work.
