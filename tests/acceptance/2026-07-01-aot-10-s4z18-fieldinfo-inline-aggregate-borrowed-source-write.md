# AOT 10-S4Z18 / 11-S4BD FieldInfo inline aggregate borrowed-source write

Time: 2026-07-01 11:34:10 +08:00

Status: completed support sub-slice. This is a restricted byte-copy write contract for blittable inline aggregates, not full nested field decomposition or managed `FieldInfo.SetValue`.

Note: 10-S4Z19 later extends this boundary for selected non-blittable `FIELD_COPY` layouts through `ZrCore_TypeLayout_CopyInline()`. This file records the narrower S4Z18 acceptance point.

Completed:

- `reflection_field_value.c` now reuses the FieldDef field signature type-node reader for non-primitive writes.
- Non-`VALUE_SLOT`, non-GC, non-ownership inline struct/union fields with validated `FIELD_SIG(TYPE_DEF/TYPE_REF)` and matching resolved field type layout size can be written from a non-null `ZR_VALUE_TYPE_NATIVE_POINTER` source when the resolved field type layout is blittable.
- Accepted writes copy exactly the resolved field byte size from the native-pointer source into caller-provided inline storage.
- Null native-pointer sources, scalar sources, non-blittable field type layouts, GC/ownership field flags, primitive POD mismatches, and short storage remain rejected without changing existing bytes.
- `test_reflection_reads_field_info_object_inline_struct_borrowed_view()` now covers borrowed native-pointer read, native-pointer source write, null/scalar source rejection, non-blittable rejection, and GC/ownership exposure rejection.

RED/GREEN:

- RED: Windows MSVC Debug focused `reflection_token_resolve` failed 1/24 after adding the native-pointer source write expectation, with `Expected TRUE Was FALSE`.
- GREEN: Windows MSVC Debug focused `reflection_token_resolve` passed 24/0 after the borrowed-source write path.

Validation:

- WSL GCC direct: `reflection_token_resolve` 24/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- WSL Clang direct: `reflection_token_resolve` 24/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- Windows MSVC Debug direct: `reflection_token_resolve` 24/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- Focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout`: 3/3 on WSL GCC, WSL Clang, and Windows MSVC Debug.

Remaining open:

- Full nested inline field decomposition and recursive marshaling.
- Non-blittable field-copy/drop semantics.
- Managed `FieldInfo.GetValue/SetValue` method surface.
- Cross-module provider loading/version compatibility.
- `@dynamically_accessed` dataflow, DESCRIPTION promotion, trim analyzer completion, and full metadata sweep.
