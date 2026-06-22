# AOT M1.5 07-S5 GET_SUB_FUNCTION Native Closure Boundary Helper

Time: 2026-06-21 23:39:10 +08:00
Status: Accepted slice, 07-S5 still partial.

## Scope

- Move zero-capture `GET_SUB_FUNCTION` native-closure construction out of generated C and into a runtime boundary helper.
- Preserve the existing unsupported materialization path for captured, unresolved, or missing child functions.
- Keep generated C responsible only for the marker, guard, slot/index operands, and generated thunk pointer.

## RED

- `zr_vm_aot_c_constant_contracts_test` failed 1/4 after the contract required `zr_aot_value_get_sub_function_native_closure_boundary`.
- The old generated lowering still emitted the direct native-closure construction marker/template.

## Implementation

- Added `ZrLibrary_AotRuntime_GetSubFunctionNativeClosure(...)` to the public AOT runtime surface.
- Runtime helper validates the frame, owner function, destination slot, child index, and generated thunk.
- Runtime helper releases the destination value, allocates `SZrClosureNative`, binds `nativeThunk` and child shim metadata, and initializes the destination closure value.
- C lowering now emits `ZR_AOT_C_GUARD(ZrLibrary_AotRuntime_GetSubFunctionNativeClosure(state, &frame, ..., zr_aot_fn_N));`.
- Added a generated shared-library smoke that hand-builds a root function with one child and confirms the generated product uses the helper boundary.

## GREEN

- `zr_vm_aot_c_constant_contracts_test`: 4/0.
- `zr_vm_aot_c_source_contracts_test`: 19/0.
- `zr_vm_aot_c_shared_library_smoke_test`: 8/0.
- `zr_vm_aot_c_call_shared_library_smoke_test`: 3/0.
- `zr_vm_aot_c_global_shared_library_smoke_test`: 9/0.
- `zr_vm_aot_c_typed_scalar_test`: 1/0.
- `zr_vm_aot_c_return_contracts_test`: 1/0.
- `zr_vm_aot_c_frame_setup_contracts_test`: 1/0.
- Generated `aot_c_get_sub_function_native_closure_smoke.c` contains the helper marker/call and `zr_aot_fn_1`; old direct native-closure template needles are absent.
- `git diff --check` exits 0 with only existing LF/CRLF warnings.

## Decision

Accepted for this 07-S5 sub-slice. Remaining 07-S5 work still includes typed-to-typed native signature routing, in/out writeback, deopt/dynamic bridges, and any remaining boundary templates.
