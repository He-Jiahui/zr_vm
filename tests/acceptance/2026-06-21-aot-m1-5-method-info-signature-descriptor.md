# AOT M1.5 07-S5 MethodInfo Signature Descriptor

Date: 2026-06-22 04:31:28 +08:00

## Scope

This slice replaces the old null `SZrAotMethodInfo.signature` placeholder with a generated per-function signature descriptor.

Covered:

- `zr_aot_abi.h` now defines `SZrAotSignatureType` and `SZrAotSignature`.
- The AOT C emitter writes `zr_aot_signature_N_types` and `zr_aot_signature_N` for every generated function.
- Signature type rows are derived from `SZrFunctionTypedTypeRef` return and parameter metadata where available; missing type metadata keeps an unknown zero row while preserving parameter arity.
- `SZrAotMethodInfo.signature` points at `&zr_aot_signature_N` instead of `ZR_NULL`.
- The value typed-call shared-library smoke locks a one-parameter callee descriptor with non-null return and parameter type pointers.

Out of scope:

- Replacing `ZrLibrary_AotRuntime_CallStaticDirect()` with typed-to-typed direct C calls.
- Replacing inline-struct typed call/return helper boundaries.
- `in` / `out` writeback templates.
- Deopt/dynamic bridge boxing templates.
- Marking 07-S5, 07, or M1.5 complete.

## RED

`zr_vm_aot_c_frame_setup_contracts_test` first failed because the public AOT ABI still only had a signature forward declaration and the generated MethodInfo source still emitted `.signature = ZR_NULL`.

## GREEN

Focused WSL GCC validation:

- `zr_vm_aot_c_frame_setup_contracts_test`: 1/0.
- `zr_vm_aot_c_typed_scalar_test`: 1/0.
- `zr_vm_aot_c_call_shared_library_smoke_test`: 5/0.

Broader WSL GCC focused group:

- call contracts: 4/0.
- call shared-library smoke: 5/0.
- shared-library smoke: 8/0.
- value-type shared-library smoke: 1/0.
- power contracts/smoke: 2/0 + 1/0.
- source contracts: 19/0.
- generic numeric contracts/smoke: 1/0 + 1/0.
- global smoke: 9/0.
- logical contracts/smoke: 4/0 + 4/0.
- typed scalar: 1/0.
- return contracts: 1/0.
- frame setup contracts: 1/0.

## Notes

This is the metadata source for later typed-to-typed native signature routing. It intentionally leaves actual direct typed C call/return lowering for a follow-up 07-S5 slice.
