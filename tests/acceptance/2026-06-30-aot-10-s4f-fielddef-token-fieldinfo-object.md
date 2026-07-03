# AOT 10-S4F / 11-S4T Minimum FieldDef Token FieldInfo Object

Time: 2026-06-30 18:38:42 +08:00

Status: complete for the minimum FieldDef token to public FieldInfo object sub-slice.

Completed:
- `reflection.h` now exposes `ZrCore_Reflection_BuildFieldInfoTokenObject(state, runtime, fieldToken)`.
- The implementation reuses `ZrCore_Reflection_ResolveToken()` for the FieldDef carrier and `ZrCore_MetadataRuntime_ReadTypeDefLayoutBindingView()` for a TypeDef-backed field type name.
- Field, owner, and field-type names are read from the attached zrp string pool through existing row string offsets, with conservative fallbacks when metadata is incomplete.
- The public reflection object is materialized through the existing member-info builder and carries token, owner/type token, layout id, offset, size, field type, and layout sub-object fields.
- `tests/module/test_reflection_token_resolve.c` now covers null inputs, wrong-token rejection, and the positive FieldDef token object shape.

RED/GREEN:
- RED: WSL GCC failed to link the focused reflection token resolver test because `ZrCore_Reflection_BuildFieldInfoTokenObject` was not implemented.
- GREEN: the focused reflection token resolver test passed 8/0 after the FieldInfo object API and implementation were added.

Validation:
- WSL GCC direct/focused: `reflection_token_resolve` 8/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- WSL GCC CTest: `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout` 3/3.
- WSL Clang direct/focused: `reflection_token_resolve` 8/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- WSL Clang CTest: same focused set 3/3.
- Windows MSVC Debug direct/focused: `reflection_token_resolve` 8/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- Windows MSVC Debug CTest: same focused set 3/3.
- `git diff --check` exited 0 with only LF/CRLF warnings for touched files.

Not claimed:
- Field value read/write marshaling.
- Owner/module object links, cache policy, or complete public `FieldInfo` behavior.
- Public generic reflection objects, generic method reflection objects, or `MakeGenericType`.
- Cross-module FieldRef/TypeRef rules, `@dynamically_accessed` dataflow, trim warnings, DESCRIPTION promotion, or complete metadata sweep.
