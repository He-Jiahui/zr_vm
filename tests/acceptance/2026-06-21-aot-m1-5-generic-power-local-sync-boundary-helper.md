# AOT M1.5 07-S5 Generic Power Local Sync Boundary Helper

Date: 2026-06-22 03:50:11 +08:00

## Scope

This slice extends the existing generic `POW` runtime boundary so scalar-local destinations are restored after `ZrLibrary_AotRuntime_GenericPower()` writes the frame slot.

Covered:

- `backend_aot_write_c_direct_pow()` now receives `const SZrAotExecIrFunction *functionIr`.
- Generic `POW` lowering keeps `zr_aot_generic_power_boundary` and the runtime helper guard.
- Matching i64/u64/f64 scalar destinations emit `zr_aot_generic_power_sync_*_local_boundary` markers and call `ZrLibrary_AotRuntime_SyncSignedIntLocal()`, `SyncUnsignedIntLocal()`, or `SyncFloatLocal()`.
- Function-body dispatch passes `functionIr` into generic `POW` lowering.

Out of scope:

- Full typed-to-typed native signature routing.
- `in` / `out` writeback templates.
- Deopt/dynamic bridge boxing templates.
- Marking 07-S5, 07, or M1.5 complete.

## RED

`zr_vm_aot_c_power_contracts_test` first failed because generic power lowering had no scalar-local include, no `functionIr` parameter, and no i64/u64/f64 sync markers or helper calls.

## GREEN

Focused WSL GCC validation:

- `zr_vm_aot_c_power_contracts_test`: 2/0.
- `zr_vm_aot_c_power_shared_library_smoke_test`: 1/0.

Broader WSL GCC focused group:

- call contracts: 4/0.
- call shared-library smoke: 5/0.
- shared-library smoke: 8/0.
- value-type shared-library smoke: 1/0.
- power contracts/smoke: 2/0 + 1/0.
- source contracts: 19/0.
- generic numeric contracts/smoke: 1/0 + 1/0.
- global smoke: 9/0.
- logical contracts/smoke: 4/0 + 4/0.
- typed scalar: 1/0.
- return contracts: 1/0.
- frame setup contracts: 1/0.

## Notes

The local-sync behavior is locked at source-contract/codegen level. The existing power shared-library smoke validates compileability for the helper boundary; a fully executable generic power scalar-local program remains blocked on broader front-end expressibility for that shape.
