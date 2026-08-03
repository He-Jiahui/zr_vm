#include "unity.h"

#include <stddef.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/type_layout.h"
#include "zr_vm_core/value.h"
#include "zr_vm_lib_container/generational_pool.h"

typedef struct SCanonicalPoolProbe {
    TZrUInt32 dropCount;
    TZrUInt32 visitCount;
} SCanonicalPoolProbe;

typedef struct SCanonicalPoolValue {
    TZrInt32 value;
} SCanonicalPoolValue;

typedef struct SCanonicalNestedManagedValue {
    SZrTypeValue directValue;
    SZrTypeValue nestedValue;
} SCanonicalNestedManagedValue;

static void canonical_pool_record_drop(
        SZrState *state,
        TZrPtr storage,
        TZrPtr userData) {
    SCanonicalPoolProbe *probe = (SCanonicalPoolProbe *)userData;

    ZR_UNUSED_PARAMETER(state);
    ZR_UNUSED_PARAMETER(storage);
    if (probe != ZR_NULL) {
        probe->dropCount++;
    }
}

static void canonical_pool_record_visit(
        SZrState *state,
        SZrTypeValue *value,
        TZrPtr userData) {
    SCanonicalPoolProbe *probe = (SCanonicalPoolProbe *)userData;

    ZR_UNUSED_PARAMETER(state);
    TEST_ASSERT_NOT_NULL(value);
    if (probe != ZR_NULL) {
        probe->visitCount++;
    }
}

static SZrPoolConfig canonical_pool_config(void) {
    SZrPoolConfig config;

    memset(&config, 0, sizeof(config));
    config.slabCapacity = 2u;
    config.generationLimit = UINT64_MAX;
    config.concurrencyMode = ZR_POOL_CONCURRENCY_THREAD_LOCAL;
    return config;
}

static void test_canonical_gcfree_layout_defers_exactly_once_drop(void) {
    SCanonicalPoolProbe probe = {0};
    SZrTypeLayoutContract contract;
    SZrTypeLayout layout;
    SZrPoolConfig config = canonical_pool_config();
    SZrPool *pool = ZR_NULL;
    SZrPoolHandle handle;
    SZrPoolGuard guard = {0};
    SZrPoolStats stats;
    SCanonicalPoolValue source = {41};
    uint64_t scannedSlots = UINT64_MAX;
    uint64_t scannedBytes = UINT64_MAX;

    memset(&contract, 0, sizeof(contract));
    contract.gcScanKind = ZR_TYPE_LAYOUT_GC_SCAN_FREE;
    contract.customDrop = canonical_pool_record_drop;
    contract.customDropUserData = &probe;
    ZrCore_TypeLayout_InitStructWithContract(
            &layout,
            (TZrUInt32)sizeof(source),
            (TZrUInt32)_Alignof(SCanonicalPoolValue),
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_CUSTOM_THEN_FIELDS,
            ZR_NULL,
            0u,
            &contract);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&layout));

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_CreateFromTypeLayout(
                    ZR_NULL,
                    &layout,
                    ZR_NULL,
                    ZR_NULL,
                    ZR_NULL,
                    &config,
                    &pool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Deliver(pool, &source, &handle));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_TryRead(pool, handle, &guard));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Recycle(pool, handle));
    TEST_ASSERT_EQUAL_UINT32(0u, probe.dropCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Scan(pool, &scannedSlots, &scannedBytes));
    TEST_ASSERT_EQUAL_UINT64(0u, scannedSlots);
    TEST_ASSERT_EQUAL_UINT64(0u, scannedBytes);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPoolGuard_Release(&guard));
    TEST_ASSERT_EQUAL_UINT32(1u, probe.dropCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_GetStats(pool, &stats));
    TEST_ASSERT_EQUAL_UINT64(1u, stats.dropCount);
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    TEST_ASSERT_EQUAL_UINT32(1u, probe.dropCount);
}

static void test_canonical_mapped_layout_drives_scan_visitor(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SCanonicalPoolProbe probe = {0};
    SCanonicalPoolProbe dynamicProbe = {0};
    SZrTypeLayout layout;
    SZrPoolConfig config = canonical_pool_config();
    SZrPool *pool = ZR_NULL;
    SZrPoolHandle handles[2];
    SZrPoolGuard retiredGuard = {0};
    SZrTypeValue values[2];
    uint64_t scannedSlots = 0u;
    uint64_t scannedBytes = 0u;

    TEST_ASSERT_NOT_NULL(state);
    ZrCore_TypeLayout_InitValue(&layout);
    TEST_ASSERT_EQUAL_INT(
            ZR_TYPE_LAYOUT_GC_SCAN_MAPPED, layout.gcScanKind);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_CreateFromTypeLayout(
                    state,
                    &layout,
                    ZR_NULL,
                    canonical_pool_record_visit,
                    &probe,
                    &config,
                    &pool));
    for (TZrUInt32 index = 0u; index < ZR_ARRAY_COUNT(values); index++) {
        ZrCore_Value_InitAsInt(state, &values[index], (TZrInt64)index + 1);
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_OK,
                ZrPool_Deliver(pool, &values[index], &handles[index]));
    }
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_TraceGcValues(
                    pool,
                    canonical_pool_record_visit,
                    &dynamicProbe,
                    &scannedSlots,
                    &scannedBytes));
    TEST_ASSERT_EQUAL_UINT64(2u, scannedSlots);
    TEST_ASSERT_EQUAL_UINT64(2u * sizeof(SZrTypeValue), scannedBytes);
    TEST_ASSERT_EQUAL_UINT32(2u, dynamicProbe.visitCount);
    TEST_ASSERT_EQUAL_UINT32(0u, probe.visitCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Scan(pool, &scannedSlots, &scannedBytes));
    TEST_ASSERT_EQUAL_UINT64(2u, scannedSlots);
    TEST_ASSERT_EQUAL_UINT64(2u * sizeof(SZrTypeValue), scannedBytes);
    TEST_ASSERT_EQUAL_UINT32(2u, probe.visitCount);

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_TryRead(pool, handles[0], &retiredGuard));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Recycle(pool, handles[0]));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Recycle(pool, handles[1]));
    memset(&dynamicProbe, 0, sizeof(dynamicProbe));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_TraceGcValues(
                    pool,
                    canonical_pool_record_visit,
                    &dynamicProbe,
                    &scannedSlots,
                    &scannedBytes));
    TEST_ASSERT_EQUAL_UINT64(1u, scannedSlots);
    TEST_ASSERT_EQUAL_UINT32(1u, dynamicProbe.visitCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPoolGuard_Release(&retiredGuard));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_TraceGcValues(
                    pool,
                    canonical_pool_record_visit,
                    &dynamicProbe,
                    &scannedSlots,
                    &scannedBytes));
    TEST_ASSERT_EQUAL_UINT64(0u, scannedSlots);
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    ZrTests_Runtime_State_Destroy(state);
}

static void test_canonical_managed_layout_requires_scan_visitor(void) {
    SZrTypeLayout layout;
    SZrPool *pool = (SZrPool *)(uintptr_t)1u;

    ZrCore_TypeLayout_InitValue(&layout);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_INVALID_ARGUMENT,
            ZrPool_CreateFromTypeLayout(
                    ZR_NULL,
                    &layout,
                    ZR_NULL,
                    ZR_NULL,
                    ZR_NULL,
                    ZR_NULL,
                    &pool));
    TEST_ASSERT_NULL(pool);
}

static void test_canonical_managed_layout_requires_runtime_state(void) {
    SZrTypeLayoutField field;
    SZrTypeLayout layout;
    SZrPool *pool = (SZrPool *)(uintptr_t)1u;
    EZrPoolStatus status;

    ZrCore_TypeLayout_InitValue(&layout);
    status = ZrPool_CreateFromTypeLayout(
            ZR_NULL,
            &layout,
            ZR_NULL,
            canonical_pool_record_visit,
            ZR_NULL,
            ZR_NULL,
            &pool);
    if (pool != ZR_NULL && pool != (SZrPool *)(uintptr_t)1u) {
        TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    }
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_INVALID_ARGUMENT, status);
    TEST_ASSERT_NULL(pool);

    memset(&field, 0, sizeof(field));
    field.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    field.flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                  ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                  ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
    ZrCore_TypeLayout_InitStruct(
            &layout,
            field.byteSize,
            (TZrUInt32)_Alignof(SZrTypeValue),
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE,
            &field,
            1u);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&layout));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_INVALID_ARGUMENT,
            ZrPool_CreateFromTypeLayout(
                    ZR_NULL,
                    &layout,
                    ZR_NULL,
                    canonical_pool_record_visit,
                    ZR_NULL,
                    ZR_NULL,
                    &pool));
    TEST_ASSERT_NULL(pool);
}

static void test_canonical_copy_error_rolls_back_without_publishing(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeLayout layout;
    SZrPoolConfig config = canonical_pool_config();
    SZrPool *pool = ZR_NULL;
    SZrPoolHandle handle = {0};
    SZrPoolStats stats;
    SZrTypeValue source;

    TEST_ASSERT_NOT_NULL(state);
    ZrCore_TypeLayout_InitValue(&layout);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_CreateFromTypeLayout(
                    state,
                    &layout,
                    ZR_NULL,
                    canonical_pool_record_visit,
                    ZR_NULL,
                    &config,
                    &pool));
    ZrCore_Value_InitAsInt(state, &source, 73);
    state->threadStatus = ZR_THREAD_STATUS_MEMORY_ERROR;
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_CONSTRUCTION_FAILED,
            ZrPool_Deliver(pool, &source, &handle));
    state->threadStatus = ZR_THREAD_STATUS_FINE;
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_GetStats(pool, &stats));
    TEST_ASSERT_EQUAL_UINT64(1u, stats.constructionFailureCount);
    TEST_ASSERT_EQUAL_UINT64(1u, stats.partialCleanupCount);
    TEST_ASSERT_EQUAL_UINT64(0u, stats.deliverCount);
    TEST_ASSERT_EQUAL_UINT64(0u, stats.liveCount);
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    ZrTests_Runtime_State_Destroy(state);
}

static void test_canonical_stateful_layout_rejects_concurrent_mode(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeLayout layout;
    SZrPoolConfig config = canonical_pool_config();
    SZrPool *pool = (SZrPool *)(uintptr_t)1u;
    EZrPoolStatus status;

    TEST_ASSERT_NOT_NULL(state);
    config.concurrencyMode = ZR_POOL_CONCURRENCY_CONCURRENT;
    ZrCore_TypeLayout_InitValue(&layout);
    status = ZrPool_CreateFromTypeLayout(
            state,
            &layout,
            ZR_NULL,
            canonical_pool_record_visit,
            ZR_NULL,
            &config,
            &pool);
    if (pool != ZR_NULL && pool != (SZrPool *)(uintptr_t)1u) {
        TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    }
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_INVALID_ARGUMENT, status);
    TEST_ASSERT_NULL(pool);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_canonical_layout_rejects_missing_copy_path(void) {
    SCanonicalPoolProbe probe = {0};
    SZrTypeLayoutContract contract;
    SZrTypeLayout layout;
    SZrPool *pool = (SZrPool *)(uintptr_t)1u;

    memset(&contract, 0, sizeof(contract));
    contract.customDrop = canonical_pool_record_drop;
    contract.customDropUserData = &probe;
    ZrCore_TypeLayout_InitStructWithContract(
            &layout,
            (TZrUInt32)sizeof(SCanonicalPoolValue),
            (TZrUInt32)_Alignof(SCanonicalPoolValue),
            ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_CUSTOM_THEN_FIELDS,
            ZR_NULL,
            0u,
            &contract);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&layout));
    TEST_ASSERT_FALSE(ZrCore_TypeLayout_CanRawCopy(&layout));

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_INVALID_ARGUMENT,
            ZrPool_CreateFromTypeLayout(
                    ZR_NULL,
                    &layout,
                    ZR_NULL,
                    ZR_NULL,
                    ZR_NULL,
                    ZR_NULL,
                    &pool));
    TEST_ASSERT_NULL(pool);
}

static void test_canonical_layout_rejects_dangling_nested_registry(void) {
    SZrTypeLayoutField field;
    SZrTypeLayout layout;
    SZrPool *pool = (SZrPool *)(uintptr_t)1u;

    memset(&field, 0, sizeof(field));
    field.byteSize = (TZrUInt32)sizeof(SCanonicalPoolValue);
    field.typeLayoutIndex = 1u;
    field.flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NESTED_LAYOUT;
    ZrCore_TypeLayout_InitStruct(
            &layout,
            field.byteSize,
            (TZrUInt32)_Alignof(SCanonicalPoolValue),
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE,
            &field,
            1u);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&layout));

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_INVALID_ARGUMENT,
            ZrPool_CreateFromTypeLayout(
                    ZR_NULL,
                    &layout,
                    ZR_NULL,
                    ZR_NULL,
                    ZR_NULL,
                    ZR_NULL,
                    &pool));
    TEST_ASSERT_NULL(pool);
}

static void test_canonical_layout_rejects_nested_scan_drop_downgrade(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeLayoutField field;
    SZrTypeLayout nestedLayout;
    SZrTypeLayout layout;
    const SZrTypeLayout *layouts[1];
    SZrTypeLayoutRegistryView registry;
    SZrPool *pool = (SZrPool *)(uintptr_t)1u;
    EZrPoolStatus status;

    TEST_ASSERT_NOT_NULL(state);
    ZrCore_TypeLayout_InitValue(&nestedLayout);
    layouts[0] = &nestedLayout;
    registry.layouts = layouts;
    registry.count = ZR_ARRAY_COUNT(layouts);
    memset(&field, 0, sizeof(field));
    field.byteSize = nestedLayout.byteSize;
    field.typeLayoutIndex = 0u;
    field.flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NESTED_LAYOUT;
    ZrCore_TypeLayout_InitStruct(
            &layout,
            field.byteSize,
            nestedLayout.byteAlign,
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            &field,
            1u);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&layout));
    TEST_ASSERT_EQUAL_INT(ZR_TYPE_LAYOUT_GC_SCAN_FREE, layout.gcScanKind);

    status = ZrPool_CreateFromTypeLayout(
            state,
            &layout,
            &registry,
            ZR_NULL,
            ZR_NULL,
            ZR_NULL,
            &pool);
    if (pool != ZR_NULL && pool != (SZrPool *)(uintptr_t)1u) {
        TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    }
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_INVALID_ARGUMENT, status);
    TEST_ASSERT_NULL(pool);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_canonical_layout_rejects_raw_root_over_move_only_nested(void) {
    SZrTypeLayoutField field;
    SZrTypeLayout nestedLayout;
    SZrTypeLayout layout;
    const SZrTypeLayout *layouts[1];
    SZrTypeLayoutRegistryView registry;
    SZrPool *pool = (SZrPool *)(uintptr_t)1u;
    EZrPoolStatus status;

    ZrCore_TypeLayout_InitStruct(
            &nestedLayout,
            (TZrUInt32)sizeof(SCanonicalPoolValue),
            (TZrUInt32)_Alignof(SCanonicalPoolValue),
            ZR_TYPE_LAYOUT_COPY_KIND_MOVE_ONLY,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            ZR_NULL,
            0u);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&nestedLayout));
    layouts[0] = &nestedLayout;
    registry.layouts = layouts;
    registry.count = ZR_ARRAY_COUNT(layouts);
    memset(&field, 0, sizeof(field));
    field.byteSize = nestedLayout.byteSize;
    field.typeLayoutIndex = 0u;
    field.flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NESTED_LAYOUT;
    ZrCore_TypeLayout_InitStruct(
            &layout,
            field.byteSize,
            nestedLayout.byteAlign,
            ZR_TYPE_LAYOUT_COPY_KIND_BITWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            &field,
            1u);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&layout));
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_CanRawCopy(&layout));

    status = ZrPool_CreateFromTypeLayout(
            ZR_NULL,
            &layout,
            &registry,
            ZR_NULL,
            ZR_NULL,
            ZR_NULL,
            &pool);
    if (pool != ZR_NULL && pool != (SZrPool *)(uintptr_t)1u) {
        TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    }
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_INVALID_ARGUMENT, status);
    TEST_ASSERT_NULL(pool);
}

static void test_canonical_nested_managed_layout_scans_direct_and_nested_values(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SCanonicalPoolProbe probe = {0};
    SZrTypeLayoutField fields[2];
    SZrTypeLayout nestedLayout;
    SZrTypeLayout layout;
    const SZrTypeLayout *layouts[1];
    SZrTypeLayoutRegistryView registry;
    SZrPoolConfig config = canonical_pool_config();
    SZrPool *pool = ZR_NULL;
    SZrPoolHandle handle;
    SCanonicalNestedManagedValue source;
    uint64_t scannedSlots = 0u;
    uint64_t scannedBytes = 0u;

    TEST_ASSERT_NOT_NULL(state);
    ZrCore_TypeLayout_InitValue(&nestedLayout);
    layouts[0] = &nestedLayout;
    registry.layouts = layouts;
    registry.count = ZR_ARRAY_COUNT(layouts);
    memset(fields, 0, sizeof(fields));
    fields[0].byteOffset = (TZrUInt32)offsetof(
            SCanonicalNestedManagedValue, directValue);
    fields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    fields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                      ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                      ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
    fields[1].byteOffset = (TZrUInt32)offsetof(
            SCanonicalNestedManagedValue, nestedValue);
    fields[1].byteSize = nestedLayout.byteSize;
    fields[1].typeLayoutIndex = 0u;
    fields[1].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NESTED_LAYOUT;
    ZrCore_TypeLayout_InitStruct(
            &layout,
            (TZrUInt32)sizeof(source),
            (TZrUInt32)_Alignof(SCanonicalNestedManagedValue),
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE,
            fields,
            ZR_ARRAY_COUNT(fields));
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&layout));
    TEST_ASSERT_EQUAL_INT(ZR_TYPE_LAYOUT_GC_SCAN_MAPPED, layout.gcScanKind);
    TEST_ASSERT_NULL(layout.gcFieldOffsets);

    ZrCore_Value_InitAsInt(state, &source.directValue, 11);
    ZrCore_Value_InitAsInt(state, &source.nestedValue, 17);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_CreateFromTypeLayout(
                    state,
                    &layout,
                    &registry,
                    canonical_pool_record_visit,
                    &probe,
                    &config,
                    &pool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Deliver(pool, &source, &handle));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Scan(pool, &scannedSlots, &scannedBytes));
    TEST_ASSERT_EQUAL_UINT32(2u, probe.visitCount);
    TEST_ASSERT_EQUAL_UINT64(1u, scannedSlots);
    TEST_ASSERT_EQUAL_UINT64(sizeof(source), scannedBytes);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Recycle(pool, handle));
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    ZrTests_Runtime_State_Destroy(state);
}

static void test_canonical_nested_custom_drop_runs_exactly_once(void) {
    SCanonicalPoolProbe probe = {0};
    SZrTypeLayoutContract contract;
    SZrTypeLayoutField field;
    SZrTypeLayout nestedLayout;
    SZrTypeLayout layout;
    const SZrTypeLayout *layouts[1];
    SZrTypeLayoutRegistryView registry;
    SZrPoolConfig config = canonical_pool_config();
    SZrPool *pool = ZR_NULL;
    SZrPoolHandle handle;
    SCanonicalPoolValue source = {29};

    memset(&contract, 0, sizeof(contract));
    contract.customDrop = canonical_pool_record_drop;
    contract.customDropUserData = &probe;
    ZrCore_TypeLayout_InitStructWithContract(
            &nestedLayout,
            (TZrUInt32)sizeof(source),
            (TZrUInt32)_Alignof(SCanonicalPoolValue),
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_CUSTOM_THEN_FIELDS,
            ZR_NULL,
            0u,
            &contract);
    layouts[0] = &nestedLayout;
    registry.layouts = layouts;
    registry.count = ZR_ARRAY_COUNT(layouts);
    memset(&field, 0, sizeof(field));
    field.byteSize = nestedLayout.byteSize;
    field.typeLayoutIndex = 0u;
    field.flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NESTED_LAYOUT;
    ZrCore_TypeLayout_InitStruct(
            &layout,
            field.byteSize,
            nestedLayout.byteAlign,
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE,
            &field,
            1u);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&layout));

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_CreateFromTypeLayout(
                    ZR_NULL,
                    &layout,
                    &registry,
                    ZR_NULL,
                    ZR_NULL,
                    &config,
                    &pool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Deliver(pool, &source, &handle));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Recycle(pool, handle));
    TEST_ASSERT_EQUAL_UINT32(1u, probe.dropCount);
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    TEST_ASSERT_EQUAL_UINT32(1u, probe.dropCount);
}

static void test_canonical_layout_rejects_owned_value_without_drop(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeLayoutField field;
    SZrTypeLayout layout;
    SZrPool *pool = (SZrPool *)(uintptr_t)1u;
    EZrPoolStatus status;

    TEST_ASSERT_NOT_NULL(state);
    memset(&field, 0, sizeof(field));
    field.byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    field.flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                  ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE;
    ZrCore_TypeLayout_InitStruct(
            &layout,
            field.byteSize,
            (TZrUInt32)_Alignof(SZrTypeValue),
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_NONE,
            &field,
            1u);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&layout));

    status = ZrPool_CreateFromTypeLayout(
            state,
            &layout,
            ZR_NULL,
            ZR_NULL,
            ZR_NULL,
            ZR_NULL,
            &pool);
    if (pool != ZR_NULL && pool != (SZrPool *)(uintptr_t)1u) {
        TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    }
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_INVALID_ARGUMENT, status);
    TEST_ASSERT_NULL(pool);
    ZrTests_Runtime_State_Destroy(state);
}

static void test_canonical_layout_rejects_offset_table_over_managed_nested(void) {
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrTypeLayoutContract contract;
    SZrTypeLayoutField fields[2];
    TZrUInt32 gcOffsets[1];
    SZrTypeLayout nestedLayout;
    SZrTypeLayout layout;
    const SZrTypeLayout *layouts[1];
    SZrTypeLayoutRegistryView registry;
    SZrPool *pool = (SZrPool *)(uintptr_t)1u;
    EZrPoolStatus status;

    TEST_ASSERT_NOT_NULL(state);
    ZrCore_TypeLayout_InitValue(&nestedLayout);
    layouts[0] = &nestedLayout;
    registry.layouts = layouts;
    registry.count = ZR_ARRAY_COUNT(layouts);
    memset(fields, 0, sizeof(fields));
    fields[0].byteSize = (TZrUInt32)sizeof(SZrTypeValue);
    fields[0].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                      ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE;
    fields[1].byteOffset = fields[0].byteSize;
    fields[1].byteSize = nestedLayout.byteSize;
    fields[1].typeLayoutIndex = 0u;
    fields[1].flags = ZR_TYPE_LAYOUT_FIELD_FLAG_NESTED_LAYOUT;
    gcOffsets[0] = fields[0].byteOffset;
    memset(&contract, 0, sizeof(contract));
    contract.gcScanKind = ZR_TYPE_LAYOUT_GC_SCAN_MAPPED;
    contract.gcFieldOffsets = gcOffsets;
    contract.gcFieldCount = ZR_ARRAY_COUNT(gcOffsets);
    ZrCore_TypeLayout_InitStructWithContract(
            &layout,
            fields[0].byteSize + fields[1].byteSize,
            (TZrUInt32)_Alignof(SZrTypeValue),
            ZR_TYPE_LAYOUT_COPY_KIND_FIELDWISE,
            ZR_TYPE_LAYOUT_DROP_KIND_FIELDWISE,
            fields,
            ZR_ARRAY_COUNT(fields),
            &contract);
    TEST_ASSERT_TRUE(ZrCore_TypeLayout_Validate(&layout));
    TEST_ASSERT_NOT_NULL(layout.gcFieldOffsets);

    status = ZrPool_CreateFromTypeLayout(
            state,
            &layout,
            &registry,
            canonical_pool_record_visit,
            ZR_NULL,
            ZR_NULL,
            &pool);
    if (pool != ZR_NULL && pool != (SZrPool *)(uintptr_t)1u) {
        TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    }
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_INVALID_ARGUMENT, status);
    TEST_ASSERT_NULL(pool);
    ZrTests_Runtime_State_Destroy(state);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_canonical_gcfree_layout_defers_exactly_once_drop);
    RUN_TEST(test_canonical_mapped_layout_drives_scan_visitor);
    RUN_TEST(test_canonical_managed_layout_requires_scan_visitor);
    RUN_TEST(test_canonical_managed_layout_requires_runtime_state);
    RUN_TEST(test_canonical_copy_error_rolls_back_without_publishing);
    RUN_TEST(test_canonical_stateful_layout_rejects_concurrent_mode);
    RUN_TEST(test_canonical_layout_rejects_missing_copy_path);
    RUN_TEST(test_canonical_layout_rejects_dangling_nested_registry);
    RUN_TEST(test_canonical_layout_rejects_nested_scan_drop_downgrade);
    RUN_TEST(test_canonical_layout_rejects_raw_root_over_move_only_nested);
    RUN_TEST(test_canonical_nested_managed_layout_scans_direct_and_nested_values);
    RUN_TEST(test_canonical_nested_custom_drop_runs_exactly_once);
    RUN_TEST(test_canonical_layout_rejects_owned_value_without_drop);
    RUN_TEST(test_canonical_layout_rejects_offset_table_over_managed_nested);
    return UNITY_END();
}
