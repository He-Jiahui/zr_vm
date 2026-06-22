# AOT M1.5 07-S5 Reset Stack Null Boundary Helpers

## Scope

- `RESET_STACK_NULL` and `RESET_STACK_NULL2` fallback lowering now emits AOT runtime helper guards.
- Generated C keeps `zr_aot_value_exec_reset_stack_null` / `zr_aot_value_exec_reset_stack_null2` markers but no longer expands destination `SZrTypeValue *` locals or direct `ZrCore_Value_ResetAsNull(...)` calls for those fallback blocks.
- Scalar-local skip lowering remains marker-only and does not call the runtime helpers.
- Runtime helper semantics are centralized in `ZrLibrary_AotRuntime_ResetStackNull()` and `ZrLibrary_AotRuntime_ResetStackNull2()`.

## Baseline

- RED: after flipping `zr_vm_aot_c_source_contracts_test` to require helper-only reset fallback lowering, the source contract failed while the runtime helper source was still missing and `backend_aot_c_lowering_values.c` still emitted direct reset templates.
- The existing scalar-local reset skip fixtures were already separate and are intentionally not changed by this slice.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Focused source contract: `zr_vm_aot_c_source_contracts_test`.
- Generated C compile/runtime smoke: `zr_vm_aot_c_shared_library_smoke_test`, `zr_vm_aot_c_call_shared_library_smoke_test`, and `zr_vm_aot_c_typed_scalar_test`.
- Generated-product boundary checks:
  - generated `.c` sources under `aot_c_shared_library` and `aot_c_call_shared_library` must contain `ZrLibrary_AotRuntime_ResetStackNull(...)` or `ZrLibrary_AotRuntime_ResetStackNull2(...)` calls for fallback reset blocks;
  - generated `.c` sources in those fixtures must not contain the old reset2 `SZrTypeValue *zr_aot_first` / direct `ZrCore_Value_ResetAsNull(zr_aot_first)` template.

## Tooling Evidence

- Toolchain: WSL Ubuntu-22.04, GCC/Ninja build directory `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
  ```
- GREEN focused commands:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_typed_scalar_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test
  ```
- Call/shared generated reset2 coverage:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_call_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test
  ```
- Generated-product positive check:
  ```bash
  grep -R --include="*.c" "ResetStackNull\|zr_aot_value_exec_reset_stack_null" \
      build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_call_shared_library \
      build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_shared_library -n
  ```
- Generated-product negative check:
  ```bash
  grep -R --include="*.c" "SZrTypeValue \*zr_aot_first\|ZrCore_Value_ResetAsNull(zr_aot_first" \
      build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_call_shared_library \
      build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_shared_library -n
  ```
- Registered-test probe:
  ```bash
  ctest --test-dir build/codex-aot-07-wsl-gcc-debug -R "aot_c_source|aot_c_shared|aot_c_call_shared|aot_c_typed_scalar" --output-on-failure
  ```
- Diff hygiene:
  ```bash
  git diff --check
  ```

## Results

- RED observed: source contracts failed 1/19 at the new helper-only reset contract before the runtime helper source existed.
- GREEN focused results: source contracts 19/0, aggregate shared-library smoke 8/0, typed scalar 1/0, and call shared-library smoke 3/0.
- Generated positive check passed with fallback reset helper calls, including:
  - `ZrLibrary_AotRuntime_ResetStackNull2(state, &frame, 0, 1)`
  - `ZrLibrary_AotRuntime_ResetStackNull(state, &frame, 0)`
  - `ZrLibrary_AotRuntime_ResetStackNull2(state, &frame, 3, 4)`
  - `ZrLibrary_AotRuntime_ResetStackNull(state, &frame, 1)`
- Generated negative check passed: generated `.c` sources in the refreshed shared/call fixtures no longer contain the old reset2 local `zr_aot_first` template.
- Registered-test probe matched only `aot_c_typed_scalar` in this build and passed 1/1; focused source/shared/call binaries were run directly.
- `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Acceptance Decision

- Accepted for the reset-stack-null fallback boundary-helper slice.
- Modularization note: `tests/parser/test_aot_c_source_contracts.c` is already oversized; this slice only tightened the existing reset assertions there. The new runtime behavior lives in the dedicated 68-line `aot_runtime_values.c` submodule instead of adding more code to the 7k+ line `aot_runtime.c`.
- 07-S5 remains partial. Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work; 08-12 remain unstarted.
