# AOT M1.5 07-S5 Copy Stack Boundary Helper

## Scope

- `SET_STACK` fallback lowering now emits an AOT runtime helper guard through `ZrLibrary_AotRuntime_CopyStack()`.
- Generated C keeps the `zr_aot_value_exec_copy_stack` marker but no longer expands frame-slot layout lookup, inline-struct copy, object-to-inline copy, or materialized stack-value assignment templates.
- Scalar-local synchronization after a stack copy remains generated C because it updates AOT register locals from the helper-owned destination slot.
- Runtime helper semantics now include the previous inline-struct/value-slot stack-copy branches.

## Baseline

- RED: after flipping `zr_vm_aot_c_source_contracts_test` to require helper-only stack-copy fallback lowering, the source contract failed with `Missing source contract text: zr_aot_value_exec_copy_stack`.
- A broader smoke run then exposed that scalar-local synchronization still referenced the removed `zr_aot_dense_destination` generated local; the sync block was updated to re-read the destination slot through a narrow local used only for register refresh.
- Existing checkout baseline remains dirty and broader repository health is not claimed by this slice.

## Test Inventory

- Focused source contract: `zr_vm_aot_c_source_contracts_test`.
- Generated C compile/runtime smoke: `zr_vm_aot_c_shared_library_smoke_test`, `zr_vm_aot_c_call_shared_library_smoke_test`, and `zr_vm_aot_c_typed_scalar_test`.
- Generated-product boundary checks:
  - generated `.c` sources under `aot_c_shared_library` and `aot_c_call_shared_library` must contain `ZrLibrary_AotRuntime_CopyStack(...)` calls for stack-copy fallback blocks;
  - generated `.c` sources in those fixtures must not contain the old `/* zr_aot_direct_stack_copy */`, `ZrCore_Function_CopyFrameSlotInline`, `ZrCore_Function_CopyObjectValueToFrameSlotInline`, or direct `ZrCore_Value_AssignMaterializedStackValue` templates.

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
- Call/shared generated coverage:
  ```bash
  cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_call_shared_library_smoke_test -j 8
  ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test
  ```
- Generated-product positive check:
  ```bash
  grep -R --include="*.c" "ZrLibrary_AotRuntime_CopyStack\|zr_aot_value_exec_copy_stack\|zr_aot_direct_stack_copy_sync_destination" \
      build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_shared_library \
      build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_call_shared_library -n
  ```
- Generated-product negative check:
  ```bash
  grep -R --include="*.c" "/\* zr_aot_direct_stack_copy \*/\|ZrCore_Function_CopyFrameSlotInline\|ZrCore_Function_CopyObjectValueToFrameSlotInline\|ZrCore_Value_AssignMaterializedStackValue" \
      build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_shared_library \
      build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_call_shared_library -n
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

- RED observed: source contracts failed 1/19 at the new helper-only stack-copy contract while the generator still emitted the old expanded block.
- GREEN focused results: source contracts 19/0, aggregate shared-library smoke 8/0, typed scalar 1/0, and call shared-library smoke 3/0.
- Generated positive check passed with stack-copy helper calls in both shared and call-shared refreshed fixtures, including scalar-local sync blocks that read `zr_aot_direct_stack_copy_sync_destination`.
- Generated negative check passed: refreshed shared/call `.c` sources no longer contain the old direct stack-copy marker or generated inline-struct/materialized-copy templates.
- Registered-test probe matched only `aot_c_typed_scalar` in this build and passed 1/1; focused source/shared/call binaries were run directly.
- `git diff --check` exited 0 with only existing LF/CRLF warnings.

## Acceptance Decision

- Accepted for the stack-copy fallback boundary-helper slice.
- Modularization note: `ZrLibrary_AotRuntime_CopyStack()` moved out of the 7k+ line `aot_runtime.c` into the dedicated `aot_runtime_values.c` runtime values module, where it shares frame-slot validation helpers with reset-stack-null. `tests/parser/test_aot_c_source_contracts.c` is already oversized, so this slice only tightened the existing value-lowering contract block.
- 07-S5 remains partial. Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work; 08-12 remain unstarted.
