# AOT 10-S4P / 11-S4AD FieldInfo Module Reflection Link

Timestamp: 2026-06-30 21:20:26 +08:00

## Scope

- Closed the narrow 10-S4P / 11-S4AD / 12-S5 support slice for the minimum public FieldDef token `FieldInfo` object.
- `FieldInfo.module` now links to the attached runtime module reflection object when the metadata runtime carries a real module object.
- This does not implement field value read/write, complete `FieldInfo` methods, cross-module FieldRef/TypeRef, recursive type-node objects, or full signature-derived field type binding.

## RED

- Added focused assertions to `tests/module/test_reflection_token_resolve.c` requiring `FieldInfo.module` to be an object reflection carrier with:
  - `kind == "module"`
  - `name == "geometry"`
  - `qualifiedName == "geometry"`
- WSL GCC built `zr_vm_reflection_token_resolve_test`, then failed the FieldInfo test because `FieldInfo.module` was still null.

## GREEN

- `ZrCore_Reflection_BuildFieldInfoTokenObject()` now checks `runtime->module` is a real `ZR_OBJECT_INTERNAL_TYPE_MODULE`.
- The builder reuses the existing `reflection_build_module_reflection()` path and writes the result to `FieldInfo.module`; otherwise it leaves the field null.
- The focused fixture now creates a real module with `ZrCore_Module_Create()` and sets module info through `ZrCore_Module_SetInfo()`, so the assertion exercises the same module reflection/cache path used elsewhere.

## Verification

- WSL GCC: `reflection_token_resolve` 8/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0, focused CTest 3/3.
- WSL Clang: same focused executables and CTest passed; existing `reflection.c` `callerName` unused warning remains.
- Windows MSVC Debug: same focused executables and CTest passed.

## Notes

- This is an identity-link carrier only.
- It does not change zrp metadata rows, code-registration ABI, trim roots, or metadata sweep behavior.
