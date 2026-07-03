# AOT 10-S4V / 11-S4AJ / 12-S5 Support Acceptance: FieldInfo Signature Base Type-Node Object

- Time: 2026-07-01 01:41:50 +08:00
- Status: completed for the base type-node object carrier slice.
- Scope: runtime reflection now exposes `fieldTypeSignatureNodeObject.baseTypeNodeObject` for FieldInfo signature nodes
  that carry a `baseTypeBlobOffset`, starting with a focused `GENERIC_INST(TYPE_DEF(object, 17), int64)` field signature.

## Completed Items

- `ZrCore_Reflection_BuildFieldInfoTokenObject()` now passes the validated field signature blob into the signature-node
  object builder.
- `fieldTypeSignatureNodeObject` now recursively reads the same blob at `baseTypeBlobOffset` and emits a nested
  `baseTypeNodeObject` with `kind == "signatureTypeNode"` plus structural node/blob/payload summary fields.
- `tests/module/test_reflection_token_resolve.c` now covers a generic field signature and asserts both the top-level
  generic type-node object and the nested base `TYPE_DEF` node object.

## RED / GREEN

- RED: WSL GCC focused `zr_vm_reflection_token_resolve_test` failed 1/11 after adding the generic base-node object
  assertion because `baseTypeNodeObject` was missing.
- GREEN: the same test passed 11/0 after wiring the signature blob into the object builder and adding base-node object
  construction.

## Verification

- WSL GCC: `reflection_token_resolve` 11/0, `metadata_runtime_query` 24/0,
  `metadata_runtime_typespec_layout` 17/0, focused CTest 3/3.
- WSL Clang: same focused binaries and CTest passed; existing `reflection.c` `callerName` unused warning remains.
- Windows MSVC Debug: same focused binaries and CTest passed.

## Remaining Work

- Generic argument child object list, semantic token/layout binding for generic base/arguments, field read/write
  marshaling, complete FieldInfo method surface, cross-module provider loading, `@dynamically_accessed` dataflow,
  DESCRIPTION promotion, trim analyzer policy, and complete metadata sweep remain open.
