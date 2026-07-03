# AOT 10-S3B / 11-S5 MethodSpec token resolve carrier

Timestamp: 2026-06-30 00:17:04 +08:00

## Scope

- Completed a focused 10-S3 token-driven reflection sub-slice for MethodSpec tokens.
- `ZrCore_Reflection_ResolveToken()` now accepts MethodSpec `SIGNATURE` tokens when the attached metadata describes `GENERIC_INST(MEMBER_REF methodToken, args...)`.
- The public `SZrReflectionResolvedToken` carrier now exposes:
  - MethodSpec token as `token`
  - MethodSpec signature token record as `record`
  - underlying method token/record as `methodToken` / `methodRecord`
  - MethodSpec signature hash
  - generic argument count and argument-list blob offset
- The lower metadata view `SZrMetadataRuntimeMethodSpecSignatureView` now carries `methodSpecRecord`, avoiding a second reflection-layer lookup for the MethodSpec signature record.

## RED

- `tests/module/test_reflection_token_resolve.c` first asserted `ResolveToken(runtime, TEST_METHOD_SPEC_TOKEN, ...)` and referenced missing `SZrReflectionResolvedToken.methodToken` / `methodRecord`.
- `tests/module/test_metadata_runtime_query.c` then asserted `view.methodSpecRecord` from `ZrCore_MetadataRuntime_ReadMethodSpecSignatureView()`.
- WSL gcc compile failed as expected because those carrier fields did not exist yet.

## GREEN

- Added `methodSpecRecord` to `SZrMetadataRuntimeMethodSpecSignatureView` and populated it in `ZrCore_MetadataRuntime_ReadMethodSpecSignatureView()`.
- Added `methodToken` / `methodRecord` to `SZrReflectionResolvedToken`.
- `reflection_token_resolve.c` now:
  - fills method token/record for ordinary MethodDef/MethodRef results
  - resolves MethodSpec `SIGNATURE` tokens through `ZrCore_MetadataRuntime_ReadMethodSpecSignatureView()`
  - returns MethodSpec as `ZR_REFLECTION_RESOLVED_TOKEN_METHOD` with MethodSpec record plus underlying method identity and generic argument metadata
- Existing ordinary signature tokens still fail unless they pass the MethodSpec signature-view validation path.

## Validation

- WSL gcc:
  - `zr_vm_metadata_runtime_query_test`: 24/0
  - `zr_vm_reflection_token_resolve_test`: 4/0
  - `zr_vm_metadata_runtime_typespec_layout_test`: 14/0
- WSL clang:
  - same three binaries passed 24/0, 4/0, 14/0
  - existing computed-goto/unused warnings remain outside this slice
- Windows MSVC Debug:
  - same three binaries passed 24/0, 4/0, 14/0
  - existing execution/object unreachable/unused warnings remain outside this slice

## Not Closed

- No public generic method reflection object materialization.
- No MethodSpec runtime instance binding.
- No runtime generic layout construction or generic dictionary materialization.
- No cross-module token publication/rewrite.
- No full trim analyzer or annotation-driven retention closure.
