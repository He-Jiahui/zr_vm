# AOT 07-S2/S4 String Logical Bool Sync Parentheses

Date: 2026-06-28 11:05:55 +08:00

Status: complete for this focused slice. This does not close all of 07-S2/S4 or 07/M1.5.

## Scope

- Remove the Clang `-Wlogical-not-parentheses` warning in generated string logical `!=` code.
- Focused case: string equality shared-library smoke where `ss != su` and `ss != st` lower through the direct string logical emitter and sync a bool scalar local.

## RED

- WSL Clang previously warned on generated C like:
  - `zr_aot_b4 = (TZrBool)(!zr_aot_equal != 0u);`
  - `zr_aot_b5 = (TZrBool)(!zr_aot_equal != 0u);`
- Added generated-C assertions to require `(TZrBool)((!zr_aot_equal) != 0u)` and reject `!zr_aot_equal != 0u`.
- The focused WSL GCC logical smoke failed on the new required generated-C assertion before the emitter change.

## GREEN

- `backend_aot_c_write_string_bool_scalar_local()` now emits `(TZrBool)((%s) != 0u)`.
- `backend_aot_c_write_bool_local_sync()` uses the same parenthesized expression template for frame-backed string logical paths.
- Source contracts now lock the parenthesized generic logical bool sync template and forbid the old unparenthesized form.

## Verification

- WSL GCC:
  - `zr_vm_aot_c_logical_shared_library_smoke_test`: 5 tests, 0 failures.
  - `zr_vm_aot_c_source_contracts_test`: 22 tests, 0 failures.
- WSL Clang:
  - `zr_vm_aot_c_logical_shared_library_smoke_test`: 5 tests, 0 failures.
  - `zr_vm_aot_c_source_contracts_test`: 22 tests, 0 failures.
  - The earlier string equality `logical-not-parentheses` warning no longer appears in the focused Clang run.
- Windows MSVC Debug:
  - `zr_vm_aot_c_logical_shared_library_smoke_test`: 0 failures / 5 ignored Unix-only tests.
  - `zr_vm_aot_c_source_contracts_test`: 22 tests, 0 failures.

## Notes

- This is a generated-C quality and warning-cleanliness slice; it does not change runtime semantics.
- Remaining 07 work includes broader generic/dynamic/string boundaries, GC roots, exports, frame cleanup, wider byte-frame narrowing, performance/SZrValue counters, and full typed function-body zero-frame proof.
