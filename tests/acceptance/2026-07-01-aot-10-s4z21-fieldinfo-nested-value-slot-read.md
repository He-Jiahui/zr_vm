# AOT 10-S4Z21 / 11-S4BG FieldInfo nested inline VALUE_SLOT read

Time: 2026-07-01 12:24:56 +08:00

Status: completed support sub-slice. This closes the first layout-indexed nested `VALUE_SLOT` child read boundary for retained FieldInfo inline aggregates, not full recursive nested marshaling.

Completed:

- Added `ZrCore_Reflection_ReadFieldInfoTokenNestedValue()`.
- Added `ZrCore_Reflection_ReadFieldInfoObjectNestedValue()`.
- The token adapter resolves the retained FieldDef token, validates `FIELD_SIG(TYPE_DEF/TYPE_REF)`, requires a resolved struct/union field type layout, and reads a nested `SZrTypeLayoutField` by index.
- The object adapter reads `FieldInfo.metadataRuntime` and `FieldInfo.metadataToken`, then delegates to the token adapter.
- The nested read path currently supports only child fields marked `VALUE_SLOT` and returns a copied `SZrTypeValue` through `ZrCore_Value_Copy()`.
- Added `test_reflection_reads_field_info_object_nested_value_slot_from_inline_struct()`.
- The fixture verifies short inline storage rejection, out-of-range nested index rejection, outer GC/ownership flag rejection, and a successful nested int value read of `314159`.

RED/GREEN:

- RED: Windows MSVC Debug focused build failed because `ZrCore_Reflection_ReadFieldInfoObjectNestedValue` was missing, producing an undefined function warning and LNK2019 unresolved external.
- GREEN: after adding the token/object adapters, Windows focused `reflection_token_resolve` passed 26/0.

Validation:

- WSL GCC direct: `reflection_token_resolve` 26/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- WSL Clang direct: `reflection_token_resolve` 26/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- Windows MSVC Debug direct: `reflection_token_resolve` 26/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- Focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout`: 3/3 on WSL GCC, WSL Clang, and Windows MSVC Debug.

Large test-file note:

- `tests/module/test_reflection_token_resolve.c` is already oversized. This slice stayed in the existing FieldInfo inline-storage fixture because it adds the next narrow behavior on the same setup.
- Smallest useful follow-up boundary: extract FieldInfo inline-storage fixtures into a focused test target.

Remaining open:

- Multi-level recursive path API.
- Nested inline field write.
- Primitive raw child read/write marshaling.
- Managed `FieldInfo.GetValue/SetValue` method surface.
- Complete signature-derived field type binding.
- Cross-module provider loading/version compatibility.
- `@dynamically_accessed` dataflow, DESCRIPTION promotion, trim analyzer completion, and full metadata sweep.
