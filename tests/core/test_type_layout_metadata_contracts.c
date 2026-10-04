#include "unity.h"

#include <string.h>

#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"

/* 构造器将 AOT 字段表与复制、GC、所有权和跨域传递约束一起固化为布局身份。
 * 本套件直接检查该身份，供 IO、反射和跨域路径依赖同一份契约。 */
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

/* 三张字段表的用途不同；可被 GC 扫描不自动意味着拥有引用。 */
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

/* 布局哈希用于发现元数据漂移；相同字段稳定，本例偏移变化产生不同身份。 */
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

/* 验证端同时拒绝字段越界、映射失配、哈希漂移和版本漂移。 */
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

/* 跨域传递策略及 provider 身份参与布局约束，不能仅按字节形状作值复制。 */
static void test_domain_transfer_kind_is_canonical_layout_identity(void) {
    SZrTypeLayout plainLayout;
    SZrTypeLayout forbiddenLayout;
    SZrTypeLayoutField gcField;
    SZrTypeLayoutContract forbiddenContract;
    SZrTypeLayoutContract cloneContract;
    SZrTypeLayout cloneLayout;
    SZrTypeLayout driftedLayout;
    SZrTypeLayout providerLayout;
    SZrTypeLayout invalidProviderLayout;

    ZrCore_TypeLayout_InitStruct(
            &plainLayout,
            16u,
            8u,
            ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u);
    TEST_ASSERT_EQUAL_INT(
            ZR_DOMAIN_TRANSFER_KIND_VALUE_COPY,
            plainLayout.domainTransferKind);
    TEST_ASSERT_EQUAL_UINT32(
            ZR_TYPE_LAYOUT_SCHEMA_VERSION,
            plainLayout.domainTransferSchemaVersion);
    TEST_ASSERT_NOT_EQUAL(0u, plainLayout.domainTransferSchemaHash);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&plainLayout));

    memset(&forbiddenContract, 0, sizeof(forbiddenContract));
    forbiddenContract.hasDomainTransferContract = ZR_TRUE;
    forbiddenContract.domainTransferKind = ZR_DOMAIN_TRANSFER_KIND_FORBIDDEN;
    ZrCore_TypeLayout_InitStructWithContract(
            &forbiddenLayout,
            16u,
            8u,
            ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u,
            &forbiddenContract);
    TEST_ASSERT_EQUAL_INT(
            ZR_DOMAIN_TRANSFER_KIND_FORBIDDEN,
            forbiddenLayout.domainTransferKind);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&forbiddenLayout));

    memset(&gcField, 0, sizeof(gcField));
    gcField.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    gcField.flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                    ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE;
    ZrCore_TypeLayout_InitStruct(
            &cloneLayout,
            (TZrUInt32)sizeof(SZrTypeValue),
            (TZrUInt32)ZR_ALIGN_SIZE,
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE,
            &gcField,
            1u);
    TEST_ASSERT_EQUAL_INT(
            ZR_DOMAIN_TRANSFER_KIND_FORBIDDEN,
            cloneLayout.domainTransferKind);

    memset(&cloneContract, 0, sizeof(cloneContract));
    cloneContract.hasDomainTransferContract = ZR_TRUE;
    cloneContract.domainTransferKind = ZR_DOMAIN_TRANSFER_KIND_STRUCTURED_CLONE;
    cloneContract.domainTransferSchemaVersion = 3u;
    cloneContract.domainTransferSchemaHash = UINT64_C(0x123456789abcdef0);
    ZrCore_TypeLayout_InitStructWithContract(
            &cloneLayout,
            (TZrUInt32)sizeof(SZrTypeValue),
            (TZrUInt32)ZR_ALIGN_SIZE,
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE,
            &gcField,
            1u,
            &cloneContract);
    TEST_ASSERT_EQUAL_INT(
            ZR_DOMAIN_TRANSFER_KIND_STRUCTURED_CLONE,
            cloneLayout.domainTransferKind);
    TEST_ASSERT_EQUAL_UINT32(3u, cloneLayout.domainTransferSchemaVersion);
    TEST_ASSERT_EQUAL_UINT64(
            UINT64_C(0x123456789abcdef0),
            cloneLayout.domainTransferSchemaHash);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&cloneLayout));

    cloneContract.domainTransferSchemaHash ^= 1u;
    ZrCore_TypeLayout_InitStructWithContract(
            &driftedLayout,
            (TZrUInt32)sizeof(SZrTypeValue),
            (TZrUInt32)ZR_ALIGN_SIZE,
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE,
            &gcField,
            1u,
            &cloneContract);
    TEST_ASSERT_NOT_EQUAL(cloneLayout.layoutHash, driftedLayout.layoutHash);

    memset(&cloneContract, 0, sizeof(cloneContract));
    cloneContract.hasDomainTransferContract = ZR_TRUE;
    cloneContract.domainTransferKind =
            ZR_DOMAIN_TRANSFER_KIND_IMMUTABLE_HANDLE;
    cloneContract.domainTransferSchemaVersion = 1u;
    cloneContract.domainTransferSchemaHash = UINT64_C(0x2233445566778899);
    cloneContract.domainTransferProviderToken = 0x06000001u;
    cloneContract.domainTransferProviderContractHash =
            UINT64_C(0xaabbccddeeff0011);
    ZrCore_TypeLayout_InitStructWithContract(
            &providerLayout,
            16u,
            8u,
            ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u,
            &cloneContract);
    TEST_ASSERT_EQUAL_INT(
            ZR_DOMAIN_TRANSFER_KIND_IMMUTABLE_HANDLE,
            providerLayout.domainTransferKind);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&providerLayout));

    cloneContract.domainTransferKind = ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE;
    cloneContract.domainTransferProviderToken = 0x06000002u;
    cloneContract.domainTransferProviderContractHash =
            UINT64_C(0x1122334455667788);
    ZrCore_TypeLayout_InitStructWithContract(
            &providerLayout,
            16u,
            8u,
            ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u,
            &cloneContract);
    TEST_ASSERT_EQUAL_INT(
            ZR_DOMAIN_TRANSFER_KIND_RESOURCE_MOVE,
            providerLayout.domainTransferKind);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&providerLayout));

    ZrCore_TypeLayout_InitStructWithContract(
            &invalidProviderLayout,
            (TZrUInt32)sizeof(SZrTypeValue),
            (TZrUInt32)ZR_ALIGN_SIZE,
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE,
            &gcField,
            1u,
            &cloneContract);
    TEST_ASSERT_FALSE(ZrCore_TypeLayout_Validate(&invalidProviderLayout));

    gcField.flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                    ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
    ZrCore_TypeLayout_InitStructWithContract(
            &invalidProviderLayout,
            (TZrUInt32)sizeof(SZrTypeValue),
            (TZrUInt32)ZR_ALIGN_SIZE,
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE,
            &gcField,
            1u,
            &cloneContract);
    TEST_ASSERT_FALSE(ZrCore_TypeLayout_Validate(&invalidProviderLayout));

    providerLayout.domainTransferProviderToken = 0u;
    providerLayout.layoutHash = ZrCore_TypeLayout_ComputeHash(&providerLayout);
    TEST_ASSERT_FALSE(ZrCore_TypeLayout_Validate(&providerLayout));

    driftedLayout.domainTransferKind = (TZrUInt8)99u;
    TEST_ASSERT_FALSE(ZrCore_TypeLayout_Validate(&driftedLayout));
}

typedef struct SExplicitMapVisitRecord {
    SZrTypeValue *storage;
    TZrUInt32 count;
    TZrUInt32 offsets[4];
} SExplicitMapVisitRecord;

static void record_explicit_map_visit(
        struct SZrState *state, SZrTypeValue *value, TZrPtr userData) {
    SExplicitMapVisitRecord *record = (SExplicitMapVisitRecord *)userData;
    ZR_UNUSED_PARAMETER(state);
    if (record->count < ZR_ARRAY_COUNT(record->offsets)) {
        record->offsets[record->count] =
                (TZrUInt32)((TZrByte *)value - (TZrByte *)record->storage);
    }
    record->count++;
}

static void init_explicit_map_layout(
        SZrTypeLayout *layout, SZrTypeLayoutField *fields,
        TZrUInt32 fieldCount, TZrUInt32 flag,
        const TZrUInt32 *offsets, TZrUInt32 mapCount) {
    SZrTypeLayoutContract contract;
    memset(&contract, 0, sizeof(contract));
    if (flag == ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE) {
        contract.gcScanKind = ZR_TYPE_LAYOUT_GC_SCAN_MAPPED;
        contract.gcFieldOffsets = offsets;
        contract.gcFieldCount = mapCount;
    } else if (flag == ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE) {
        contract.ownershipFieldOffsets = offsets;
        contract.ownershipFieldCount = mapCount;
    } else {
        contract.refFieldOffsets = offsets;
        contract.refFieldCount = mapCount;
    }
    ZrCore_TypeLayout_InitStructWithContract(
            layout, (TZrUInt32)sizeof(SZrTypeValue) * fieldCount,
            (TZrUInt32)_Alignof(SZrTypeValue),
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE,
            fields, fieldCount, &contract);
}

static void assert_explicit_map_rejects_wrong_kind(TZrUInt32 flag) {
    SZrTypeLayoutField fields[2];
    SZrTypeLayout layout;
    SZrTypeValue storage[2];
    const TZrUInt32 wrongOffset[1] = {(TZrUInt32)sizeof(SZrTypeValue)};
    TZrBool valid;

    memset(fields, 0, sizeof(fields));
    memset(storage, 0, sizeof(storage));
    fields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    fields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT | flag;
    fields[1].byteOffset = (TZrUInt32)sizeof(SZrTypeValue);
    fields[1].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    fields[1].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT;
    init_explicit_map_layout(&layout, fields, 2u, flag, wrongOffset, 1u);
    valid = ZrCore_TypeLayout_Validate(&layout);
    /* The baseline witness visits only real aligned slots, without collecting objects. */
    if (valid && flag == ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE) {
        SExplicitMapVisitRecord record = {storage, 0u, {0u}};
        TEST_ASSERT_TRUE(ZrCore_TypeLayout_VisitGcValuesWithRegistry(
                ZR_NULL, &layout, ZR_NULL, storage,
                record_explicit_map_visit, &record));
        TEST_ASSERT_EQUAL_UINT32(1u, record.count);
        TEST_ASSERT_EQUAL_UINT32(wrongOffset[0], record.offsets[0]);
        printf("baseline wrong GC map: visited=%u expected=0\n",
               (unsigned)record.offsets[0]);
    }
    TEST_ASSERT_FALSE(valid);
}

static void test_explicit_gc_map_rejects_in_bounds_non_gc_slot(void) {
    assert_explicit_map_rejects_wrong_kind(ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE);
}

static void test_explicit_ownership_map_rejects_in_bounds_non_owner_slot(void) {
    assert_explicit_map_rejects_wrong_kind(ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE);
}

static void test_explicit_ref_map_rejects_in_bounds_non_ref_slot(void) {
    assert_explicit_map_rejects_wrong_kind(ZR_TYPE_LAYOUT_FIELD_FLAG_REF_VALUE);
}

static void assert_explicit_map_rejects_duplicate_missing_field(TZrUInt32 flag) {
    SZrTypeLayoutField fields[2];
    SZrTypeLayout layout;
    const TZrUInt32 duplicateOffsets[2] = {0u, 0u};
    memset(fields, 0, sizeof(fields));
    for (TZrUInt32 index = 0u; index < ZR_ARRAY_COUNT(fields); ++index) {
        fields[index].byteOffset = index * (TZrUInt32)sizeof(SZrTypeValue);
        fields[index].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        fields[index].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT | flag;
    }
    init_explicit_map_layout(&layout, fields, 2u, flag, duplicateOffsets, 2u);
    TEST_ASSERT_FALSE(ZrCore_TypeLayout_Validate(&layout));
}

static void test_explicit_gc_map_rejects_duplicate_and_missing_field(void) {
    assert_explicit_map_rejects_duplicate_missing_field(ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE);
}

static void test_explicit_ownership_map_rejects_duplicate_and_missing_field(void) {
    assert_explicit_map_rejects_duplicate_missing_field(ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE);
}

static void test_explicit_ref_map_rejects_duplicate_and_missing_field(void) {
    assert_explicit_map_rejects_duplicate_missing_field(ZR_TYPE_LAYOUT_FIELD_FLAG_REF_VALUE);
}

static void test_explicit_maps_allow_permutation_and_null_fallback(void) {
    SZrTypeLayoutField fields[2];
    SZrTypeLayout layout;
    SZrTypeValue storage[2];
    const TZrUInt32 reverseOffsets[2] = {(TZrUInt32)sizeof(SZrTypeValue), 0u};
    const TZrUInt32 flags[3] = {ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE,
                              ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE,
                              ZR_TYPE_LAYOUT_FIELD_FLAG_REF_VALUE};
    memset(fields, 0, sizeof(fields));
    memset(storage, 0, sizeof(storage));
    for (TZrUInt32 kind = 0u; kind < ZR_ARRAY_COUNT(flags); ++kind) {
        for (TZrUInt32 index = 0u; index < ZR_ARRAY_COUNT(fields); ++index) {
            fields[index].byteOffset = index * (TZrUInt32)sizeof(SZrTypeValue);
            fields[index].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
            fields[index].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT | flags[kind];
        }
        init_explicit_map_layout(&layout, fields, 2u, flags[kind], reverseOffsets, 2u);
        TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&layout));
        if (kind == 0u) {
            SExplicitMapVisitRecord record = {storage, 0u, {0u}};
            TEST_ASSERT_TRUE(ZrCore_TypeLayout_VisitGcValuesWithRegistry(
                    ZR_NULL, &layout, ZR_NULL, storage,
                    record_explicit_map_visit, &record));
            TEST_ASSERT_EQUAL_UINT32(2u, record.count);
            TEST_ASSERT_EQUAL_UINT32(reverseOffsets[0], record.offsets[0]);
            TEST_ASSERT_EQUAL_UINT32(reverseOffsets[1], record.offsets[1]);
        }
        init_explicit_map_layout(&layout, fields, 2u, flags[kind], ZR_NULL, 0u);
        TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&layout));
        if (kind == 0u) {
            SExplicitMapVisitRecord record = {storage, 0u, {0u}};
            TEST_ASSERT_TRUE(ZrCore_TypeLayout_VisitGcValuesWithRegistry(
                    ZR_NULL, &layout, ZR_NULL, storage,
                    record_explicit_map_visit, &record));
            TEST_ASSERT_EQUAL_UINT32(2u, record.count);
            TEST_ASSERT_EQUAL_UINT32(0u, record.offsets[0]);
            TEST_ASSERT_EQUAL_UINT32((TZrUInt32)sizeof(SZrTypeValue), record.offsets[1]);
        }
    }
}

static void test_union_explicit_maps_preserve_legal_overlap_and_active_scan(void) {
    SZrTypeLayoutField fields[2];
    SZrTypeLayoutContract contract;
    SZrTypeLayout layout;
    SZrTypeValue storage[2];
    const TZrUInt32 overlappingOffsets[2] = {0u, 0u};
    TZrUInt32 tag = 1u;
    SExplicitMapVisitRecord record = {storage, 0u, {0u}};
    memset(fields, 0, sizeof(fields));
    memset(&contract, 0, sizeof(contract));
    memset(storage, 0, sizeof(storage));
    for (TZrUInt32 index = 0u; index < ZR_ARRAY_COUNT(fields); ++index) {
        fields[index].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        fields[index].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE |
                ZR_TYPE_LAYOUT_FIELD_FLAG_REF_VALUE;
        fields[index].activeTag = index;
    }
    contract.gcScanKind = ZR_TYPE_LAYOUT_GC_SCAN_MAPPED;
    contract.gcFieldOffsets = overlappingOffsets;
    contract.ownershipFieldOffsets = overlappingOffsets;
    contract.refFieldOffsets = overlappingOffsets;
    contract.gcFieldCount = contract.ownershipFieldCount = contract.refFieldCount = 2u;
    ZrCore_TypeLayout_InitUnionWithContract(
            &layout, (TZrUInt32)sizeof(storage), (TZrUInt32)_Alignof(SZrTypeValue),
            (TZrUInt32)sizeof(SZrTypeValue), (TZrUInt32)sizeof(tag),
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE, ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE,
            fields, ZR_ARRAY_COUNT(fields), &contract);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&layout));
    memcpy((TZrByte *)storage + sizeof(SZrTypeValue), &tag, sizeof(tag));
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_VisitGcValuesWithRegistry(
            ZR_NULL, &layout, ZR_NULL, storage, record_explicit_map_visit, &record));
    TEST_ASSERT_EQUAL_UINT32(1u, record.count);
    TEST_ASSERT_EQUAL_UINT32(0u, record.offsets[0]);
}

static void test_union_explicit_map_rejects_wrong_overlap_multiplicity(void) {
    SZrTypeLayoutField fields[3];
    SZrTypeLayoutContract contract;
    SZrTypeLayout layout;
    const TZrUInt32 wrongOffsets[3] = {
        0u, (TZrUInt32)sizeof(SZrTypeValue), (TZrUInt32)sizeof(SZrTypeValue)};
    memset(fields, 0, sizeof(fields));
    memset(&contract, 0, sizeof(contract));
    for (TZrUInt32 index = 0u; index < ZR_ARRAY_COUNT(fields); ++index) {
        fields[index].byteOffset = index == 2u ? (TZrUInt32)sizeof(SZrTypeValue) : 0u;
        fields[index].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
        fields[index].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                              ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE;
        fields[index].activeTag = index;
    }
    contract.gcScanKind = ZR_TYPE_LAYOUT_GC_SCAN_MAPPED;
    contract.gcFieldOffsets = wrongOffsets;
    contract.gcFieldCount = 3u;
    ZrCore_TypeLayout_InitUnionWithContract(
            &layout, (TZrUInt32)sizeof(SZrTypeValue) * 3u,
            (TZrUInt32)_Alignof(SZrTypeValue),
            (TZrUInt32)sizeof(SZrTypeValue) * 2u, (TZrUInt32)sizeof(TZrUInt32),
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE, ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE,
            fields, ZR_ARRAY_COUNT(fields), &contract);
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
    RUN_TEST(test_domain_transfer_kind_is_canonical_layout_identity);
    RUN_TEST(test_explicit_gc_map_rejects_in_bounds_non_gc_slot);
    RUN_TEST(test_explicit_ownership_map_rejects_in_bounds_non_owner_slot);
    RUN_TEST(test_explicit_ref_map_rejects_in_bounds_non_ref_slot);
    RUN_TEST(test_explicit_gc_map_rejects_duplicate_and_missing_field);
    RUN_TEST(test_explicit_ownership_map_rejects_duplicate_and_missing_field);
    RUN_TEST(test_explicit_ref_map_rejects_duplicate_and_missing_field);
    RUN_TEST(test_explicit_maps_allow_permutation_and_null_fallback);
    RUN_TEST(test_union_explicit_maps_preserve_legal_overlap_and_active_scan);
    RUN_TEST(test_union_explicit_map_rejects_wrong_overlap_multiplicity);
    return UNITY_END();
}
