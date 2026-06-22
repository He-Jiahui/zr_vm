# AOT M1.5 COPY_STACK Local Sync Boundary Helpers

## Scope

- Tightened 07-S5 stack-copy scalar-local synchronization after `ZrLibrary_AotRuntime_CopyStack(...)`.
- Generated C now keeps only `zr_aot_direct_stack_copy_sync_*_local_boundary` markers and `ZrLibrary_AotRuntime_Sync*Local(...)` guards for bool, signed int, unsigned int, and float locals.
- AOT runtime local-sync helpers moved into `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_sync.c`.

## Baseline

- Before this slice, stack-copy fallback itself was helper-owned, but scalar-local refresh still generated `zr_aot_direct_stack_copy_sync_destination = ZrCore_Stack_GetValue(frame.slotBase + destinationSlot)` plus tag-specific payload reload branches.
- That kept stack-slot decoding inside generated C after a helper boundary and duplicated the same local-sync behavior needed by static direct calls.

## Test Inventory

- Source contract: `tests/parser/test_aot_c_source_contracts.c`.
- Shared-library smoke: `tests/parser/test_aot_c_shared_library_smoke.c`.
- Call contract regression: `tests/parser/test_aot_c_call_contracts.c`.
- Broader focused group: call/shared/power/source/generic numeric/global/logical/typed-scalar/return/frame-setup AOT binaries.
- Generated-product checks: refreshed numeric-arithmetic generated C must contain stack-copy sync helper marker/calls and must not contain `zr_aot_direct_stack_copy_sync_destination`.

## Tooling Evidence

- Tool: WSL `Ubuntu-22.04`, GCC Debug build directory `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```bash
  wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_call_contracts_test -j 8 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test'
  ```
- RED result: `zr_vm_aot_c_source_contracts_test` failed 18/1 after the contract required the new runtime sync source and stack-copy sync helper-only shape; the missing runtime source produced `Expected Non-NULL`.
- GREEN focused command: same command after implementation.
- GREEN broad command:
  ```bash
  wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_call_shared_library_smoke_test zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_power_contracts_test zr_vm_aot_c_power_shared_library_smoke_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_generic_numeric_contracts_test zr_vm_aot_c_generic_numeric_shared_library_smoke_test zr_vm_aot_c_global_shared_library_smoke_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_typed_scalar_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test -j 8 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_power_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_power_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_generic_numeric_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_logical_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test'
  ```
- Generated-product checks:
  ```powershell
  Select-String -Path build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_shared_library/numeric_arithmetic_project/bin/aot_c/src/main.c -Pattern 'direct_stack_copy_sync|SyncSignedIntLocal|SyncUnsignedIntLocal|SyncFloatLocal|SyncBoolLocal|zr_aot_direct_stack_copy_sync_destination'
  Select-String -Path build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_shared_library/numeric_arithmetic_project/bin/aot_c/src/main.c -Pattern 'zr_aot_direct_stack_copy_sync_destination'
  ```

## Results

- `zr_vm_aot_c_source_contracts_test`: 19 tests, 0 failures.
- `zr_vm_aot_c_shared_library_smoke_test`: 8 tests, 0 failures.
- `zr_vm_aot_c_call_contracts_test`: 4 tests, 0 failures.
- `zr_vm_aot_c_call_shared_library_smoke_test`: 3 tests, 0 failures.
- `zr_vm_aot_c_power_contracts_test`: 2 tests, 0 failures.
- `zr_vm_aot_c_power_shared_library_smoke_test`: 1 test, 0 failures.
- `zr_vm_aot_c_generic_numeric_contracts_test`: 1 test, 0 failures.
- `zr_vm_aot_c_generic_numeric_shared_library_smoke_test`: 1 test, 0 failures.
- `zr_vm_aot_c_global_shared_library_smoke_test`: 9 tests, 0 failures.
- `zr_vm_aot_c_logical_shared_library_smoke_test`: 4 tests, 0 failures.
- `zr_vm_aot_c_typed_scalar_test`: 1 test, 0 failures.
- `zr_vm_aot_c_return_contracts_test`: 1 test, 0 failures.
- `zr_vm_aot_c_frame_setup_contracts_test`: 1 test, 0 failures.
- Generated numeric-arithmetic C contains `zr_aot_direct_stack_copy_sync_i64_local_boundary` and `ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, ...)`; `zr_aot_direct_stack_copy_sync_destination` has no matches.
- `aot_runtime_values.c` no longer owns local-sync helper definitions; `aot_runtime_sync.c` contains signed, unsigned, float, and bool local-sync helpers.

## Acceptance Decision

- Accepted for this 07-S5 sub-slice.
- Reason: generated stack-copy scalar-local refresh is now a narrow runtime boundary, matching the static direct-call sync shape and preserving old no-op behavior for type mismatch.
- Modularization note: `aot_runtime_values.c` is 877 lines after the split, and new `aot_runtime_sync.c` is 109 lines.
- Remaining risks: 07-S5 remains partial; typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work. 08-12 remain unstarted.
