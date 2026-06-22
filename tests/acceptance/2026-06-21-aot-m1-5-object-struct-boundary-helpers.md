# AOT M1.5 07-S5 Object/Struct Boundary Helpers

## Scope

- `TO_OBJECT` and `TO_STRUCT` C lowering now emit `ZrLibrary_AotRuntime_ToObject()` and `ZrLibrary_AotRuntime_ToStruct()` helper guards.
- Generated C keeps the `zr_aot_value_exec_to_object` and `zr_aot_value_exec_to_struct` markers but no longer expands destination/source `SZrTypeValue` lookup, type-name constant lookup, or direct `ZrCore_Execution_ToObject()` / `ZrCore_Execution_ToStruct()` calls.
- Runtime helper semantics remain centralized in the existing AOT runtime helpers, which own frame slot validation, type-name constant validation, source/destination value lookup, core conversion calls, and failure reporting.

## Baseline

- RED: after flipping `zr_vm_aot_c_global_contracts_test` and `zr_vm_aot_c_global_shared_library_smoke_test` to require helper-only object/struct conversion lowering, the source contract failed while `backend_aot_c_lowering_values.c` still emitted the old direct core conversion templates.
- Existing runtime APIs already exposed `ZrLibrary_AotRuntime_ToObject()` and `ZrLibrary_AotRuntime_ToStruct()`, but the C backend did not use them for generated `TO_OBJECT` / `TO_STRUCT`.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Focused source contract: `zr_vm_aot_c_global_contracts_test`.
- Generated C compile smoke: `zr_vm_aot_c_global_shared_library_smoke_test`.
- Aggregate AOT contracts and smoke: `zr_vm_aot_c_source_contracts_test`, `zr_vm_aot_c_shared_library_smoke_test`, `zr_vm_aot_c_return_contracts_test`, `zr_vm_aot_c_value_semir_contracts_test`, and `zr_vm_aot_c_control_contracts_test`.
- Generated-product boundary check: `aot_c_object_struct_conversion_smoke.c` must contain `ZrLibrary_AotRuntime_ToObject(state, &frame, 1, 0, 0)` and `ZrLibrary_AotRuntime_ToStruct(state, &frame, 2, 1, 0)`, and must not contain the old generated direct core conversion template.

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
  grep -n "zr_aot_value_exec_to_object\|zr_aot_value_exec_to_struct\|ZrLibrary_AotRuntime_ToObject\|ZrLibrary_AotRuntime_ToStruct\|ZrCore_Execution_ToObject\|ZrCore_Execution_ToStruct\|zr_aot_type_name" build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_global_shared_library/src/aot_c_object_struct_conversion_smoke.c
  ```
- Registered-test probe:
  ```bash
  ctest --test-dir build/codex-aot-07-wsl-gcc-debug -R "aot_c_global" --output-on-failure
  ```
- Diff whitespace check:
  ```bash
  git diff --check
  ```

## Results

- RED observed: `Missing source contract text: ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_ToObject(state, &frame, %u, %u, %u));`.
- GREEN focused results: global contracts 6/0 and global shared-library smoke 7/0.
- Generated check passed: `aot_c_object_struct_conversion_smoke.c` contains `zr_aot_value_exec_to_object`, `ZrLibrary_AotRuntime_ToObject(state, &frame, 1, 0, 0)`, `zr_aot_value_exec_to_struct`, and `ZrLibrary_AotRuntime_ToStruct(state, &frame, 2, 1, 0)`. It does not contain the old generated `ZrCore_Execution_ToObject()`, `ZrCore_Execution_ToStruct()`, or `zr_aot_type_name` direct type-name lookup template.
- Broader GREEN results: source contracts 19/0, global contracts 6/0, global shared-library smoke 7/0, aggregate shared-library smoke 8/0, return contracts 1/0, value SemIR contracts 4/0, and control contracts 1/0.
- `ctest -R "aot_c_global"` exited 0 but reported no registered tests in this build; the focused global binaries were run directly.
- `git diff --check` exited 0 with only existing LF/CRLF warnings.
- During GREEN validation, one forbidden source-contract token was narrowed because it matched unrelated constant-table lowering elsewhere in `backend_aot_c_lowering_values.c`; the helper-only requirement and direct object/struct core-call forbids remain specific to this slice.

## Acceptance Decision

- Accepted for the `TO_OBJECT` / `TO_STRUCT` boundary-helper slice.
- 07-S5 remains partial. Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work; 08-12 remain unstarted.
