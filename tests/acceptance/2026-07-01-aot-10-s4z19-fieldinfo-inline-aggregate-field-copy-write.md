# AOT 10-S4Z19 / 11-S4BE FieldInfo inline aggregate field-copy borrowed-source write

Time: 2026-07-01 11:52:49 +08:00

Status: completed support sub-slice. This is a layout-aware borrowed-source write contract for selected inline aggregates, not full nested field decomposition or managed `FieldInfo.SetValue`.

Completed:

- `reflection_field_value.c` now permits native-pointer source writes for inline aggregate fields when the S4Z17 borrowed-view predicate passes and the resolved field type layout is either raw-copy capable or `ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY`.
- Non-primitive writes delegate to `ZrCore_TypeLayout_CopyInline()` instead of directly using `memcpy`.
- Raw-copy layouts keep the S4Z18 byte-copy behavior, while non-blittable field-copy layouts now copy fields through the central type-layout copy semantics.
- Null native-pointer sources, scalar sources, GC/ownership field flags, unsupported copy layouts, primitive POD mismatches, and short storage remain rejected without changing existing bytes.
- `test_reflection_reads_field_info_object_inline_struct_borrowed_view()` now covers a non-blittable two-int field-copy layout source write from `{11, 22}` while retaining borrowed native-pointer read and rejection coverage.

RED/GREEN:

- RED: Windows MSVC Debug focused `reflection_token_resolve` failed 1/24 after changing the non-blittable field-copy source write expectation to success, with `Expected TRUE Was FALSE`.
- GREEN: Windows MSVC Debug focused `reflection_token_resolve` passed 24/0 after routing inline aggregate writes through `ZrCore_TypeLayout_CopyInline()`.

Validation:

- WSL GCC direct: `reflection_token_resolve` 24/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- WSL Clang direct: `reflection_token_resolve` 24/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- Windows MSVC Debug direct: `reflection_token_resolve` 24/0, `metadata_runtime_query` 24/0, `metadata_runtime_typespec_layout` 17/0.
- Focused CTest `reflection_token_resolve|metadata_runtime_query|metadata_runtime_typespec_layout`: 3/3 on WSL GCC, WSL Clang, and Windows MSVC Debug.

Remaining open:

- Full nested inline field decomposition and recursive marshaling.
- Destination ownership replacement/drop lifecycle for writes over existing owned fields.
- Managed `FieldInfo.GetValue/SetValue` method surface.
- Cross-module provider loading/version compatibility.
- `@dynamically_accessed` dataflow, DESCRIPTION promotion, trim analyzer completion, and full metadata sweep.
