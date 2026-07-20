#include "unity.h"

#include <string.h>

#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"

void setUp(void) {}

void tearDown(void) {}

static void test_pod_layout_records_blittable_and_c_type_metadata(void) {
    SZrTypeLayout layout;
    SZrTypeLayoutMetadata metadata;

    memset(&metadata, 0, sizeof(metadata));
    metadata.cTypeId = 42u;
    metadata.gcFieldOffsets = ZR_NULL;
    metadata.ownershipFieldOffsets = ZR_NULL;

    ZrCore_TypeLayout_InitStructWithMetadata(
            &layout,
            16u,
            8u,
            ZR_TYPE_LAYOUT_COPY_KIND_POD,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u,
            &metadata);

    TEST_ASSERT_TRUE(layout.blittable);
    TEST_ASSERT_EQUAL_UINT32(42u, layout.cTypeId);
    TEST_ASSERT_NULL(layout.gcFieldOffsets);
    TEST_ASSERT_NULL(layout.ownershipFieldOffsets);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_CanRawCopy(&layout));
}

static void test_managed_layout_records_gc_and_ownership_offset_tables(void) {
    SZrTypeLayoutField fields[2];
    TZrUInt32 gcOffsets[2] = {8u, 24u};
    TZrUInt32 ownershipOffsets[1] = {24u};
    SZrTypeLayoutMetadata metadata;
    SZrTypeLayout layout;

    memset(&metadata, 0, sizeof(metadata));

    fields[0].byteOffset = 8u;
    fields[0].byteSize = sizeof(SZrTypeValue);
    fields[0].typeLayoutIndex = 0u;
    fields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                      ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE;
    fields[0].activeTag = 0u;

    fields[1].byteOffset = 24u;
    fields[1].byteSize = sizeof(SZrTypeValue);
    fields[1].typeLayoutIndex = 0u;
    fields[1].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                      ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                      ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
    fields[1].activeTag = 0u;

    metadata.cTypeId = 77u;
    metadata.gcFieldOffsets = gcOffsets;
    metadata.ownershipFieldOffsets = ownershipOffsets;

    ZrCore_TypeLayout_InitStructWithMetadata(
            &layout,
            64u,
            8u,
            ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY,
            ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP,
            fields,
            ZR_ARRAY_COUNT(fields),
            &metadata);

    TEST_ASSERT_FALSE(layout.blittable);
    TEST_ASSERT_EQUAL_UINT32(77u, layout.cTypeId);
    TEST_ASSERT_EQUAL_UINT32(2u, layout.gcFieldCount);
    TEST_ASSERT_EQUAL_UINT32(1u, layout.ownershipFieldCount);
    TEST_ASSERT_EQUAL_PTR(gcOffsets, layout.gcFieldOffsets);
    TEST_ASSERT_EQUAL_PTR(ownershipOffsets, layout.ownershipFieldOffsets);
    TEST_ASSERT_EQUAL_UINT32(8u, layout.gcFieldOffsets[0]);
    TEST_ASSERT_EQUAL_UINT32(24u, layout.gcFieldOffsets[1]);
    TEST_ASSERT_EQUAL_UINT32(24u, layout.ownershipFieldOffsets[0]);
    TEST_ASSERT_FALSE(ZrCore_TypeLayout_CanRawCopy(&layout));
}

static void test_default_struct_init_keeps_neutral_aot_metadata(void) {
    SZrTypeLayout layout;

    ZrCore_TypeLayout_InitStruct(
            &layout,
            8u,
            4u,
            ZR_TYPE_LAYOUT_COPY_KIND_POD,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u);

    TEST_ASSERT_TRUE(layout.blittable);
    TEST_ASSERT_EQUAL_UINT32(0u, layout.cTypeId);
    TEST_ASSERT_NULL(layout.gcFieldOffsets);
    TEST_ASSERT_NULL(layout.ownershipFieldOffsets);
}

static void test_null_field_table_does_not_scan_metadata_counts(void) {
    SZrTypeLayout layout;

    ZrCore_TypeLayout_InitStruct(
            &layout,
            8u,
            4u,
            ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY,
            ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP,
            ZR_NULL,
            3u);

    TEST_ASSERT_EQUAL_UINT32(3u, layout.fieldCount);
    TEST_ASSERT_EQUAL_UINT32(0u, layout.gcFieldCount);
    TEST_ASSERT_EQUAL_UINT32(0u, layout.ownershipFieldCount);
    TEST_ASSERT_FALSE(layout.blittable);
}

static void test_layout_contract_exposes_canonical_copy_drop_and_scan_kinds(void) {
    SZrTypeLayout layout;
    TZrByte source[8] = {0u};
    TZrByte destination[8] = {0u};

    TEST_ASSERT_EQUAL_INT(ZR_TYPE_LAYOUT_COPY_KIND_BITWISE, ZR_TYPE_LAYOUT_COPY_KIND_POD);
    TEST_ASSERT_EQUAL_INT(ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE, ZR_TYPE_LAYOUT_COPY_KIND_FIELD_COPY);
    TEST_ASSERT_EQUAL_INT(ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE, ZR_TYPE_LAYOUT_DROP_KIND_FIELD_DROP);

    ZrCore_TypeLayout_InitStruct(
            &layout,
            sizeof(source),
            4u,
            ZR_TYPE_LAYOUT_COPY_KIND_MOVE_ONLY,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u);

    TEST_ASSERT_EQUAL_UINT8(ZR_TYPE_LAYOUT_COPY_KIND_MOVE_ONLY, layout.copyKind);
    TEST_ASSERT_EQUAL_UINT8(ZR_TYPE_LAYOUT_GC_SCAN_FREE, layout.gcScanKind);
    TEST_ASSERT_FALSE(layout.blittable);
    TEST_ASSERT_FALSE(ZrCore_TypeLayout_CanRawCopy(&layout));
    TEST_ASSERT_FALSE(ZrCore_TypeLayout_CopyInline(ZR_NULL, &layout, destination, source));
}

static void test_layout_contract_records_gc_ownership_and_ref_maps(void) {
    SZrTypeLayoutField fields[3];
    const TZrUInt32 gcOffsets[1] = {0u};
    const TZrUInt32 ownershipOffsets[1] = {(TZrUInt32)sizeof(SZrTypeValue)};
    const TZrUInt32 refOffsets[1] = {(TZrUInt32)(sizeof(SZrTypeValue) * 2u)};
    SZrTypeLayoutContract contract;
    SZrTypeLayout layout;

    memset(fields, 0, sizeof(fields));
    memset(&contract, 0, sizeof(contract));
    fields[0].byteOffset = gcOffsets[0];
    fields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    fields[0].typeLayoutIndex = 11u;
    fields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                      ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE;
    fields[1].byteOffset = ownershipOffsets[0];
    fields[1].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    fields[1].typeLayoutIndex = 12u;
    fields[1].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                      ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
    fields[2].byteOffset = refOffsets[0];
    fields[2].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    fields[2].typeLayoutIndex = 13u;
    fields[2].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                      ZR_TYPE_LAYOUT_FIELD_FLAG_REF_VALUE;

    contract.cTypeId = 99u;
    contract.gcScanKind = ZR_TYPE_LAYOUT_GC_SCAN_MAPPED;
    contract.gcFieldOffsets = gcOffsets;
    contract.gcFieldCount = ZR_ARRAY_COUNT(gcOffsets);
    contract.ownershipFieldOffsets = ownershipOffsets;
    contract.ownershipFieldCount = ZR_ARRAY_COUNT(ownershipOffsets);
    contract.refFieldOffsets = refOffsets;
    contract.refFieldCount = ZR_ARRAY_COUNT(refOffsets);

    ZrCore_TypeLayout_InitStructWithContract(
            &layout,
            (TZrUInt32)(sizeof(SZrTypeValue) * 3u),
            (TZrUInt32)ZR_ALIGN_SIZE,
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE,
            fields,
            ZR_ARRAY_COUNT(fields),
            &contract);

    TEST_ASSERT_EQUAL_UINT32(99u, layout.cTypeId);
    TEST_ASSERT_EQUAL_UINT8(ZR_TYPE_LAYOUT_GC_SCAN_MAPPED, layout.gcScanKind);
    TEST_ASSERT_EQUAL_UINT32(1u, layout.gcFieldCount);
    TEST_ASSERT_EQUAL_UINT32(1u, layout.ownershipFieldCount);
    TEST_ASSERT_EQUAL_UINT32(1u, layout.refFieldCount);
    TEST_ASSERT_EQUAL_PTR(gcOffsets, layout.gcFieldOffsets);
    TEST_ASSERT_EQUAL_PTR(ownershipOffsets, layout.ownershipFieldOffsets);
    TEST_ASSERT_EQUAL_PTR(refOffsets, layout.refFieldOffsets);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&layout));
}

static void test_layout_hash_is_stable_and_tracks_structural_drift(void) {
    SZrTypeLayoutField baseFields[1];
    SZrTypeLayoutField changedFields[1];
    SZrTypeLayout first;
    SZrTypeLayout second;
    SZrTypeLayout changed;

    memset(baseFields, 0, sizeof(baseFields));
    baseFields[0].byteOffset = 4u;
    baseFields[0].byteSize = 4u;
    baseFields[0].typeLayoutIndex = 7u;
    memcpy(changedFields, baseFields, sizeof(baseFields));
    changedFields[0].byteOffset = 8u;

    ZrCore_TypeLayout_InitStruct(&first,
                                 16u,
                                 8u,
                                 ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
                                 ZR_TYPE_LAYOUT_DROP_KIND_NONE,
                                 baseFields,
                                 ZR_ARRAY_COUNT(baseFields));
    ZrCore_TypeLayout_InitStruct(&second,
                                 16u,
                                 8u,
                                 ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
                                 ZR_TYPE_LAYOUT_DROP_KIND_NONE,
                                 baseFields,
                                 ZR_ARRAY_COUNT(baseFields));
    ZrCore_TypeLayout_InitStruct(&changed,
                                 16u,
                                 8u,
                                 ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
                                 ZR_TYPE_LAYOUT_DROP_KIND_NONE,
                                 changedFields,
                                 ZR_ARRAY_COUNT(changedFields));

    TEST_ASSERT_EQUAL_UINT32(ZR_TYPE_LAYOUT_SCHEMA_VERSION, first.layoutVersion);
    TEST_ASSERT_NOT_EQUAL(0u, first.layoutHash);
    TEST_ASSERT_EQUAL_UINT64(first.layoutHash, second.layoutHash);
    TEST_ASSERT_NOT_EQUAL(first.layoutHash, changed.layoutHash);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&first));
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&second));
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&changed));
}

static void test_layout_validation_rejects_invalid_spans_maps_and_identity(void) {
    const TZrUInt32 invalidGcOffset[1] = {12u};
    SZrTypeLayoutField field;
    SZrTypeLayoutContract contract;
    SZrTypeLayout layout;

    memset(&field, 0, sizeof(field));
    memset(&contract, 0, sizeof(contract));
    field.byteOffset = 6u;
    field.byteSize = 4u;
    field.typeLayoutIndex = 1u;
    field.flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                  ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE;
    contract.gcScanKind = ZR_TYPE_LAYOUT_GC_SCAN_MAPPED;
    contract.gcFieldOffsets = invalidGcOffset;
    contract.gcFieldCount = ZR_ARRAY_COUNT(invalidGcOffset);

    ZrCore_TypeLayout_InitStructWithContract(&layout,
                                             8u,
                                             4u,
                                             ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
                                             ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE,
                                             &field,
                                             1u,
                                             &contract);

    TEST_ASSERT_FALSE(ZrCore_TypeLayout_Validate(&layout));

    field.byteOffset = 0u;
    field.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    ZrCore_TypeLayout_InitStructWithContract(&layout,
                                             (TZrUInt32)sizeof(SZrTypeValue),
                                             (TZrUInt32)ZR_ALIGN_SIZE,
                                             ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
                                             ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE,
                                             &field,
                                             1u,
                                             &contract);
    TEST_ASSERT_FALSE(ZrCore_TypeLayout_Validate(&layout));

    contract.gcFieldOffsets = ZR_NULL;
    contract.gcFieldCount = 0u;
    contract.gcScanKind = ZR_TYPE_LAYOUT_GC_SCAN_FREE;
    ZrCore_TypeLayout_InitStructWithContract(&layout,
                                             (TZrUInt32)sizeof(SZrTypeValue),
                                             (TZrUInt32)ZR_ALIGN_SIZE,
                                             ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
                                             ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE,
                                             &field,
                                             1u,
                                             &contract);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&layout));
    layout.layoutHash ^= 1u;
    TEST_ASSERT_FALSE(ZrCore_TypeLayout_Validate(&layout));
    layout.layoutHash ^= 1u;
    layout.layoutVersion++;
    TEST_ASSERT_FALSE(ZrCore_TypeLayout_Validate(&layout));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pod_layout_records_blittable_and_c_type_metadata);
    RUN_TEST(test_managed_layout_records_gc_and_ownership_offset_tables);
    RUN_TEST(test_default_struct_init_keeps_neutral_aot_metadata);
    RUN_TEST(test_null_field_table_does_not_scan_metadata_counts);
    RUN_TEST(test_layout_contract_exposes_canonical_copy_drop_and_scan_kinds);
    RUN_TEST(test_layout_contract_records_gc_ownership_and_ref_maps);
    RUN_TEST(test_layout_hash_is_stable_and_tracks_structural_drift);
    RUN_TEST(test_layout_validation_rejects_invalid_spans_maps_and_identity);
    return UNITY_END();
}
