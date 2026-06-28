# AOT 07-S2/S4 Bool Generic Conversion Source Frame Elision

Date: 2026-06-28 09:38:30 +08:00

Status: complete for this focused slice. This does not close all of 07-S2/S4 or 07/M1.5.

## Scope

- Extend bool scalar-local source reuse from direct returns into typed generic primitive conversions.
- Keep `<int> flag`, `<uint> flag`, and `<float> flag` on `zr_aot_b*` when `flag` is already written as a bool scalar local.
- Remove the false liveness dependency caused by unused conversion/unary operand fields defaulting to slot 0.

## RED

- Extended `test_aot_c_generated_shared_library_executes_generic_primitive_conversions` with assertions requiring:
  - `zr_aot_scalar_constant_bool_local`
  - `zr_aot_b0 = ZR_TRUE;`
  - `zr_aot_b0 ? (TZrUInt64)1u : (TZrUInt64)0u;`
  - `zr_aot_b0 ? (TZrFloat64)1.0 : (TZrFloat64)0.0;`
  - no `frame.slotBase[0].value`
- Initial RED showed bool-to-u64/f64 still used frame-source fallback.
- After adding bool-source conversion paths, RED advanced to the bool constant still materializing slot 0. Diagnosis: generic/unary conversion instructions left unused `operand1[1] = 0`, and the generic slot-mention helper treated that as a real read of slot 0.

## GREEN

- `backend_aot_c_scalar_conversion.c`
  - `TO_INT`, `TO_UINT`, and `TO_FLOAT` can now read already-written bool scalar locals directly.
  - u64/f64 fallback conversion paths guard only the destination frame slot when the source is a bool scalar local.
- `backend_aot_c_frame_descriptor.c`
  - `TO_UINT` and `TO_FLOAT` local-only conversion proof now accepts bool-written sources.
- `backend_aot_c_scalar_locals.c`
  - bool local consumers include `TO_INT`, `TO_UINT`, and `TO_FLOAT`.
  - conversion/unary slot mention checks now inspect only the real source operand.
- Source contracts now lock the bool-source conversion expressions and one-source operand mention behavior.

## Verification

- WSL GCC:
  - `zr_vm_aot_c_shared_library_smoke_test`: 13 tests, 0 failures.
  - `zr_vm_aot_c_source_contracts_test`: 21 tests, 0 failures.
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1 test, 0 failures.
- WSL Clang:
  - `zr_vm_aot_c_shared_library_smoke_test`: 13 tests, 0 failures.
  - `zr_vm_aot_c_source_contracts_test`: 21 tests, 0 failures.
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1 test, 0 failures.
- Windows MSVC Debug:
  - shared-library smoke target builds; runtime test reports 0 failures / 13 ignored Unix-only tests.
  - `zr_vm_aot_c_source_contracts_test`: 21 tests, 0 failures.
  - `zr_vm_aot_c_frame_setup_contracts_test`: 1 test, 0 failures.

## Remaining Work

- Destination value slots are still materialized where later stack-copy or runtime boundary behavior needs them.
- Generic runtime boundaries, dynamic/string cases, GC roots, exports, frame cleanup, wider byte-frame narrowing, performance counters, and full typed function-body zero-frame goals remain later 07 work.
