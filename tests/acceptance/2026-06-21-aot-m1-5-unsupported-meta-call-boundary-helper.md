# AOT M1.5 07-S5 Unsupported Meta Call Boundary Helper

Timestamp: 2026-06-21 15:06:55 +08:00

Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; 08-12 not started.

Plan alignment: `docs/plans/aot/07-codegen-register-model-and-environment-isolation.md`
section 6 requires VM/native marshaling and dynamic/deopt edges to live behind fixed
boundary templates instead of expanding interpreter state in generated C. This slice
centralizes the currently unsupported meta-call boundary in
`ZrLibrary_AotRuntime_UnsupportedMetaCall()`.

RED:

- `zr_vm_aot_c_call_contracts_test` first failed in the meta-call case after the contract
  required `ZrLibrary_AotRuntime_UnsupportedMetaCall(struct SZrState *state,` and forbade
  the old generated destination/receiver/argument locals, `SZrTypeValue *` slot access,
  and generated-C `ZrCore_Debug_RunError(state, ...)` template.
- During broader validation, `zr_vm_aot_c_typed_scalar_test` exposed a pre-existing
  stack-copy proof gap: an f64 local copy `22 <- 40` fell back to frame-based
  `ZrCore_Stack_GetValue`, forcing descriptor/frame setup back into focused scalar C.

GREEN:

- `ZrLibrary_AotRuntime_UnsupportedMetaCall()` now validates the current frame slots and
  records the unsupported meta-call failure from runtime code.
- `backend_aot_write_c_unsupported_meta_call()` now emits only
  `zr_aot_unsupported_meta_call` plus one helper guard.
- `backend_aot_c_scalar_locals_record_exec_instruction_write()` now lets stack-copy
  write proofs inherit the source slot's declared scalar kind when the source kind was
  established before the current local range. This restores the focused f64 stack-copy
  local-only path needed by the 07-S4 frame-free scalar contract.

Verification:

- WSL GCC focused group passed: source contracts 19/0, return contracts 1/0, frame setup
  contracts 1/0, typed scalar 1/0, value SemIR contracts 4/0, call contracts 4/0, call
  shared-library smoke 3/0.
- `ctest -R 'aot_c_typed_scalar|aot_c_call_shared_library'` matched registered
  `aot_c_typed_scalar` and passed 1/1; the call shared-library smoke remains a direct
  binary in this build and was executed directly.
- Generated meta-call boundary C contains three `ZrLibrary_AotRuntime_UnsupportedMetaCall`
  guards and no generated `const TZrUInt32 zr_aot_argument_count`,
  `SZrTypeValue *zr_aot_receiver`, `SZrTypeValue *zr_aot_destination`, or
  `ZrCore_Debug_RunError(state, "unsupported AOT meta call")` template.
- Generated typed-scalar C has no `zr_aot_generated_frame_setup` and keeps
  `zr_aot_scalar_stack_copy_f64 dstSlot=22 srcSlot=40` as a local-only copy.

Remaining:

- This is still an unsupported meta-call boundary, not final meta-call execution.
  Typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and
  remaining unsupported boundary templates remain later 07-S5 work.
