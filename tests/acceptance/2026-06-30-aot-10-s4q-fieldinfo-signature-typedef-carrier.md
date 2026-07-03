# AOT 10-S4Q / 11-S4AE FieldInfo Signature TypeDef Carrier

Timestamp: 2026-06-30 21:45:03 +08:00

## Scope

- Closed the narrow 10-S4Q / 11-S4AE / 12-S5 support slice for direct `TYPE_DEF` field signature nodes.
- `FieldInfo` now exposes a signature-derived type token/layout carrier when the validated `FIELD_SIG` field type-node matches an attached local `TYPE_DEF` signature record.
- This does not implement field value read/write, complete `FieldInfo` methods, recursive type-node reflection objects, full signature-derived semantic field type binding, or cross-module `TypeRef`/`FieldRef`.

## RED

- Added a focused `tests/module/test_reflection_token_resolve.c` case with `FIELD_SIG(TYPE_DEF(object, 17))`.
- The fixture required:
  - `fieldTypeSignatureTypeToken == TEST_FIELD_TYPE_DEF_TOKEN`
  - `fieldTypeSignatureTypeLayoutId == 42`
  - `fieldTypeSignatureTypeSize == 16`
- WSL GCC built `zr_vm_reflection_token_resolve_test`, then failed because `fieldTypeSignatureTypeToken` was not present yet.

## GREEN

- Added a reflection-local signature type-record matcher that compares the direct signature type-node against attached token records through existing metadata runtime signature blob readers.
- `ZrCore_Reflection_BuildFieldInfoTokenObject()` now resolves a matching direct `TYPE_DEF` node through `ZrCore_MetadataRuntime_ResolveTypeTokenLayout()` and writes:
  - `fieldTypeSignatureTypeToken`
  - `fieldTypeSignatureTypeLayoutId`
  - `fieldTypeSignatureTypeSize`
- Missing, invalid, primitive, or unmatched signature nodes keep these carrier fields at `0`.

## Verification

- WSL GCC:
  - `zr_vm_reflection_token_resolve_test`: 9 tests, 0 failures
  - `zr_vm_metadata_runtime_query_test`: 24 tests, 0 failures
  - `zr_vm_metadata_runtime_typespec_layout_test`: 17 tests, 0 failures
  - focused CTest: 3/3
- WSL Clang:
  - same focused binaries and CTest passed
  - existing `reflection.c` `callerName` unused warning remains
- Windows MSVC Debug:
  - same focused binaries and CTest passed

## Notes

- This is a public carrier slice only. It proves the minimum FieldInfo object can surface direct local signature type identity without changing zrp rows, code-registration ABI, or trim roots.
- `TYPE_REF`/cross-module provider resolution, recursive type-node objects, field type consistency checks, field read/write, `@dynamically_accessed` dataflow, DESCRIPTION promotion, and full metadata sweep remain later work.
