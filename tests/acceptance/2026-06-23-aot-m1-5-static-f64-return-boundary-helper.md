# AOT M1.5 07-S5 Static F64 Return Boundary Helper Acceptance

Date: 2026-06-23 15:17:34 +08:00
Status: Completed sub-slice; 07-S5/M1.5/07 remain partial; 08-12 not started.

## Scope

This slice adds a narrow native-to-VM return boundary for generated AOT functions whose callable return type is provably
`float` and whose returned value already lives in an f64 scalar local. It does not complete the general typed-return ABI.

## RED

- `zr_vm_aot_c_return_contracts_test` first failed after the contract required
  `backend_aot_write_c_direct_return_f64_local(FILE *file, TZrUInt32 sourceSlot);`.
- The f64 shared-library smoke added a normal static call into an f64-return callee and required the generated callee
  entry to contain `zr_aot_direct_return_f64_local` and `ZrLibrary_AotRuntime_ReturnF64(state, zr_aot_f*)`.

## Implementation

- `ZrLibrary_AotRuntime_ReturnF64()` is declared in the public AOT runtime header and implemented in
  `aot_runtime_return.c`, using `ZrCore_Value_InitAsFloat()` to place the native `TZrFloat64` in the caller result slot.
- `backend_aot_write_c_direct_return_f64_local()` emits `zr_aot_direct_return_f64_local` and
  `ZrLibrary_AotRuntime_ReturnF64(state, zr_aot_f*)`.
- `backend_aot_c_scalar_locals_can_direct_return_f64_local()` allows the route only when the function has callable
  return type `float`, the source slot has an f64 scalar local, the local is written before the return, and exception
  handlers, exported slots, and constructors are absent.
- `FUNCTION_RETURN` now checks the f64 route after the i64, bool, and u64 direct-return routes.

## GREEN

- Focused WSL GCC validation:
  - `zr_vm_aot_c_return_contracts_test`: 1/0.
  - `zr_vm_aot_c_typed_direct_call_f64_shared_library_smoke_test`: 19/0.
- Broader WSL GCC AOT validation:
  - Contracts: source 19/0, call 4/0, typed call 4/0, generic numeric 1/0, global 7/0, logical 4/0, power 2/0,
    frame setup 1/0, return 1/0, value SemIR 4/0, float 1/0.
  - Shared-library smokes: shared 8/0, call 5/0, typed direct-call 5/0, bool 26/0, u64 23/0, f64 19/0, global 9/0,
    logical 4/0, power 1/0.

## Open

- General typed-return ABI.
- Inline structs.
- `in`/`out` writeback.
- Deopt/dynamic bridges.
- Full 07-S5 acceptance and stages 08-12.
