# AOT 10-S4Z20 / 11-S4BF FieldInfo inline aggregate replacement/drop borrowed-source write coverage

Time: 2026-07-01 12:09:54 +08:00

Status: completed coverage-green support sub-slice. This proves replacement/drop behavior for a selected inline aggregate write path, not full recursive nested field access or managed `FieldInfo.SetValue`.

Completed:

- Added `test_reflection_writes_field_info_object_inline_struct_drops_replaced_owned_value_field()`.
- The fixture builds a FieldInfo object for a retained inline aggregate field with a `FIELD_COPY/FIELD_DROP` field type layout.
- The nested aggregate field is a `SZrTypeValue` marked `VALUE_SLOT | GC_VALUE | OWNERSHIP_VALUE`.
- The destination starts with a unique-owned old string; the source is a non-null native-pointer aggregate carrying a plain new string.
- `ZrCore_Reflection_WriteFieldInfoObjectValue()` writes through the existing S4Z19 `ZrCore_TypeLayout_CopyInline()` path.
- The test verifies the old string owner strong ref drops from 1 to 0, the destination slot now references the new string, and destination ownership metadata is normalized to none.

RED/GREEN:

- Coverage GREEN: the new test passed immediately because the existing S4Z19 write path already delegates nested value slots to `ZrCore_Value_Copy()`.
- No production code or metadata ABI changed in this slice.

Validation:

- WSL GCC direct: `reflection_token_resolve` 25/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- WSL Clang direct: `reflection_token_resolve` 25/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- Windows MSVC Debug direct: `reflection_token_resolve` 25/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- Focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout`: 3/3 on WSL GCC, WSL Clang, and Windows MSVC Debug.

Large test-file note:

- `tests/module/test_reflection_token_resolve.c` is already oversized. This slice stayed in the existing fixture because it is coverage-only and shares the same FieldInfo inline-storage setup.
- Smallest useful follow-up boundary: extract FieldInfo inline-storage fixtures into a focused test target.

Remaining open:

- Recursive nested inline field decomposition and marshaling.
- Managed `FieldInfo.GetValue/SetValue` method surface.
- Complete signature-derived field type binding.
- Cross-module provider loading/version compatibility.
- `@dynamically_accessed` dataflow, DESCRIPTION promotion, trim analyzer completion, and full metadata sweep.
