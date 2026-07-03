# AOT 10-S4Z3 FieldInfo Signature Node Type Literal Carrier

## Scope

- Plan slices: `10-S4Z3 / 11-S4AO / 12-S5 support`.
- Goal: let recursive `FieldInfo.fieldTypeSignatureNodeObject` nodes with semantic `typeName` expose nested public `type` type literal objects.
- Non-goals: cross-module provider loading, provider version compatibility, full recursive/signature-derived field type materialization, field value read/write, complete FieldInfo methods, trim analyzer policy, and metadata sweep.

## RED

- Test: `tests/module/test_reflection_token_resolve.c`.
- Fixture: `FIELD_SIG(GENERIC_INST(TYPE_DEF(object, 17), int64, TYPE_DEF(object, 17), TYPE_REF(object, 23)))`.
- New assertions: `baseTypeNodeObject.type`, primitive `childNodeObjects[0].type`, direct TypeDef `childNodeObjects[1].type`, and direct TypeRef `childNodeObjects[2].type` are reflection objects with `kind == "type"` and `name/qualifiedName == "int"`.
- Result: WSL GCC `zr_vm_reflection_token_resolve_test` failed 1/11 with `Expected Non-NULL` while the nested `type` field was missing.

## GREEN

- `reflection_build_signature_type_node_object_internal()` now builds a `type` literal object whenever the effective semantic type name is nonempty.
- The implementation reuses `reflection_build_type_literal_object_internal()` and the existing public `type` field convention from FieldInfo/ParameterInfo objects.
- The slice does not change metadata ABI, token record schema, layout resolver contracts, or FieldInfo read/write behavior.

## Verification

- WSL GCC: `zr_vm_reflection_token_resolve_test` 11/0, `zr_vm_metadata_runtime_query_test` 24/0, `zr_vm_metadata_runtime_typespec_layout_test` 17/0, focused CTest 3/3.
- WSL Clang: same 11/0, 24/0, 17/0, focused CTest 3/3; existing `reflection.c` `callerName` unused warning remains.
- Windows MSVC Debug: same 11/0, 24/0, 17/0, focused CTest 3/3.

## Status

Completed at 2026-07-01 03:17:49 +08:00.

Recorded in:

- `docs/plans/aot/10-reflection.md`
- `docs/plans/aot/11-metadata.md`
- `docs/plans/aot/12-code-stripping.md`
- `docs/plans/aot/index.md`
- `docs/module-system/typed-module-metadata.md`
- `.codex/sessions/20260620-2321-aot-07-12-codegen.md`
