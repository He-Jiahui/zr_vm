# AOT M1.5 07-S5 Static Direct Call Boundary Helper

Timestamp: 2026-06-21 14:25:45 +08:00

Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; 08-12 not started.

Plan alignment: `docs/plans/aot/07-codegen-register-model-and-environment-isolation.md`
section 6 requires VM/native marshaling to live in fixed boundary templates instead of
being expanded through generated function bodies. This slice centralizes the static
direct-call VM boundary in `ZrLibrary_AotRuntime_CallStaticDirect()`.

RED:

- `zr_vm_aot_c_call_contracts_test` first failed after requiring generated static
  direct-call lowering to use `ZrLibrary_AotRuntime_CallStaticDirect(state,` and
  forbidding the old inline `SZrCallInfo` / prepared-call template in
  `backend_aot_c_lowering_calls.c`.

GREEN:

- `ZrLibrary_AotRuntime_CallStaticDirect()` owns the current static direct-call
  VM frame preparation, VALUE argument source materialization, callee thunk invocation,
  `PostCall`, and caller frame restoration.
- `backend_aot_write_c_static_direct_function_call()` now emits only a
  `zr_aot_direct_static_function_call` marker plus one helper guard carrying
  destination slot, function slot, argument count, callee index, and callee thunk.
- The existing call shared-library smoke still executes `apply(addFour, 3)` through
  the AOT C shared-library path and returns the expected result.

Verification:

- WSL GCC focused group passed: source contracts 19/0, return contracts 1/0, frame setup
  contracts 1/0, typed scalar 1/0, value SemIR contracts 4/0, call contracts 4/0, call
  shared-library smoke 3/0.
- `ctest -R 'aot_c_typed_scalar|aot_c_call_shared_library'` matched registered
  `aot_c_typed_scalar` and passed 1/1; the call shared-library smoke remains a direct
  binary in this build and was executed directly.
- Generated static direct-call C contains `ZrLibrary_AotRuntime_CallStaticDirect(state,`
  and no generated `ZrCore_Function_PreCallPreparedResolvedVmFunctionWithArgumentSource`,
  `zr_aot_materialize_argument_source_slot`, or
  `ZrCore_Function_PostCall(state, zr_aot_call_info, 1)` in that call block.

Remaining:

- This is still a VM-boundary template consolidation step. The final typed-to-typed
  native C signature ABI, in/out writeback, deopt/dynamic bridges, and remaining boundary
  templates remain later 07-S5 work.
