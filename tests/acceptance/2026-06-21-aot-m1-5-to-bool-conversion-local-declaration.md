# AOT M1.5 07-S5 TO_BOOL Local Declaration

Date: 2026-06-22 04:08:25 +08:00

## Scope

This slice closes the scalar-local proof gap for the generic `TO_BOOL` conversion boundary.

Covered:

- `backend_aot_c_scalar_locals_kind_from_conversion_opcode()` maps `ZR_INSTRUCTION_OP_TO_BOOL` to `ZR_AOT_SCALAR_LOCAL_KIND_BOOL`.
- Source contracts lock the `TO_BOOL` conversion-kind mapping together with the generic conversion local-sync boundary.
- The previous generic conversion sync path can now reach `ZrLibrary_AotRuntime_SyncBoolLocal()` for bool-local destinations.

Out of scope:

- Splitting the already oversized `backend_aot_c_scalar_locals.c`.
- New conversion runtime semantics.
- Typed-to-typed native signature routing.
- `in` / `out` writeback templates.
- Deopt/dynamic bridge boxing templates.
- Marking 07-S5, 07, or M1.5 complete.

## RED

`zr_vm_aot_c_source_contracts_test` first failed because the conversion scalar-local kind mapping did not contain `case ZR_INSTRUCTION_OP_TO_BOOL:` or the bool-kind return.

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

`backend_aot_c_scalar_locals.c` was already above the large-file threshold before this task. This slice keeps the one-line proof correction in place; the smallest follow-up extraction remains a dedicated scalar-local proof/result-skip module.
