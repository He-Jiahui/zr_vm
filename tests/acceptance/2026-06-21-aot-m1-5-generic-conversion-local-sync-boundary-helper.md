# AOT M1.5 07-S5 Generic Conversion Local Sync Boundary Helper

Date: 2026-06-22 04:01:40 +08:00

## Scope

This slice extends generic primitive conversion boundaries so scalar-local destinations are restored after the runtime conversion helper writes the frame slot.

Covered:

- `backend_aot_write_c_direct_to_bool()`, `direct_to_int()`, `direct_to_uint()`, and `direct_to_float()` now receive `const SZrAotExecIrFunction *functionIr`.
- Existing `ZrLibrary_AotRuntime_ConvertGenericToBool/Int/UInt/Float()` helper guards remain the conversion boundary.
- Matching bool/i64/u64/f64 scalar destinations emit `zr_aot_convert_generic_sync_*_local_boundary` markers and call `ZrLibrary_AotRuntime_SyncBoolLocal()`, `SyncSignedIntLocal()`, `SyncUnsignedIntLocal()`, or `SyncFloatLocal()`.
- Function-body dispatch passes `functionIr` into all four generic conversion emitters.

Out of scope:

- Typed numeric conversion lowering, which already uses direct casts and result-skip proof.
- Typed-to-typed native signature routing.
- `in` / `out` writeback templates.
- Deopt/dynamic bridge boxing templates.
- Marking 07-S5, 07, or M1.5 complete.

## RED

`zr_vm_aot_c_source_contracts_test` first failed because generic conversion lowering had no scalar-local include, no `functionIr` parameter, and no bool/i64/u64/f64 sync markers or helper calls.

## GREEN

Focused WSL GCC validation:

- `zr_vm_aot_c_source_contracts_test`: 19/0.
- `zr_vm_aot_c_shared_library_smoke_test`: 8/0.

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

The local-sync behavior is locked by source-contract/codegen shape and exercised by the existing aggregate shared-library generic primitive conversion runtime smoke.
