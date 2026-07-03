# AOT 10-S3E / 11-S2D MethodSpec Underlying Method Binding Carrier

## Scope

- Completed a public reflection carrier slice for MethodSpec tokens.
- `ZrCore_Reflection_ResolveToken()` keeps the MethodSpec signature/generic identity while reusing the underlying
  MethodDef token to read the 11-S2D AOT method binding view.
- This does not implement public generic method reflection objects, MethodSpec-specific generated function slots,
  cross-module token rewrite, or `Method.Invoke` parameter/return marshaling.

## RED

- `tests/module/test_reflection_token_resolve.c` was extended so the MethodSpec fixture registers the underlying
  MethodDef in `methodTokens[]`, `methodInfos[]`, and `functionPointers[]`.
- The first WSL gcc run built successfully but failed:
  - `test_reflection_resolves_method_spec_generic_arguments`
  - expected `methodFunctionIndex == 1`
  - actual `methodFunctionIndex == 0`

## GREEN

- `zr_vm_core/src/zr_vm_core/reflection_token_resolve.c` now calls the existing method binding helper from the
  MethodSpec resolution path using `view.methodToken`.
- The resolved MethodSpec carrier now includes:
  - MethodSpec token/record/signature hash and generic argument count
  - underlying method token/record
  - underlying AOT `methodFunctionIndex`, `methodInfo`, entry thunk, and invoker when present

## Verification

- WSL gcc:
  - `zr_vm_reflection_token_resolve_test` 5/0
  - `zr_vm_metadata_runtime_method_binding_test` 2/0
  - `zr_vm_metadata_runtime_query_test` 24/0
- WSL clang:
  - `zr_vm_reflection_token_resolve_test` 5/0
  - `zr_vm_metadata_runtime_method_binding_test` 2/0
  - `zr_vm_metadata_runtime_query_test` 24/0
- Windows MSVC Debug:
  - `zr_vm_reflection_token_resolve_test` 5/0
  - `zr_vm_metadata_runtime_method_binding_test` 2/0
  - `zr_vm_metadata_runtime_query_test` 24/0

## Files

- `zr_vm_core/src/zr_vm_core/reflection_token_resolve.c`
- `tests/module/test_reflection_token_resolve.c`
- `docs/plans/aot/10-reflection.md`
- `docs/plans/aot/11-metadata.md`
- `docs/plans/aot/index.md`
- `docs/module-system/typed-module-metadata.md`
