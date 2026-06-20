#include "unity.h"

#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"

void setUp(void) {}

void tearDown(void) {}

static void test_pod_layout_records_blittable_and_c_type_metadata(void) {
    SZrTypeLayout layout;
    SZrTypeLayoutMetadata metadata;

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

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_pod_layout_records_blittable_and_c_type_metadata);
    RUN_TEST(test_managed_layout_records_gc_and_ownership_offset_tables);
    RUN_TEST(test_default_struct_init_keeps_neutral_aot_metadata);
    RUN_TEST(test_null_field_table_does_not_scan_metadata_counts);
    return UNITY_END();
}
