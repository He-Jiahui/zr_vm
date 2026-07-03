# AOT 10-S4Z6 / 11-S4AR FieldInfo Primitive POD Read/Write

## Scope

- Extends the public FieldDef-token value APIs to same-runtime primitive POD inline fields.
- Keeps `VALUE_SLOT` read/write behavior unchanged while adding a raw primitive path in
  `zr_vm_core/src/zr_vm_core/reflection_field_value.c`.
- Uses existing metadata only: FieldDef token resolution, owner `SZrTypeLayoutField` offset/range checks,
  and `FIELD_SIG(PRIMITIVE(...))` signature parsing.

## Baseline

- Previous 10-S4Z4/10-S4Z5 support accepted only `VALUE_SLOT` fields.
- RED after adding the primitive POD fixture:
  - Windows MSVC Debug `zr_vm_reflection_token_resolve_test` built and ran 14 tests with 1 failure.
  - Failure: primitive POD read returned false at the new assertion, because the old resolver rejected fields
    without `ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT`.

## Test Inventory

- `tests/module/test_reflection_token_resolve.c`
  - Added `test_reflection_reads_and_writes_field_info_primitive_pod_from_inline_storage`.
  - Fixture encodes `FIELD_SIG(PRIMITIVE(INT32))`.
  - Owner field uses raw inline storage at byte offset `40`, `byteSize == sizeof(TZrInt32)`, and no
    `VALUE_SLOT`/GC/ownership flags.
  - Read path verifies raw `-12345` is returned as `ZR_VALUE_TYPE_INT64`.
  - Write path rejects a bool write, accepts int `2048`, mutates raw storage, and reads back `2048`.
  - Short-storage guard remains false.

## Implementation

- Added `reflection_field_value.c` as the FieldInfo token value boundary.
- The shared resolver now validates FieldDef token kind, owner layout, field offset/type-layout match, and caller
  inline-storage bounds.
- `VALUE_SLOT` fields still copy `SZrTypeValue` through `ZrCore_Value_Copy()`.
- Raw primitive fields require:
  - `FIELD_SIG` root and `PRIMITIVE` field type node.
  - No `VALUE_SLOT`, GC value, or ownership value field flags.
  - Exact primitive byte size match.
- Raw primitive load/store uses `memcpy` and canonical boxes signed integers as `INT64`, unsigned integers as
  `UINT64`, bool as bool, and float/double as double.

## Validation

- Focused RED: Windows MSVC Debug `zr_vm_reflection_token_resolve_test`, 14 tests / 1 failure.
- Focused GREEN: Windows MSVC Debug `zr_vm_reflection_token_resolve_test`, 14/0.
- Direct three-environment verification:
  - WSL GCC: `reflection_token_resolve` 14/0, `metadata_runtime_query` 24/0,
    `metadata_runtime_typespec_layout` 17/0.
  - WSL Clang: `reflection_token_resolve` 14/0, `metadata_runtime_query` 24/0,
    `metadata_runtime_typespec_layout` 17/0.
  - Windows MSVC Debug: `reflection_token_resolve` 14/0, `metadata_runtime_query` 24/0,
    `metadata_runtime_typespec_layout` 17/0.
- Focused CTest:
  - WSL GCC: 3/3.
  - WSL Clang: 3/3.
  - Windows MSVC Debug: 3/3.

## Acceptance Decision

Accepted for 10-S4Z6 / 11-S4AR / 12-S5 support.

Remaining work: broader primitive variant fixture matrix, nested POD/struct field marshaling, object-level
`FieldInfo.GetValue/SetValue`, cross-module provider binding, `@dynamically_accessed` dataflow, DESCRIPTION
promotion, and complete metadata sweep.

