# AOT M1.5 Static Direct Call Local Sync Boundary Helper

## Scope
- Changed 07-S5 static direct-call scalar-local restoration after `ZrLibrary_AotRuntime_CallStaticDirect(...)`.
- Affected layers: AOT C call lowering, AOT runtime helper ABI, parser call/source/shared-library smoke contracts, AOT semantic docs, and AOT plan/status docs.

## Baseline
- Before this slice, static direct-call lowering refreshed `frame.callInfo` / `frame.slotBase`, then generated `const SZrTypeValue *zr_aot_direct_call_result = ZrCore_Stack_GetValue(frame.slotBase + slot)` plus tag-specific payload reloads to refresh `zr_aot_sN` or `zr_aot_bN`.
- That shape kept call result marshaling details inside generated C instead of a narrow boundary helper, which conflicted with the 07-S5 direction to keep SZrValue/native transitions centralized.

## Test Inventory
- Source contract: `tests/parser/test_aot_c_call_contracts.c`.
- Generated product smokes: `tests/parser/test_aot_c_call_shared_library_smoke.c` and `tests/parser/test_aot_c_shared_library_smoke.c`.
- Regression group: source contracts, power contracts/smoke, generic numeric contracts/smoke, aggregate shared-library smoke, call shared-library smoke, global shared-library smoke, logical shared-library smoke, typed scalar, return contracts, and frame setup contracts.
- Boundary cases covered: i64 local sync helper, bool local sync helper, old `zr_aot_direct_call_result` template absence, and bool helper no-op semantics for non-bool source slots.

## Tooling Evidence
- Tool: WSL `Ubuntu-22.04`, GCC Debug build directory `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```bash
  wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_call_shared_library_smoke_test -j 8 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test'
  ```
- RED result: `zr_vm_aot_c_call_contracts_test` failed after the static direct-call contract required `zr_aot_direct_static_function_call_sync_i64_local_boundary`.
- Intermediate failure: the first broad regression run failed `zr_vm_aot_c_logical_shared_library_smoke_test` because `SyncBoolLocal()` treated a non-bool source slot as `unsupported AOT local sync`. The old generated bool sync only updated the bool local when the result was bool, so the helper was corrected to no-op for non-bool source slots.
- GREEN build command:
  ```bash
  wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_call_shared_library_smoke_test zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_power_contracts_test zr_vm_aot_c_power_shared_library_smoke_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_generic_numeric_contracts_test zr_vm_aot_c_generic_numeric_shared_library_smoke_test zr_vm_aot_c_global_shared_library_smoke_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_typed_scalar_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test -j 8'
  ```
- GREEN run command:
  ```bash
  wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_power_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_power_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_generic_numeric_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_logical_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test'
  ```
- Generated-product checks:
  ```powershell
  Select-String -Path build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_shared_library/numeric_arithmetic_project/bin/aot_c/src/main.c -Pattern 'SyncSignedIntLocal|zr_aot_direct_static_function_call_sync_i64_local_boundary|zr_aot_direct_call_result|ZrCore_Stack_GetValue\(frame.slotBase'
  Select-String -Path build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_shared_library/numeric_arithmetic_project/bin/aot_c/src/main.c -Pattern 'zr_aot_direct_call_result'
  ```
- Generated-product result: the fixture contains `zr_aot_direct_static_function_call_sync_i64_local_boundary` and `ZrLibrary_AotRuntime_SyncSignedIntLocal(state, &frame, ...)`; `zr_aot_direct_call_result` has no matches.

## Results
- `zr_vm_aot_c_call_contracts_test`: 4 tests, 0 failures.
- `zr_vm_aot_c_call_shared_library_smoke_test`: 3 tests, 0 failures.
- `zr_vm_aot_c_shared_library_smoke_test`: 8 tests, 0 failures.
- `zr_vm_aot_c_power_contracts_test`: 2 tests, 0 failures.
- `zr_vm_aot_c_power_shared_library_smoke_test`: 1 test, 0 failures.
- `zr_vm_aot_c_source_contracts_test`: 19 tests, 0 failures.
- `zr_vm_aot_c_generic_numeric_contracts_test`: 1 test, 0 failures.
- `zr_vm_aot_c_generic_numeric_shared_library_smoke_test`: 1 test, 0 failures.
- `zr_vm_aot_c_global_shared_library_smoke_test`: 9 tests, 0 failures.
- `zr_vm_aot_c_logical_shared_library_smoke_test`: 4 tests, 0 failures.
- `zr_vm_aot_c_typed_scalar_test`: 1 test, 0 failures.
- `zr_vm_aot_c_return_contracts_test`: 1 test, 0 failures.
- `zr_vm_aot_c_frame_setup_contracts_test`: 1 test, 0 failures.

## Acceptance Decision
- Accepted for this 07-S5 sub-slice.
- Reason: generated C now treats static direct-call scalar-local restoration as a narrow runtime boundary while preserving old bool-sync behavior.
- Remaining risks: 07-S5 is still partial; typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work. `aot_runtime_values.c` is close to the modularization threshold and should be split before more substantial helper growth.
