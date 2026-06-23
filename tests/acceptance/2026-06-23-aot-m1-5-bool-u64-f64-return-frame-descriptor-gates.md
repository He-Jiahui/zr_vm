# AOT M1.5 07-S5 Bool/U64/F64 Return Frame Descriptor Gates Acceptance

Date: 2026-06-23 15:35:25 +08:00
Status: Completed sub-slice; 07-S5/M1.5/07 remain partial; 08-12 not started.

## Scope

This slice aligns `FUNCTION_RETURN` frame descriptor eligibility with the direct-return proof gates already available for
bool, u64, and f64 return helpers. It does not claim fully frame-free generation for functions whose other instructions
still require a generated frame descriptor.

## RED

- `zr_vm_aot_c_frame_setup_contracts_test` first failed after the contract required
  `backend_aot_c_scalar_locals_can_direct_return_bool_local(...)` in the frame descriptor return proof.

## Implementation

- `backend_aot_c_frame_descriptor.c` now rejects export-tail returns first, then accepts a local-only
  `FUNCTION_RETURN` when any direct-return proof gate accepts the returned scalar slot:
  i64, bool, u64, or f64.
- Other frame descriptor checks remain conservative. Any instruction that is not proven local-only still forces the
  generated frame descriptor path.

## GREEN

- Focused WSL GCC validation:
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1/0.
  - `zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`: 26/0.
  - `zr_vm_aot_c_typed_direct_call_u64_shared_library_smoke_test`: 23/0.
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
