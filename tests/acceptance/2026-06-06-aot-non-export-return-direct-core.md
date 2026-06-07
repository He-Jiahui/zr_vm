# AOT Non-Export Return Direct Core

## Scope

Generated AOT C lowers non-export `FUNCTION_RETURN` directly instead of calling the instruction-shaped AOT runtime return helper. A later root/export return slice moved export materialization to `ZrLibrary_AotRuntime_PublishModuleExports` and then uses the same direct return block, so the checked C backend no longer emits `ZrLibrary_AotRuntime_Return(state, &frame, ...)`.

## Evidence

- RED: GCC `zr_vm_aot_c_source_contracts_test` first failed after the direct-return contract required the generated `zr_vm_core/execution_control.h` include and direct return core-call markers.
- GREEN: GCC `zr_vm_aot_c_source_contracts_test` passes with 17 tests and 0 failures.
- GREEN: GCC `zr_vm_aot_c_shared_library_smoke_test` passes with 6 tests and 0 failures, compiling generated C that contains the expanded direct return body.
- GREEN: Clang `zr_vm_aot_c_source_contracts_test` passes with 17 tests and 0 failures.
- GREEN: Clang `zr_vm_aot_c_shared_library_smoke_test` passes with 6 tests and 0 failures.
- GREEN: MSVC builds both focused targets; `zr_vm_aot_c_source_contracts_test` passes with 17 tests and 0 failures, and `zr_vm_aot_c_shared_library_smoke_test` reports its 6 Unix-only runtime checks ignored.
- Source scan: the C backend no longer has a `publishExports || exceptionHandlerCount` return-helper branch and no longer emits `ZrLibrary_AotRuntime_Return(..., ZR_FALSE)` for generated C returns; the later export-return slice removes the remaining `ZR_TRUE` generated return-helper path.
- Generated-C scan: GCC generated shared-library smoke C contains `zr_aot_direct_return`, `execution_discard_exception_handlers_for_callinfo`, `ZrCore_Function_ApplyReturnEscape`, and `ZrCore_Closure_CloseClosure`, and contains no `ZrLibrary_AotRuntime_Return`.

## Contract

Non-export generated C return validates the frame source slot, discards active exception handlers for the current call frame, applies return escape metadata, closes captured stack values, copies the result to the caller frame base, and restores `state->stackTop` to the caller result slot. Export publication remains an explicit runtime boundary.
