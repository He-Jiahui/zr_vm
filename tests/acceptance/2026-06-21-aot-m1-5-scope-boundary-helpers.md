# AOT M1.5 07-S5 Scope Boundary Helpers

## Scope

- `MARK_TO_BE_CLOSED` and `CLOSE_SCOPE` C lowering now emits `ZrLibrary_AotRuntime_MarkToBeClosed()` and `ZrLibrary_AotRuntime_CloseScope()` helper guards.
- Generated C keeps the scope lifecycle markers but no longer expands slot lookup, to-be-closed closure registration, close-scope stack-top save/restore, or direct closure close loops.
- Runtime semantics remain centralized in the existing AOT runtime helpers and their internal `aot_runtime_close_scope_registrations()` implementation.

## Baseline

- RED: after flipping `zr_vm_aot_c_scope_contracts_test` to require helper-only scope lifecycle lowering, the test failed while `backend_aot_c_lowering_scope.c` still emitted direct generated closure/stack code.
- Existing runtime API already exposed `ZrLibrary_AotRuntime_MarkToBeClosed()` and `ZrLibrary_AotRuntime_CloseScope()`, but the C backend did not use them for generated scope lifecycle instructions.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Focused source contract: `zr_vm_aot_c_scope_contracts_test`.
- Generated C compile smoke: `zr_vm_aot_c_scope_shared_library_smoke_test`.
- Aggregate AOT contracts and smoke: `zr_vm_aot_c_source_contracts_test`, `zr_vm_aot_c_shared_library_smoke_test`, `zr_vm_aot_c_return_contracts_test`, `zr_vm_aot_c_value_semir_contracts_test`, and `zr_vm_aot_c_control_contracts_test`.
- Generated-product boundary check: `aot_c_scope_smoke.c` must contain both runtime helper calls and must not contain the old generated closure/stack templates.

## Tooling Evidence

- Toolchain: WSL Ubuntu-22.04, GCC/Ninja build directory `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_scope_contracts_test zr_vm_aot_c_scope_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_scope_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_scope_shared_library_smoke_test
  ```
- GREEN focused commands:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_scope_contracts_test zr_vm_aot_c_scope_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_scope_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_scope_shared_library_smoke_test
  ```
- Broader focused build and run:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_scope_contracts_test zr_vm_aot_c_scope_shared_library_smoke_test zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_value_semir_contracts_test zr_vm_aot_c_control_contracts_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_scope_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_scope_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_value_semir_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_control_contracts_test
  ```
- Registered-test probe:
  ```bash
  ctest --test-dir build/codex-aot-07-wsl-gcc-debug -R 'aot_c_scope' --output-on-failure
  ```

## Results

- RED observed: `Missing source contract text: ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_MarkToBeClosed(state, &frame, %u));`.
- GREEN focused results: scope contracts 1/0 and scope shared-library smoke 1/0.
- Generated check passed: `aot_c_scope_smoke.c` contains `ZrLibrary_AotRuntime_MarkToBeClosed(state, &frame, 1)` and `ZrLibrary_AotRuntime_CloseScope(state, &frame, 1)`. It does not contain the old generated `ZrCore_Closure_ToBeClosedValueClosureNew`, `ZrCore_Closure_CloseStackValue`, `ZrCore_Closure_CloseRegisteredValues`, `ZrCore_Stack_SavePointerAsOffset`, or `ZrCore_Stack_LoadOffsetToPointer` templates.
- Broader GREEN results: source contracts 19/0, scope contracts 1/0, scope shared-library smoke 1/0, aggregate shared-library smoke 8/0, return contracts 1/0, value SemIR contracts 4/0, and control contracts 1/0.
- The first all-in-one post-fix build-and-run command timed out at the tool limit. The same build and tests were then rerun as separate build and execution commands and passed.
- `ctest -R 'aot_c_scope'` exited 0 but reported no registered tests in this build; the focused scope binaries were run directly.
- `git diff --check` exited 0. It printed only the repository's existing LF/CRLF conversion warnings.

## Acceptance Decision

- Accepted for the scope lifecycle boundary-helper slice.
- 07-S5 remains partial. Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work; 08-12 remain unstarted.
