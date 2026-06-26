# 2026-06-24 AOT 12-S8E Full-AOT Generic METHOD Slot Closure

## Scope

12-S8E closes the runtime METHOD-slot branch for statically collected shared generic `CALL_TYPED` in full-AOT mode:

- Default hybrid AOT C output can still use `ZrAot_GenericSlot_Method()` and deopt when the METHOD slot is missing.
- `requireFullAot` directly calls the statically collected `zr_aot_fn_<callee>` method entry.
- The full-AOT generated C no longer contains a METHOD-slot null runtime branch for this collected callsite.

This does not cover manifest-declared dynamic generic instances, reflection `MakeGenericType`, or a complete
mark-and-sweep generic instance closure report.

## Implementation

- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_calls.c`
  keeps the full-AOT no-deopt marker but emits `ZrLibrary_AotRuntime_CallInlineStruct(..., zr_aot_fn_<callee>)`
  directly when the callee function index is already statically resolved.
- The hybrid branch remains unchanged and still emits a callsite-local generic dictionary, METHOD-slot lookup, and
  missing-instance deopt bridge.
- `tests/parser/test_aot_c_generic_call_typed.c`
  now asserts that full-AOT generated C has no `if (zr_aot_generic_call_typed_method == ZR_NULL)` runtime branch.

## RED / GREEN

RED:

- The tightened full-AOT generic call typed fixture failed because generated C still contained the METHOD-slot null
  branch.

GREEN:

- Full-AOT generated C keeps `zr_aot_generic_call_typed_full_aot_no_deopt`.
- Full-AOT generated C omits the METHOD-slot null branch, missing-instance marker, and dynamic deopt bridge.
- The generated full-AOT shared library still executes and returns the interpreter-matching `42`.

## Validation

- `wsl.exe --cd /mnt/e/Git/zr_vm bash -lc 'cmake --build build-wsl-gcc --target zr_vm_aot_c_generic_call_typed_test -j2 && ./build-wsl-gcc/bin/zr_vm_aot_c_generic_call_typed_test'`
  - 6 tests, 0 failures

## Acceptance Decision

Accepted as 08-S7B / 12-S8E only. Statically collected shared generic callsites no longer retain a runtime missing
METHOD branch in full-AOT mode, but dynamic generic instance collection and complete full-AOT closure diagnostics remain
open.
