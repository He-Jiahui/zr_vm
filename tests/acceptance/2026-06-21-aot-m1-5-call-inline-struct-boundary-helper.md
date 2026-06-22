# AOT M1.5 07-S5 Call Inline Struct Boundary Helper

Timestamp: 2026-06-21 14:12:31 +08:00

Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; 08-12 not started.

Plan alignment: `docs/plans/aot/07-codegen-register-model-and-environment-isolation.md`
section 6 requires VM/native marshaling to be fixed boundary templates. This slice
centralizes the value SemIR typed inline-struct call boundary in
`ZrLibrary_AotRuntime_CallInlineStruct()`.

RED:

- `zr_vm_aot_c_value_semir_contracts_test` first failed after the contract required
  `ZrLibrary_AotRuntime_CallInlineStruct(state,` and forbade the old generated
  `SZrCallInfo` / callable `SZrTypeValue` / prepared-call window template.
- `zr_vm_aot_c_source_contracts_test` then exposed the broader source contract still
  expecting old generated typed-call boundary internals.

GREEN:

- `ZrLibrary_AotRuntime_CallInlineStruct()` owns the current VM call-frame preparation,
  VALUE argument source materialization, callee thunk invocation, `PostCall`, and caller
  frame restoration for inline-struct typed-call results.
- `backend_aot_try_write_c_value_semir_call_typed_exec()` now emits a single helper
  guard with destination slot, callee slot, argument count, callee function index,
  destination layout id/offset/size, and callee thunk.
- Source and generated-C contracts now require the helper and reject the old inline
  prepared-call template in this path.

Verification:

- WSL GCC focused group passed: source contracts 19/0, return contracts 1/0, frame setup
  contracts 1/0, typed scalar 1/0, value SemIR contracts 4/0, call contracts 4/0, call
  shared-library smoke 3/0.
- `ctest -R 'aot_c_typed_scalar|aot_c_call_shared_library'` matched registered
  `aot_c_typed_scalar` and passed 1/1; the call shared-library smoke remains a direct
  binary in this build and was executed directly.
- Generated value-typed smoke C contains `ZrLibrary_AotRuntime_CallInlineStruct(state,`
  and no `ZrCore_Function_PreCallPreparedResolvedVmFunctionWithArgumentSource`,
  `zr_aot_materialize_argument_source_slot`,
  `SZrFunction *zr_aot_metadata_function;`, or
  `ZrCore_Function_PostCall(state, zr_aot_call_info, 1)` in the typed-call path.

Remaining:

- This is still a boundary-template consolidation step, not the final typed-to-typed C
  signature ABI. Typed-to-typed direct argument/return routing, in/out writeback, deopt
  bridges, and remaining dynamic boundary templates remain later 07-S5 work.
