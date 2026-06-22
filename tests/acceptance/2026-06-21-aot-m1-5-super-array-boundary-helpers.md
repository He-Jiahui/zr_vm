# AOT M1.5 07-S5 Super-Array Boundary Helpers

## Scope

- Super-array integer C lowering now emits `ZrLibrary_AotRuntime_SuperArray*()` helper guards for get, set, add, add4, add4-const, and fill4-const operations.
- Generated C keeps the super-array operation markers but no longer expands `SZrTypeValue` slot lookup, fast-path applicability checks, constant extraction, or direct `ZrCore_Object_SuperArray*` calls.
- Runtime helper semantics remain centralized in the existing AOT runtime helper family, including `ZR_INSTRUCTION_USE_RET_FLAG` result-discard handling for `SuperArrayAddInt`.

## Baseline

- RED: after flipping `zr_vm_aot_c_super_array_contracts_test` to require helper-only super-array lowering, the test failed while `backend_aot_c_lowering_super_array.c` still emitted direct generated fast-path/core code.
- Existing runtime API already exposed the `ZrLibrary_AotRuntime_SuperArray*()` helpers, but the C backend did not use them for generated super-array integer instructions.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Focused source contract: `zr_vm_aot_c_super_array_contracts_test`.
- Generated C compile smoke: `zr_vm_aot_c_super_array_shared_library_smoke_test`.
- Aggregate AOT contracts and smoke: `zr_vm_aot_c_source_contracts_test`, `zr_vm_aot_c_shared_library_smoke_test`, `zr_vm_aot_c_return_contracts_test`, `zr_vm_aot_c_value_semir_contracts_test`, and `zr_vm_aot_c_control_contracts_test`.
- Generated-product boundary check: `aot_c_super_array_smoke.c` must contain all runtime helper calls and must not contain the old generated direct super-array fast/core templates.

## Tooling Evidence

- Toolchain: WSL Ubuntu-22.04, GCC/Ninja build directory `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_super_array_contracts_test zr_vm_aot_c_super_array_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_super_array_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_super_array_shared_library_smoke_test
  ```
- GREEN focused commands:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_super_array_contracts_test zr_vm_aot_c_super_array_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_super_array_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_super_array_shared_library_smoke_test
  ```
- Broader focused build and run:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_super_array_contracts_test zr_vm_aot_c_super_array_shared_library_smoke_test zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_value_semir_contracts_test zr_vm_aot_c_control_contracts_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_super_array_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_super_array_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_semir_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_control_contracts_test
  ```
- Registered-test probe:
  ```bash
  ctest --test-dir build/codex-aot-07-wsl-gcc-debug -R 'aot_c_super_array' --output-on-failure
  ```

## Results

- RED observed: `Missing source contract text: ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_SuperArrayGetInt(state, &frame, %u, %u, %u));`.
- GREEN focused results: super-array contracts 1/0 and super-array shared-library smoke 1/0.
- Generated check passed: `aot_c_super_array_smoke.c` contains `SuperArrayGetInt`, `SuperArraySetInt`, `SuperArrayAddInt`, `SuperArrayAddInt4`, `SuperArrayAddInt4Const`, and `SuperArrayFillInt4Const` helper calls, including `ZrLibrary_AotRuntime_SuperArrayAddInt(state, &frame, 65535, 0, 1)` for the return-flag discard case. It does not contain the old generated `ZrCore_Object_SuperArrayTry*Fast`, `ZrCore_Object_SuperArrayAddInt*AssumeFast`, `ZrCore_Object_SuperArrayFillInt4ConstAssumeFast`, or unsupported fast-path templates.
- Broader GREEN results: source contracts 19/0, super-array contracts 1/0, super-array shared-library smoke 1/0, aggregate shared-library smoke 8/0, return contracts 1/0, value SemIR contracts 4/0, and control contracts 1/0.
- The first post-fix build command timed out at the tool limit while the WSL build process was still linking. The process completed, the binaries were updated, and the focused tests passed when run directly afterward.
- `ctest -R 'aot_c_super_array'` exited 0 but reported no registered tests in this build; the focused super-array binaries were run directly.

## Acceptance Decision

- Accepted for the super-array integer boundary-helper slice.
- 07-S5 remains partial. Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work; 08-12 remain unstarted.
