# AOT M1.5 Generic Power Boundary Helper

## Scope
- Changed 07-S5 generic `POW` C lowering.
- Affected layers: AOT C backend, AOT runtime helper ABI, parser source contracts, generated power shared-library smoke test, and AOT plan/status docs.

## Baseline
- Before this slice, `backend_aot_c_lowering_generic_power.c` expanded generated destination/left/right `SZrTypeValue *` locals, a generated `SZrMeta *zr_aot_meta`, direct `ZrCore_Value_GetMeta(state, zr_aot_left, ZR_META_POW)`, direct null reset, and a generated unsupported meta-dispatch failure block.
- That shape violated the 07-S5 boundary-template direction because generic power meta-boundary semantics lived inside generated C instead of a runtime boundary helper.

## Test Inventory
- Source contract: `tests/parser/test_aot_c_power_contracts.c`.
- Generated product smoke: `tests/parser/test_aot_c_power_shared_library_smoke.c`.
- Regression group: source contracts, generic numeric contracts/smoke, aggregate shared-library smoke, call shared-library smoke, global shared-library smoke, logical shared-library smoke, typed scalar, return contracts, and frame setup contracts.
- Boundary cases covered: generic `POW` helper dispatch, no-meta destination null reset, unsupported meta-function failure string, absence of generated direct meta lookup/reset/error templates, and continued absence of the broader `ZrLibrary_AotRuntime_Pow(state, &frame, ...)` helper in generated C.

## Tooling Evidence
- Tool: WSL `Ubuntu-22.04`, GCC Debug build directory `build/codex-aot-07-wsl-gcc-debug`.
- RED command:
  ```bash
  wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_power_contracts_test zr_vm_aot_c_power_shared_library_smoke_test -j 8 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_power_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_power_shared_library_smoke_test'
  ```
- RED result: `zr_vm_aot_c_power_contracts_test` failed 1/2 with missing source contract text `zr_aot_generic_power_boundary`.
- GREEN command:
  ```bash
  wsl.exe -d Ubuntu-22.04 -- bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_power_contracts_test zr_vm_aot_c_power_shared_library_smoke_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_generic_numeric_contracts_test zr_vm_aot_c_generic_numeric_shared_library_smoke_test zr_vm_aot_c_shared_library_smoke_test zr_vm_aot_c_call_shared_library_smoke_test zr_vm_aot_c_global_shared_library_smoke_test zr_vm_aot_c_logical_shared_library_smoke_test zr_vm_aot_c_typed_scalar_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test -j 8 && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_power_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_power_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_source_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_generic_numeric_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_generic_numeric_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_call_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_global_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_logical_shared_library_smoke_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_scalar_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_return_contracts_test && ./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_frame_setup_contracts_test'
  ```
- Generated-product check:
  ```powershell
  Select-String -Path build/codex-aot-07-wsl-gcc-debug/tests_generated/aot_c_power_shared_library/src/aot_c_typed_power_smoke.c -Pattern 'GenericPower|zr_aot_generic_power|ZrCore_Value_GetMeta\(state, zr_aot_left, ZR_META_POW\)|ZrCore_Value_ResetAsNull\(zr_aot_destination\)|ZrCore_Debug_RunError\(state, "unsupported AOT generic power meta dispatch"\)|ZrLibrary_AotRuntime_Pow\(state, &frame'
  ```
- Generated-product result: only `zr_aot_generic_power_boundary` and `ZrLibrary_AotRuntime_GenericPower(state, &frame, 11, 9, 10)` matched.

## Results
- `zr_vm_aot_c_power_contracts_test`: 2 tests, 0 failures.
- `zr_vm_aot_c_power_shared_library_smoke_test`: 1 test, 0 failures.
- `zr_vm_aot_c_source_contracts_test`: 19 tests, 0 failures.
- `zr_vm_aot_c_generic_numeric_contracts_test`: 1 test, 0 failures.
- `zr_vm_aot_c_generic_numeric_shared_library_smoke_test`: 1 test, 0 failures.
- `zr_vm_aot_c_shared_library_smoke_test`: 8 tests, 0 failures.
- `zr_vm_aot_c_call_shared_library_smoke_test`: 3 tests, 0 failures.
- `zr_vm_aot_c_global_shared_library_smoke_test`: 9 tests, 0 failures.
- `zr_vm_aot_c_logical_shared_library_smoke_test`: 4 tests, 0 failures.
- `zr_vm_aot_c_typed_scalar_test`: 1 test, 0 failures.
- `zr_vm_aot_c_return_contracts_test`: 1 test, 0 failures.
- `zr_vm_aot_c_frame_setup_contracts_test`: 1 test, 0 failures.
- Build emitted existing `project.c` const-qualifier warnings outside this slice.

## Acceptance Decision
- Accepted for this 07-S5 sub-slice.
- Reason: generated C now keeps generic power at a single runtime helper boundary, while the runtime helper preserves the no-meta/null behavior and explicit unsupported meta-dispatch failure.
- Remaining risks: 07-S5 is still partial; typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary templates are still later 07 work.
