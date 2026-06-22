# AOT M1.5 07-S5 Call Stack Value Boundary Helper

Timestamp: 2026-06-21 14:41:47 +08:00

Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; 08-12 not started.

Plan alignment: `docs/plans/aot/07-codegen-register-model-and-environment-isolation.md`
section 6 requires VM/native marshaling to be fixed boundary templates. This slice
centralizes the generic/dynamic stack-value function-call boundary in
`ZrLibrary_AotRuntime_CallStackValue()`.

RED:

- `zr_vm_aot_c_call_contracts_test` first failed in the dynamic and generic call
  cases after the contracts required `ZrLibrary_AotRuntime_CallStackValue(state,`
  and forbade the old generated stack-anchor / `CallAndRestoreAnchor` result-copy
  template.

GREEN:

- `ZrLibrary_AotRuntime_CallStackValue()` owns the current stack-value call boundary:
  call-base and destination anchors, `ZrCore_Function_CallAndRestoreAnchor`, result
  slot copy, and caller frame refresh.
- `backend_aot_write_c_core_function_call()` now emits only the existing
  `zr_aot_direct_function_call` or `zr_aot_direct_dynamic_function_call` marker plus
  one helper guard.
- The call shared-library smoke still executes the `apply(addFour, 3)` fixture through
  the AOT C shared-library path.

Verification:

- WSL GCC focused group passed: source contracts 19/0, return contracts 1/0, frame setup
  contracts 1/0, typed scalar 1/0, value SemIR contracts 4/0, call contracts 4/0, call
  shared-library smoke 3/0.
- `ctest -R 'aot_c_typed_scalar|aot_c_call_shared_library'` matched registered
  `aot_c_typed_scalar` and passed 1/1; the call shared-library smoke remains a direct
  binary in this build and was executed directly.
- Generated call-smoke C contains `ZrLibrary_AotRuntime_CallStackValue(state,` and no
  generated `ZrCore_Function_CallAndRestoreAnchor(state, &zr_aot_call_anchor, 1)`,
  `SZrFunctionStackAnchor zr_aot_call_anchor`, or
  `ZrCore_Function_StackAnchorRestore(state, &zr_aot_destination_anchor)`.

Remaining:

- This remains a VM-boundary template consolidation step. Typed-to-typed native
  signature routing, in/out writeback, deopt/dynamic bridges, and remaining boundary
  templates remain later 07-S5 work.
