# AOT M1.5 07-S5 TYPEOF Boundary Helper

## Scope

- `TYPEOF` C lowering now emits one `ZrLibrary_AotRuntime_TypeOf()` helper guard.
- Generated C keeps the `zr_aot_value_exec_typeof` marker but no longer expands destination/source `SZrTypeValue` lookup or direct `ZrCore_Reflection_TypeOfValue()` calls.
- Runtime helper semantics remain centralized in the existing AOT runtime helper, which owns frame slot validation, source/destination value lookup, reflection call, and failure reporting.

## Baseline

- RED: after flipping `zr_vm_aot_c_global_contracts_test` and `zr_vm_aot_c_global_shared_library_smoke_test` to require helper-only `TYPEOF` lowering, the source contract failed while `backend_aot_c_lowering_values.c` still emitted the old direct reflection template.
- Existing runtime API already exposed `ZrLibrary_AotRuntime_TypeOf()`, but the C backend did not use it for generated `TYPEOF`.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Focused source contract: `zr_vm_aot_c_global_contracts_test`.
- Generated C compile smoke: `zr_vm_aot_c_global_shared_library_smoke_test`.
- Aggregate AOT contracts and smoke: `zr_vm_aot_c_source_contracts_test`, `zr_vm_aot_c_shared_library_smoke_test`, `zr_vm_aot_c_return_contracts_test`, `zr_vm_aot_c_value_semir_contracts_test`, and `zr_vm_aot_c_control_contracts_test`.
- Generated-product boundary check: `aot_c_typeof_smoke.c` must contain `ZrLibrary_AotRuntime_TypeOf(state, &frame, 1, 0)` and must not contain the old generated direct reflection template.

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

- RED observed: `Missing source contract text: ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_TypeOf(state, &frame, %u, %u));`.
- GREEN focused results: global contracts 6/0 and global shared-library smoke 7/0.
- Generated check passed: `aot_c_typeof_smoke.c` contains `zr_aot_value_exec_typeof` and `ZrLibrary_AotRuntime_TypeOf(state, &frame, 1, 0)`. It does not contain the old generated `ZrCore_Reflection_TypeOfValue(state, zr_aot_source, zr_aot_destination)` direct reflection template.
- Broader GREEN results: source contracts 19/0, global contracts 6/0, global shared-library smoke 7/0, aggregate shared-library smoke 8/0, return contracts 1/0, value SemIR contracts 4/0, and control contracts 1/0.
- `ctest -R 'aot_c_global'` exited 0 but reported no registered tests in this build; the focused global binaries were run directly.
- `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Acceptance Decision

- Accepted for the `TYPEOF` boundary-helper slice.
- 07-S5 remains partial. Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work; 08-12 remain unstarted.
