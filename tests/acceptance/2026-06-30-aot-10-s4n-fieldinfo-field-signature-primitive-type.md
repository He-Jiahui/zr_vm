# AOT 10-S4N / 11-S4AB FieldInfo Field Signature Primitive Type Carrier

Timestamp: 2026-06-30 20:46:10 +08:00

## Scope

- Completed a support slice for the minimum FieldDef-token `FieldInfo` object.
- `ZrCore_Reflection_BuildFieldInfoTokenObject()` now exposes primitive field-signature type semantics as separate public carriers:
  - `fieldTypeSignatureValueType`
  - `fieldTypeSignatureTypeName`
- The existing layout-derived `typeName` / `type` object remain unchanged; this slice does not claim full field type binding.

## RED

- Added focused assertions to `tests/module/test_reflection_token_resolve.c`.
- The fixture keeps the layout-derived field type name as `int`, while the validated `FIELD_SIG` node is `PRIMITIVE(BOOL)`.
- Before implementation, WSL GCC built `zr_vm_reflection_token_resolve_test`, then failed `test_reflection_builds_field_info_object_from_fielddef_token` because the new primitive signature carriers were missing.

## GREEN

- `zr_vm_core/src/zr_vm_core/reflection.c` now detects `ZR_METADATA_SIGNATURE_NODE_PRIMITIVE` after reading the field type-node view.
- Primitive nodes write:
  - `fieldTypeSignatureValueType = payload0`
  - `fieldTypeSignatureTypeName = reflection_builtin_type_name(payload0)`
- Missing, invalid, or non-primitive signature nodes keep `fieldTypeSignatureValueType = 0` and `fieldTypeSignatureTypeName = ""`.

## Verification

- WSL GCC: `reflection_token_resolve` 8/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0, focused CTest 3/3.
- WSL Clang: same focused suite passed 8/0, 24/0, 17/0, focused CTest 3/3.
- Windows MSVC Debug: same focused suite passed 8/0, 24/0, 17/0, focused CTest 3/3.

## Notes

- This is still a primitive-only carrier.
- Complete signature-derived field type binding, TypeDef/TypeRef signature token binding, recursive type-node reflection objects, field value read/write, complete `FieldInfo` methods, module reflection links, cross-module metadata rules, dataflow analysis, DESCRIPTION promotion, and full metadata sweep remain later work.
