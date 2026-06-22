# AOT M1.5 07-S5 CATCH Boundary Helper

## Scope

- `backend_aot_write_c_catch()` now emits only the existing `zr_aot_catch_direct` marker and a `ZrLibrary_AotRuntime_Catch(state, &frame, destinationSlot)` guard.
- The CATCH destination slot validation, current-exception copy/clear, null fallback, and pending-control cleanup stay centralized in the existing AOT runtime helper.
- Affected layers: AOT C control lowering, source/control contracts, generated control shared-library smoke, and AOT 07 plan/docs.

## Baseline

- RED: `zr_vm_aot_c_control_contracts_test` failed 1/1 after the contract required `ZrLibrary_AotRuntime_Catch(state, &frame, %u)` and forbade the old generated CATCH inline template.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Focused control source contract: `zr_vm_aot_c_control_contracts_test`.
- Aggregate AOT source contract: `zr_vm_aot_c_source_contracts_test`.
- Generated C compile smoke: `zr_vm_aot_c_control_shared_library_smoke_test`.
- Broader focused AOT group: source, return, frame setup, typed scalar, value SemIR, call, control, and global contract/smoke binaries.
- Generated-product boundary check: control smoke C must contain the CATCH helper and must not contain the old generated exception copy/reset template.

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

- RED observed: `Missing source contract text: ZrLibrary_AotRuntime_Catch(state, &frame, %u)`.
- GREEN focused results: control contracts 1/0, source contracts 19/0, control shared-library smoke 1/0.
- GREEN broader results: source contracts 19/0, return contracts 1/0, frame setup contracts 1/0, typed scalar 1/0, value SemIR contracts 4/0, call contracts 4/0, call shared-library smoke 3/0, control contracts 1/0, control shared-library smoke 1/0, global contracts 6/0, global shared-library smoke 7/0.
- Generated check passed: `aot_c_control_smoke.c` contains `ZrLibrary_AotRuntime_Catch(state, ...)` and does not contain the old generated `ZrCore_Value_Copy(state, zr_aot_destination, &state->currentException)`, `ZrCore_Exception_ClearCurrent(state)`, or `ZrCore_Value_ResetAsNull(zr_aot_destination)` CATCH template.
- CTest filter matched only the registered `aot_c_typed_scalar` target in this build and passed 1/1; call/control/global shared-library smoke binaries were run directly.

## Acceptance Decision

- Accepted for the CATCH boundary-helper slice.
- 07-S5 remains partial. THROW, END_FINALLY, pending control, typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work.
