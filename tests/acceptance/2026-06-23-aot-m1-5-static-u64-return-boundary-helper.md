# AOT M1.5 07-S5 Static U64 Return Boundary Helper Acceptance

Date: 2026-06-23 14:26:38 +08:00
Status: Completed sub-slice; 07-S5/M1.5/07 remain partial; 08-12 not started.

## Scope

This slice adds a narrow native-to-VM return boundary for generated AOT functions whose callable return type is provably
`uint` and whose returned value already lives in a u64 scalar local. It does not complete the general typed-return ABI.

## RED

- `zr_vm_aot_c_return_contracts_test` first failed after the contract required
  `backend_aot_write_c_direct_return_u64_local(FILE *file, TZrUInt32 sourceSlot);`.
- The first implementation was too broad: existing int-return u64 smoke cases incorrectly routed through u64 return
  packing and failed their runtime result checks.

## Implementation

- `ZrLibrary_AotRuntime_ReturnU64()` is declared in the public AOT runtime header and implemented in
  `aot_runtime_return.c`, using `ZrCore_Value_InitAsUInt()` to place the native `TZrUInt64` in the caller result slot.
- `backend_aot_write_c_direct_return_u64_local()` emits `zr_aot_direct_return_u64_local` and
  `ZrLibrary_AotRuntime_ReturnU64(state, zr_aot_u*)`.
- `backend_aot_c_scalar_locals_can_direct_return_u64_local()` allows the route only when the function has callable return
  type `uint`, the source slot has a u64 scalar local, the local is written before the return, and exception handlers,
  exported slots, and constructors are absent.
- `FUNCTION_RETURN` now checks the u64 route after the existing i64 direct-return route.
- The u64 shared-library smoke helper now reads unsigned runtime results through `nativeUInt64` when the actual result
  type is unsigned, avoiding signed-union reads for unsigned AOT results.

## GREEN

- Focused WSL GCC validation:
  - `zr_vm_aot_c_return_contracts_test`: 1/0.
  - `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`: 23/0.
- Broader WSL GCC AOT validation at 2026-06-23 14:42:02 +08:00:
  - Contracts: source 19/0, call 4/0, typed call 4/0, generic numeric 1/0, global 7/0, logical 4/0, power 2/0,
    frame setup 1/0, return 1/0, value SemIR 4/0, float 1/0.
  - Shared-library smokes: shared 8/0, call 5/0, typed direct-call 5/0, bool 25/0, u64 23/0, f64 18/0, global 9/0,
    logical 4/0, power 1/0.

## Open

- Bool/f64/general typed-return ABI.
- Inline structs.
- `in`/`out` writeback.
- Deopt/dynamic bridges.
- Full 07-S5 acceptance and stages 08-12.
