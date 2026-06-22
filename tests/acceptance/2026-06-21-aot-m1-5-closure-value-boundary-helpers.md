# AOT M1.5 07-S5 Closure Value Boundary Helpers

## Scope

- `GET_CLOSURE`, `SET_CLOSURE`, `GETUPVAL`, and `SETUPVAL` C lowering now emit AOT runtime helper guards.
- Generated C keeps `zr_aot_value_exec_get_closure_value` / `zr_aot_value_exec_set_closure_value` markers but no longer expands current-closure stack lookup, native/VM closure decoding, capture-value read/write, direct copy, or setter barrier templates.
- Runtime helper semantics remain centralized in the existing `ZrLibrary_AotRuntime_GetClosureValue()` and `ZrLibrary_AotRuntime_SetClosureValue()` helpers.

## Baseline

- RED: after flipping `zr_vm_aot_c_constant_contracts_test` and `zr_vm_aot_c_global_shared_library_smoke_test` to require helper-only closure capture access lowering, the source contract failed while `backend_aot_c_lowering_values.c` still emitted direct closure/capture access templates.
- Existing runtime APIs already exposed `ZrLibrary_AotRuntime_GetClosureValue()` and `ZrLibrary_AotRuntime_SetClosureValue()`, but the C backend did not use them for generated closure capture access.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Focused source contract: `zr_vm_aot_c_constant_contracts_test`.
- Generated C compile smoke: `zr_vm_aot_c_global_shared_library_smoke_test`.
- Aggregate AOT contracts and smoke: `zr_vm_aot_c_source_contracts_test`, `zr_vm_aot_c_shared_library_smoke_test`, `zr_vm_aot_c_return_contracts_test`, `zr_vm_aot_c_value_semir_contracts_test`, and `zr_vm_aot_c_control_contracts_test`.
- Generated-product boundary check: `aot_c_closure_value_access_smoke.c` must contain `ZrLibrary_AotRuntime_GetClosureValue(state, &frame, 0, 0)`, `ZrLibrary_AotRuntime_SetClosureValue(state, &frame, 0, 0)`, `ZrLibrary_AotRuntime_GetClosureValue(state, &frame, 1, 0)`, and `ZrLibrary_AotRuntime_SetClosureValue(state, &frame, 1, 0)`, and must not contain the old generated direct capture access templates.

## Tooling Evidence

- Toolchain: WSL Ubuntu-22.04, GCC/Ninja build directory `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_constant_contracts_test zr_vm_aot_c_global_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_constant_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_shared_library_smoke_test
  ```
- GREEN focused commands:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_constant_contracts_test zr_vm_aot_c_global_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_constant_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_shared_library_smoke_test
  ```
- Broader focused build and run:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_constant_contracts_test zr_vm_aot_c_global_shared_library_smoke_test zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_value_semir_contracts_test zr_vm_aot_c_control_contracts_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_constant_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_semir_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_control_contracts_test
  ```
- Generated-product check:
  ```bash
  grep -n "zr_aot_value_exec_get_closure_value\|zr_aot_value_exec_set_closure_value\|ZrLibrary_AotRuntime_GetClosureValue\|ZrLibrary_AotRuntime_SetClosureValue\|ZrCore_ClosureNative_GetCaptureValue\|ZrCore_ClosureValue_GetValue\|ZrCore_Value_Barrier(state, zr_aot_barrier_object" build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_global_shared_library/src/aot_c_closure_value_access_smoke.c
  ```
- Registered-test probe:
  ```bash
  ctest --test-dir build/codex-aot-07-wsl-gcc-debug -R "aot_c_constant|aot_c_global" --output-on-failure
  ```
- Diff hygiene:
  ```bash
  git diff --check
  ```

## Results

- RED observed: `Missing source contract text: ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetClosureValue(state, &frame, %u, %u));`.
- GREEN focused results: constant contracts 4/0 and global shared-library smoke 8/0.
- Generated check passed: `aot_c_closure_value_access_smoke.c` contains both get/set markers and all four helper calls:
  - `ZrLibrary_AotRuntime_GetClosureValue(state, &frame, 0, 0)`
  - `ZrLibrary_AotRuntime_SetClosureValue(state, &frame, 0, 0)`
  - `ZrLibrary_AotRuntime_GetClosureValue(state, &frame, 1, 0)`
  - `ZrLibrary_AotRuntime_SetClosureValue(state, &frame, 1, 0)`
- Generated check also confirmed the fixture does not contain the old generated `ZrCore_ClosureNative_GetCaptureValue`, `ZrCore_ClosureValue_GetValue`, or setter barrier local template.
- Broader GREEN results: source contracts 19/0, constant contracts 4/0, global shared-library smoke 8/0, aggregate shared-library smoke 8/0, return contracts 1/0, value SemIR contracts 4/0, and control contracts 1/0.
- Registered-test probe exited 0 but reported no `aot_c_constant` or `aot_c_global` matches in this build; the focused binaries were run directly.
- `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Acceptance Decision

- Accepted for the closure capture access boundary-helper slice.
- 07-S5 remains partial. Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work; 08-12 remain unstarted.
