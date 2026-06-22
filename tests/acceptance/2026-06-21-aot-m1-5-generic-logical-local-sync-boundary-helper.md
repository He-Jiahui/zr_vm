# AOT M1.5 Generic Logical Local Sync Boundary Helper

## Scope

- Tightened 07-S5 generic logical bool scalar-local synchronization after helper-owned `LOGICAL_NOT`, `LOGICAL_EQUAL`, and `LOGICAL_NOT_EQUAL` destination writes.
- Generated C now keeps only `zr_aot_generic_logical_sync_bool_local_boundary` and `ZrLibrary_AotRuntime_SyncBoolLocal(...)`.

## Baseline

- Before this slice, generic logical helper calls were already boundary-owned, but bool scalar-local refresh still generated `const SZrTypeValue *zr_aot_bool_sync = ZrCore_Stack_GetValue(frame.slotBase + destinationSlot)` plus bool tag checks and nativeBool payload reloads.
- That duplicated stack-slot decoding in generated C after a helper boundary and diverged from the static direct-call and stack-copy sync helper shape.

## Test Inventory

- Source contract: `tests/parser/test_aot_c_logical_contracts.c`.
- Generated C compile/runtime smoke: `tests/parser/test_aot_c_logical_shared_library_smoke.c`.
- Broader focused group: call/shared/power/source/generic numeric/global/logical/typed-scalar/return/frame-setup AOT binaries.
- Generated-product checks: refreshed generic-equality generated C must contain the generic logical sync marker/helper and must not contain `zr_aot_bool_sync`.

## Tooling Evidence

- Tool: WSL `Ubuntu-22.04`, GCC Debug build directory `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```bash
  wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test -j 8 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_logical_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_logical_shared_library_smoke_test'
  ```
- RED result: `zr_vm_aot_c_logical_contracts_test` failed 2/4; both generic truthiness and generic primitive equality contracts missed `zr_aot_generic_logical_sync_bool_local_boundary`.
- GREEN focused command: same command after implementation.
- GREEN broad command:
  ```bash
  wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_call_shared_library_smoke_test zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_power_contracts_test zr_vm_aot_c_power_shared_library_smoke_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_generic_numeric_contracts_test zr_vm_aot_c_generic_numeric_shared_library_smoke_test zr_vm_aot_c_global_shared_library_smoke_test zr_vm_aot_c_logical_contracts_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_typed_scalar_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test -j 8 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_power_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_power_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_generic_numeric_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_logical_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_logical_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test'
  ```
- Generated-product check:
  ```powershell
  Select-String -Path build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_shared_library/generic_equality_project/bin/aot_c/src/main.c -Pattern 'zr_aot_generic_logical_sync_bool_local_boundary|ZrLibrary_AotRuntime_SyncBoolLocal|zr_aot_bool_sync'
  ```

## Results

- `zr_vm_aot_c_logical_contracts_test`: 4 tests, 0 failures.
- `zr_vm_aot_c_logical_shared_library_smoke_test`: 4 tests, 0 failures.
- `zr_vm_aot_c_call_contracts_test`: 4 tests, 0 failures.
- `zr_vm_aot_c_call_shared_library_smoke_test`: 3 tests, 0 failures.
- `zr_vm_aot_c_shared_library_smoke_test`: 8 tests, 0 failures.
- `zr_vm_aot_c_power_contracts_test`: 2 tests, 0 failures.
- `zr_vm_aot_c_power_shared_library_smoke_test`: 1 test, 0 failures.
- `zr_vm_aot_c_source_contracts_test`: 19 tests, 0 failures.
- `zr_vm_aot_c_generic_numeric_contracts_test`: 1 test, 0 failures.
- `zr_vm_aot_c_generic_numeric_shared_library_smoke_test`: 1 test, 0 failures.
- `zr_vm_aot_c_global_shared_library_smoke_test`: 9 tests, 0 failures.
- `zr_vm_aot_c_typed_scalar_test`: 1 test, 0 failures.
- `zr_vm_aot_c_return_contracts_test`: 1 test, 0 failures.
- `zr_vm_aot_c_frame_setup_contracts_test`: 1 test, 0 failures.
- Generated generic-equality C contains `zr_aot_generic_logical_sync_bool_local_boundary` and `ZrLibrary_AotRuntime_SyncBoolLocal(state, &frame, ...)`; `zr_aot_bool_sync` has no matches.

## Acceptance Decision

- Accepted for this 07-S5 sub-slice.
- Reason: generic logical helper-written bool destinations now use the shared local-sync runtime boundary and no longer decode stack slots in generated C.
- Remaining risks: 07-S5 remains partial; typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work. 08-12 remain unstarted.
