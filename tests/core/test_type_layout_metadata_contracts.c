#include "unity.h"

#include <string.h>

#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"

/* Unity 套件直接核对布局构造、身份校验及显式映射的约束；
 * GC 访问用栈上值槽记录偏移，不执行对象收集或跨域传递。 */
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

/* 旧式 metadata 入口借用两张栈上偏移表，计数仍由字段标记推导；
 * 本例只检查记录结果，不将这组字段跨度作为有效布局的证明。 */
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

/* 故意保留非零 fieldCount 与空表，只核构造器不解引用空表；
 * 不把构造完成当成 Validate 成功。 */
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

/* 无托管字段也不能绕过 MOVE_ONLY；字节缓冲仅用于复制拒绝路径。 */
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

/* 前两组描述符用于拒绝无效跨度或显式映射；不隔离每个拒绝原因。
 * 随后清空显式表，靠字段推导 GC 映射建立有效基线，再分别篡改 hash 与版本。 */
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

/* 同步 GC 遍历的借用记录；storage 与本记录均须活到遍历返回，
 * count 可超过 offsets 容量，未保存的尾项仍计入总次数。 */
typedef struct SExplicitMapVisitRecord {
    SZrTypeValue *storage;
    TZrUInt32 count;
    TZrUInt32 offsets[4];
} SExplicitMapVisitRecord;

/* 访问器只记录相对同一栈数组的字节偏移；count 计全部调用，
 * offsets 最多保留前四项，使额外访问仍能被次数断言发现。 */
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

/* 三个映射类别共用相同字段形状，以隔离显式表与相应字段标记的匹配。
 * 调用仅传 GC/OWNERSHIP/REF 三种 flag；字段和偏移表由用例栈保有，
 * 临时 contract 在返回后无须保留，布局借用的是其中指向的表。 */
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

/* 偏移仍在真实值槽内，但指向未带目标标记的第二槽；防止只检查边界
 * 与数量而接受错误映射。三种 flag 共用该反例。 */
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
    /* 仅当回归使错误 GC 表被接受时记录错误访问，帮助定位拒绝断言的失败；
     * storage 是对齐的真实值槽，不调用收集器，也不释放或复制槽中对象。 */
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

/* 两字段都有目标标记，但显式表重复第一槽并遗漏第二槽；
 * 计数相等仍须拒绝，不能以逐项落在某个合法字段内代替完整覆盖。 */
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

/* 三类显式映射都允许颠倒顺序，也允许空表由字段推导；
 * 仅 GC 类别执行访问器，分别核对显式表顺序与字段回退顺序。 */
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

/* 两个变体合法共享偏移，三张表都保留重复项；tag 放在载荷之后。
 * GC 遍历仍只报告 tag=1 的活动成员，不能把显式表的两项都当作根。 */
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

/* 联合字段的偏移多重集为 {0,0,一槽大小}，表却为 {0,一槽大小,一槽大小}；
 * 同样数量与合法偏移不能掩盖各偏移出现次数失配。 */
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

/* CTest 通过套件脚本启动本可执行文件；注册全部用例，并以 Unity 汇总结果退出。 */
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
