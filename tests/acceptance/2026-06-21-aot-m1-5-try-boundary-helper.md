# AOT M1.5 07-S5 TRY Boundary Helper

Timestamp: 2026-06-21 16:20:39 +08:00

Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; 08-12 not started.

Plan alignment: `docs/plans/aot/07-codegen-register-model-and-environment-isolation.md`
section 6 requires VM/native marshaling and dynamic/control edges to stay behind fixed
boundary templates instead of expanding interpreter state in generated C. This slice
centralizes the generated TRY boundary in `ZrLibrary_AotRuntime_Try()`.

## Scope

- AOT C lowering for `TRY`.
- Runtime helper success-path frame/call-info synchronization.
- Source-contract and generated control shared-library smoke coverage under
  `tests/parser`.

## Baseline

- `backend_aot_write_c_try()` expanded `SZrCallInfo` recovery, handler-index checks,
  `execution_push_exception_handler()`, generated AOT TRY error text, and frame/call-info
  synchronization directly into generated C.
- `ZrLibrary_AotRuntime_Try()` already existed, but generated C did not use it and the
  helper did not own the full success-path frame synchronization that the old generated
  block performed.
- The repository checkout is heavily dirty with unrelated AOT, parser, runtime, and docs
  work; this slice only accepts the focused WSL evidence below.

## RED

- `zr_vm_aot_c_control_contracts_test` first failed 1/1 after the contract required
  `ZrLibrary_AotRuntime_Try(state, &frame, %u)` and rejected the old
  `execution_push_exception_handler(state, zr_aot_call_info, ...)` generated template.

## GREEN

- `backend_aot_write_c_try()` now emits only the existing `zr_aot_try_direct` marker and
  one `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_Try(state, &frame, handlerIndex))` call.
- `ZrLibrary_AotRuntime_Try()` now owns the success-path `frame->callInfo`,
  `frame->slotBase`, `state->callInfoList`, and `state->stackTop.valuePointer`
  synchronization that previously lived in generated C.
- Control generated-C smoke now requires the TRY helper call and rejects the old
  `execution_push_exception_handler(...)` template while leaving END_TRY, THROW, CATCH,
  and END_FINALLY for later slices.

## Verification

- `zr_vm_aot_c_control_contracts_test` passes 1/0.
- `zr_vm_aot_c_source_contracts_test` passes 19/0.
- `zr_vm_aot_c_control_shared_library_smoke_test` passes 1/0.
- Broader WSL GCC focused group passes: source contracts 19/0, return contracts 1/0,
  frame setup contracts 1/0, typed scalar 1/0, value SemIR contracts 4/0, call contracts
  4/0, call shared-library smoke 3/0, control contracts 1/0, control shared-library
  smoke 1/0, global contracts 6/0, and global shared-library smoke 7/0.
- `ctest -R 'aot_c_(typed_scalar|call_shared_library|control_shared_library|global_(contracts|shared_library_smoke))'`
  still matches only the registered `aot_c_typed_scalar` test in this build and passes
  1/1.
- Generated control smoke C contains `ZrLibrary_AotRuntime_Try(state, ...)` and no
  generated `execution_push_exception_handler(state, zr_aot_call_info, ...)` TRY block.

## Acceptance Decision

Accepted for this 07-S5 sub-slice. TRY setup now uses the shared AOT runtime helper and
no longer expands handler push and frame synchronization in generated C.

Remaining work: END_TRY, THROW, CATCH, END_FINALLY, pending control, typed-to-typed
native signature routing, in/out writeback, deopt/dynamic bridges, and other boundary
templates remain later 07-S5 work; stages 08-12 are not started.
