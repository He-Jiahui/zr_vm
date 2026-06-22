# AOT M1.5 07-S5 CREATE_OBJECT/CREATE_ARRAY Boundary Helpers

## Scope

- `CREATE_OBJECT` and `CREATE_ARRAY` C lowering now each emit one AOT runtime helper guard.
- Generated C keeps `zr_aot_value_exec_create_object` / `zr_aot_value_exec_create_array` markers but no longer expands object/array allocation, destination `SZrTypeValue` lookup, ownership release, raw-object initialization, array type tagging, or null reset templates.
- Runtime helper semantics remain centralized in the existing `ZrLibrary_AotRuntime_CreateObject()` and `ZrLibrary_AotRuntime_CreateArray()` helpers.

## Baseline

- RED: after flipping `zr_vm_aot_c_global_contracts_test` and `zr_vm_aot_c_global_shared_library_smoke_test` to require helper-only object/array creation lowering, the source contract failed while `backend_aot_c_lowering_values.c` still emitted direct core object/array creation templates.
- Existing runtime APIs already exposed `ZrLibrary_AotRuntime_CreateObject()` and `ZrLibrary_AotRuntime_CreateArray()`, but the C backend did not use them for generated `CREATE_OBJECT` / `CREATE_ARRAY`.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Focused source contract: `zr_vm_aot_c_global_contracts_test`.
- Generated C compile smoke: `zr_vm_aot_c_global_shared_library_smoke_test`.
- Aggregate AOT contracts and smoke: `zr_vm_aot_c_source_contracts_test`, `zr_vm_aot_c_shared_library_smoke_test`, `zr_vm_aot_c_return_contracts_test`, `zr_vm_aot_c_value_semir_contracts_test`, and `zr_vm_aot_c_control_contracts_test`.
- Generated-product boundary check: `aot_c_object_array_creation_smoke.c` must contain `ZrLibrary_AotRuntime_CreateObject(state, &frame, 0)` and `ZrLibrary_AotRuntime_CreateArray(state, &frame, 1)` and must not contain the old generated direct core object/array creation template.

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
  grep -n "zr_aot_value_exec_create_object\|zr_aot_value_exec_create_array\|ZrLibrary_AotRuntime_CreateObject\|ZrLibrary_AotRuntime_CreateArray\|ZrCore_Object_New(state, ZR_NULL)\|ZrCore_Object_NewCustomized(state,\|zr_aot_destination->type = ZR_VALUE_TYPE_ARRAY" build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_global_shared_library/src/aot_c_object_array_creation_smoke.c
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

- RED observed: `Missing source contract text: zr_aot_value_exec_create_object`.
- GREEN focused results: global contracts 7/0 and global shared-library smoke 8/0.
- Generated check passed: `aot_c_object_array_creation_smoke.c` contains `zr_aot_value_exec_create_object`, `ZrLibrary_AotRuntime_CreateObject(state, &frame, 0)`, `zr_aot_value_exec_create_array`, and `ZrLibrary_AotRuntime_CreateArray(state, &frame, 1)`. It does not contain the old generated direct object/array allocation templates or direct array type-tag assignment.
- Broader GREEN results: source contracts 19/0, global contracts 7/0, global shared-library smoke 8/0, aggregate shared-library smoke 8/0, return contracts 1/0, value SemIR contracts 4/0, and control contracts 1/0.
- Registered-test probe exited 0 but reported no `aot_c_global` matches in this build; the focused binaries were run directly.
- `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Acceptance Decision

- Accepted for the `CREATE_OBJECT` / `CREATE_ARRAY` boundary-helper slice.
- 07-S5 remains partial. Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work; 08-12 remain unstarted.
