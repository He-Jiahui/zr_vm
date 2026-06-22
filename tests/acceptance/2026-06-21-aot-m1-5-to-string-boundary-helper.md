# AOT M1.5 07-S5 TO_STRING Boundary Helper

## Scope

- `TO_STRING` C lowering now emits one `ZrLibrary_AotRuntime_ToString()` helper guard.
- Generated C keeps the `zr_aot_value_exec_to_string` marker but no longer expands source/destination `SZrTypeValue` lookup, direct `ZrCore_Value_ConvertToString()`, frame refresh, string result initialization, or null reset.
- Runtime helper semantics remain centralized in the existing AOT runtime helper, which owns frame slot validation, conversion, call-info/frame refresh, destination relookup, string/null result write, and failure reporting.

## Baseline

- RED: after flipping `zr_vm_aot_c_global_contracts_test` and `zr_vm_aot_c_global_shared_library_smoke_test` to require helper-only `TO_STRING` lowering, the source contract failed while `backend_aot_c_lowering_values.c` still emitted the old direct string conversion template.
- Existing runtime API already exposed `ZrLibrary_AotRuntime_ToString()`, but the C backend did not use it for generated `TO_STRING`.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Focused source contract: `zr_vm_aot_c_global_contracts_test`.
- Generated C compile smoke: `zr_vm_aot_c_global_shared_library_smoke_test`.
- Aggregate AOT contracts and smoke: `zr_vm_aot_c_source_contracts_test`, `zr_vm_aot_c_shared_library_smoke_test`, `zr_vm_aot_c_return_contracts_test`, `zr_vm_aot_c_value_semir_contracts_test`, and `zr_vm_aot_c_control_contracts_test`.
- Generated-product boundary check: `aot_c_to_string_smoke.c` must contain `ZrLibrary_AotRuntime_ToString(state, &frame, 1, 0)` and must not contain the old generated direct string conversion template.

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
- Generated-product check:
  ```bash
  grep -n "zr_aot_value_exec_to_string\|ZrLibrary_AotRuntime_ToString\|ZrCore_Value_ConvertToString\|zr_aot_result_string\|frame.slotBase = zr_aot_call_info\|ZrCore_Value_InitAsRawObject(state, zr_aot_destination\|ZrCore_Value_ResetAsNull(zr_aot_destination" build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_global_shared_library/src/aot_c_to_string_smoke.c
  ```
- Registered-test probe:
  ```bash
  ctest --test-dir build/codex-aot-07-wsl-gcc-debug -R "aot_c_global" --output-on-failure
  ```
- Diff hygiene:
  ```bash
  git diff --check
  ```

## Results

- RED observed: `Missing source contract text: ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ToString(state, &frame, %u, %u));`.
- GREEN focused results: global contracts 6/0 and global shared-library smoke 7/0.
- Generated check passed: `aot_c_to_string_smoke.c` contains `zr_aot_value_exec_to_string` and `ZrLibrary_AotRuntime_ToString(state, &frame, 1, 0)`. It does not contain the old generated `ZrCore_Value_ConvertToString(state, zr_aot_source)`, result-string local, direct frame refresh assignment, direct raw-object string initialization, or destination null-reset template.
- Broader GREEN results: source contracts 19/0, global contracts 6/0, global shared-library smoke 7/0, aggregate shared-library smoke 8/0, return contracts 1/0, value SemIR contracts 4/0, and control contracts 1/0.
- Registered-test probe exited 0 but reported no `aot_c_global` matches in this build; the focused binaries were run directly.
- `git diff --check` exited 0 with only existing LF/CRLF warnings.
- During GREEN validation, one forbidden source-contract token was narrowed because it matched unrelated destination null-reset lowering elsewhere in `backend_aot_c_lowering_values.c`; the helper-only requirement and direct string conversion forbids remain specific to this slice.

## Acceptance Decision

- Accepted for the `TO_STRING` boundary-helper slice.
- 07-S5 remains partial. Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work; 08-12 remain unstarted.
