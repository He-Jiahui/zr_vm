# AOT M1.5 07-S5 GET_GLOBAL Boundary Helper

## Scope

- `GET_GLOBAL` C lowering now emits one `ZrLibrary_AotRuntime_GetGlobal()` helper guard.
- Generated C keeps the `zr_aot_value_exec_get_global` marker but no longer expands `SZrTypeValue` destination lookup, global-object type checks, direct `ZrCore_Value_Copy()`, or direct `ZrCore_Value_ResetAsNull()`.
- Runtime helper semantics remain centralized in the existing AOT runtime helper, which owns frame slot validation, destination value lookup, global object copy, and null reset fallback.

## Baseline

- RED: after flipping `zr_vm_aot_c_global_contracts_test` and `zr_vm_aot_c_global_shared_library_smoke_test` to require helper-only `GET_GLOBAL` lowering, the source contract failed while `backend_aot_c_lowering_values.c` still emitted the old direct generated value-copy template.
- Existing runtime API already exposed `ZrLibrary_AotRuntime_GetGlobal()`, but the C backend did not use it for generated `GET_GLOBAL`.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Focused source contract: `zr_vm_aot_c_global_contracts_test`.
- Generated C compile smoke: `zr_vm_aot_c_global_shared_library_smoke_test`.
- Aggregate AOT contracts and smoke: `zr_vm_aot_c_source_contracts_test`, `zr_vm_aot_c_shared_library_smoke_test`, `zr_vm_aot_c_return_contracts_test`, `zr_vm_aot_c_value_semir_contracts_test`, and `zr_vm_aot_c_control_contracts_test`.
- Generated-product boundary check: `aot_c_get_global_smoke.c` must contain `ZrLibrary_AotRuntime_GetGlobal(state, &frame, 0)` and must not contain the old generated direct global value-copy template.

## Tooling Evidence

- Toolchain: WSL Ubuntu-22.04, GCC/Ninja build directory `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_global_contracts_test zr_vm_aot_c_global_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_shared_library_smoke_test
  ```
- GREEN focused commands:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_global_contracts_test zr_vm_aot_c_global_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_shared_library_smoke_test
  ```
- Broader focused build and run:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_global_contracts_test zr_vm_aot_c_global_shared_library_smoke_test zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_value_semir_contracts_test zr_vm_aot_c_control_contracts_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_semir_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_control_contracts_test
  ```
- Registered-test probe:
  ```bash
  ctest --test-dir build/codex-aot-07-wsl-gcc-debug -R 'aot_c_global' --output-on-failure
  ```

## Results

- RED observed: `Missing source contract text: zr_aot_value_exec_get_global`.
- GREEN focused results: global contracts 6/0 and global shared-library smoke 7/0.
- Generated check passed: `aot_c_get_global_smoke.c` contains `zr_aot_value_exec_get_global` and `ZrLibrary_AotRuntime_GetGlobal(state, &frame, 0)`. It does not contain the old generated `const SZrTypeValue *zr_aot_global_object`, `state->global->zrObject.type == ZR_VALUE_TYPE_OBJECT`, direct global `ZrCore_Value_Copy`, or direct destination null-reset template.
- Broader GREEN results: source contracts 19/0, global contracts 6/0, global shared-library smoke 7/0, aggregate shared-library smoke 8/0, return contracts 1/0, value SemIR contracts 4/0, and control contracts 1/0.
- The first combined post-fix command timed out at the tool limit without enough output to classify; the focused build and binaries were rerun separately and passed. The broader focused build exited 0; Ninja emitted `premature end of file; recovering`.
- `ctest -R 'aot_c_global'` exited 0 but reported no registered tests in this build; the focused global binaries were run directly.
- `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Acceptance Decision

- Accepted for the `GET_GLOBAL` boundary-helper slice.
- 07-S5 remains partial. Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work; 08-12 remain unstarted.
