# AOT M1.5 07-S5 END_TRY Boundary Helper

Timestamp: 2026-06-21 16:30:20 +08:00

Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; 08-12 not started.

Plan alignment: `docs/plans/aot/07-codegen-register-model-and-environment-isolation.md`
section 6 requires VM/native marshaling and dynamic/control edges to stay behind fixed
boundary templates instead of expanding interpreter state in generated C. This slice
centralizes the generated END_TRY boundary in `ZrLibrary_AotRuntime_EndTry()`.

## Scope

- AOT C lowering for `END_TRY`.
- Runtime helper success-path frame/call-info synchronization.
- Source-contract and generated control shared-library smoke coverage under
  `tests/parser`.

## Baseline

- `backend_aot_write_c_end_try()` expanded `SZrCallInfo` recovery, handler lookup,
  finally-phase mutation, handler pop, generated AOT END_TRY error text, and
  frame/call-info synchronization directly into generated C.
- `ZrLibrary_AotRuntime_EndTry()` already existed, but generated C did not use it and the
  helper did not own the full success-path frame synchronization that the old generated
  block performed.

## RED

- `zr_vm_aot_c_control_contracts_test` first failed 1/1 after the contract required
  `ZrLibrary_AotRuntime_EndTry(state, &frame, %u)` and rejected the old generated
  handler-state/finally-phase template.

## GREEN

- `backend_aot_write_c_end_try()` now emits only the existing `zr_aot_end_try_direct`
  marker and one `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_EndTry(state, &frame, handlerIndex))`
  call.
- `ZrLibrary_AotRuntime_EndTry()` now owns the success-path `frame->callInfo`,
  `frame->slotBase`, `state->callInfoList`, and `state->stackTop.valuePointer`
  synchronization that previously lived in generated C.
- Control generated-C smoke now requires the END_TRY helper call and rejects the old
  generated handler-state/finally-phase template while leaving THROW, CATCH, and
  END_FINALLY for later slices.

## Verification

- `zr_vm_aot_c_control_contracts_test` passes 1/0.
- `zr_vm_aot_c_source_contracts_test` passes 19/0.
- `zr_vm_aot_c_control_shared_library_smoke_test` passes 1/0.
- Broader WSL GCC focused group passes: source contracts 19/0, return contracts 1/0,
  frame setup contracts 1/0, typed scalar 1/0, value SemIR contracts 4/0, call contracts
  4/0, call shared-library smoke 3/0, control contracts 1/0, control shared-library
  smoke 1/0, global contracts 6/0, and global shared-library smoke 7/0.
- Generated control smoke C contains `ZrLibrary_AotRuntime_EndTry(state, ...)` and no
  generated `handlerState->phase = ZR_VM_EXCEPTION_HANDLER_PHASE_FINALLY` END_TRY block.

## Acceptance Decision

Accepted for this 07-S5 sub-slice. END_TRY handling now uses the shared AOT runtime
helper and no longer expands handler-state mutation and frame synchronization in
generated C.

Remaining work: THROW, CATCH, END_FINALLY, pending control, typed-to-typed native
signature routing, in/out writeback, deopt/dynamic bridges, and other boundary templates
remain later 07-S5 work; stages 08-12 are not started.
