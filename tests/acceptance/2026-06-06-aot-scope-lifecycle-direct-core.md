# AOT Scope Lifecycle Direct Core

## Scope

Generated AOT C lowers `MARK_TO_BE_CLOSED` and `CLOSE_SCOPE` directly instead of calling instruction-shaped AOT runtime helpers.

## Evidence

- RED: GCC `zr_vm_aot_c_scope_contracts_test` first failed because `backend_aot_c_lowering_scope.c` did not exist.
- GREEN: GCC and Clang `zr_vm_aot_c_scope_contracts_test` pass with 1 test and 0 failures.
- GREEN: GCC and Clang `zr_vm_aot_c_scope_shared_library_smoke_test` pass with 1 test and 0 failures, compiling generated C into a shared library.
- GREEN: MSVC builds both scope test targets; the contract passes with 1 test and 0 failures, and the smoke target reports its Unix-only runtime path ignored.
- Source scan: `backend_aot_c_function_body.c`, `backend_aot_c_lowering_scope.c`, and generated scope smoke C contain no `ZrLibrary_AotRuntime_MarkToBeClosed` or `ZrLibrary_AotRuntime_CloseScope` calls.

## Contract

`MARK_TO_BE_CLOSED` resolves `frame.slotBase + slot` and calls `ZrCore_Closure_ToBeClosedValueClosureNew`. `CLOSE_SCOPE` preserves stack top with core stack offset helpers, closes stack values with `ZrCore_Closure_CloseStackValue`, and drains registered close values with `ZrCore_Closure_CloseRegisteredValues`.
