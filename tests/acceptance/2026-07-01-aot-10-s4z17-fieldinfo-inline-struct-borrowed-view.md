# AOT 10-S4Z17 / 11-S4BC FieldInfo inline struct borrowed view

Time: 2026-07-01 11:26:41 +08:00

Status: completed support sub-slice. This is the first read-only nested inline field marshaling contract, not full nested field decomposition or write support.

Completed:

- `reflection_field_value.c` now reads the FieldDef field type-node once for non-`VALUE_SLOT` reads.
- Non-GC/non-ownership inline struct/union fields with validated `FIELD_SIG(TYPE_DEF/TYPE_REF)` and matching resolved field type layout size return a borrowed `ZR_VALUE_TYPE_NATIVE_POINTER` view to the caller-provided inline storage.
- At this slice, non-primitive inline aggregate writes remained rejected; 10-S4Z18 later adds a restricted native-pointer source write.
- `test_reflection_reads_field_info_object_inline_struct_borrowed_view()` covers short storage rejection, borrowed native-pointer metadata, write rejection with byte preservation, and GC/ownership flag rejection.

RED/GREEN:

- RED: Windows MSVC Debug focused `reflection_token_resolve` failed 1/24 after adding the inline struct object fixture, with `Expected TRUE Was FALSE`.
- GREEN: Windows MSVC Debug focused `reflection_token_resolve` passed 24/0 after the borrowed-view read path.

Validation:

- WSL GCC direct: `reflection_token_resolve` 24/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- WSL Clang direct: `reflection_token_resolve` 24/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- Windows MSVC Debug direct: `reflection_token_resolve` 24/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- Focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout`: 3/3 on WSL GCC, WSL Clang, and Windows MSVC Debug.

Remaining open:

- Full nested inline field decomposition/copy/write.
- Managed `FieldInfo.GetValue/SetValue` method surface.
- Cross-module provider loading/version compatibility.
- `@dynamically_accessed` dataflow, DESCRIPTION promotion, trim analyzer completion, and full metadata sweep.
