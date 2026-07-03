# AOT 10-S4O / 11-S4AC FieldInfo Field Signature Type Object Carrier

Timestamp: 2026-06-30 21:02:11 +08:00

## Scope

- Completed a support slice for the minimum FieldDef-token `FieldInfo` object.
- `ZrCore_Reflection_BuildFieldInfoTokenObject()` now exposes a primitive field-signature type literal object as `fieldTypeSignatureType`.
- The existing layout-derived `typeName` / `type` object remain unchanged; this slice only adds a signature-derived object carrier.

## RED

- Added focused assertions to `tests/module/test_reflection_token_resolve.c`.
- The fixture keeps the layout-derived field `type` object as `int`, while the validated `FIELD_SIG` node is `PRIMITIVE(BOOL)`.
- Before implementation, WSL GCC built `zr_vm_reflection_token_resolve_test`, then failed `test_reflection_builds_field_info_object_from_fielddef_token` because `fieldTypeSignatureType` was missing.

## GREEN

- `zr_vm_core/src/zr_vm_core/reflection.c` now reuses the primitive signature type name to build a type-literal reflection object.
- Primitive nodes write `fieldTypeSignatureType` as a reflection object with:
  - `kind = "type"`
  - `name = "bool"`
  - `qualifiedName = "bool"`
- Missing, invalid, or non-primitive signature nodes keep `fieldTypeSignatureType` as null.

## Verification

- WSL GCC: `reflection_token_resolve` 8/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0, focused CTest 3/3.
- WSL Clang: same focused suite passed 8/0, 24/0, 17/0, focused CTest 3/3.
- Windows MSVC Debug: same focused suite passed 8/0, 24/0, 17/0, focused CTest 3/3.

## Notes

- This is still a primitive-only type-literal carrier.
- Complete signature-derived field type binding, TypeDef/TypeRef signature token binding, recursive type-node reflection objects, field value read/write, complete `FieldInfo` methods, module reflection links, cross-module metadata rules, dataflow analysis, DESCRIPTION promotion, and full metadata sweep remain later work.
