#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "unity.h"

#include "zr_vm_core/raw_object.h"
#include "zr_vm_lib_container/generational_pool.h"

#define ZR_M5_ELEMENT_COUNT ((TZrSize)65536u)
#define ZR_M5_SLAB_CAPACITY ((TZrSize)256u)
#define ZR_M5_SCAN_PASS_COUNT ((TZrSize)8u)
#define ZR_M5_VALIDATION_COUNT ((TZrSize)1000000u)

typedef struct SM5PoolElement {
    uint64_t identity;
    uint64_t payload[3];
} SM5PoolElement;

typedef struct SM5PerItemClassStorage {
    SZrRawObject header;
    SM5PoolElement value;
} SM5PerItemClassStorage;

typedef struct SM5ScanProbe {
    uint64_t scanCount;
    uint64_t checksum;
} SM5ScanProbe;

static void m5_scan_element(void *element, void *context) {
    const SM5PoolElement *value = (const SM5PoolElement *)element;
    SM5ScanProbe *probe = (SM5ScanProbe *)context;

    probe->scanCount++;
    probe->checksum += value->identity + value->payload[0];
}

static SZrPoolTypeLayout m5_layout(
        EZrPoolGcScanKind scanKind,
        SM5ScanProbe *probe) {
    SZrPoolTypeLayout layout;

    memset(&layout, 0, sizeof(layout));
    layout.elementSize = sizeof(SM5PoolElement);
    layout.elementAlignment = _Alignof(SM5PoolElement);
    layout.gcScanKind = scanKind;
    layout.scan = scanKind == ZR_POOL_GC_SCAN_FREE ? ZR_NULL : m5_scan_element;
    layout.context = probe;
    return layout;
}

static SZrPoolConfig m5_config(EZrPoolConcurrencyMode concurrencyMode) {
    SZrPoolConfig config;

    config.slabCapacity = ZR_M5_SLAB_CAPACITY;
    config.generationLimit = UINT64_MAX;
    config.concurrencyMode = concurrencyMode;
    return config;
}

static clock_t m5_scan_pool(
        SZrPool *pool,
        uint64_t *outSlots,
        uint64_t *outBytes) {
    clock_t start = clock();

    *outSlots = 0u;
    *outBytes = 0u;
    for (TZrSize pass = 0u; pass < ZR_M5_SCAN_PASS_COUNT; pass++) {
        uint64_t scannedSlots = 0u;
        uint64_t scannedBytes = 0u;

        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_OK,
                ZrPool_Scan(pool, &scannedSlots, &scannedBytes));
        *outSlots += scannedSlots;
        *outBytes += scannedBytes;
    }
    return clock() - start;
}

static clock_t m5_scan_per_item_baseline(
        SM5PerItemClassStorage **objects,
        uint64_t *outVisits,
        uint64_t *outObjectBytes,
        uint64_t *outChecksum) {
    clock_t start = clock();
    uint64_t checksum = 0u;

    *outVisits = 0u;
    *outObjectBytes = 0u;
    for (TZrSize pass = 0u; pass < ZR_M5_SCAN_PASS_COUNT; pass++) {
        for (TZrSize index = 0u; index < ZR_M5_ELEMENT_COUNT; index++) {
            checksum += objects[index]->value.identity;
            (*outVisits)++;
            *outObjectBytes += sizeof(*objects[index]);
        }
    }
    *outChecksum = checksum;
    return clock() - start;
}

static void test_m5_allocation_and_gc_pause_work_matrix(void) {
    const uint64_t expectedSlabAllocations =
            (ZR_M5_ELEMENT_COUNT + ZR_M5_SLAB_CAPACITY - 1u) /
            ZR_M5_SLAB_CAPACITY;
    const uint64_t expectedScanVisits =
            ZR_M5_ELEMENT_COUNT * ZR_M5_SCAN_PASS_COUNT;
    const uint64_t expectedMappedBytes =
            expectedScanVisits * sizeof(SM5PoolElement);
    SM5ScanProbe freeProbe = {0};
    SM5ScanProbe mappedProbe = {0};
    SZrPoolTypeLayout freeLayout = m5_layout(
            ZR_POOL_GC_SCAN_FREE, &freeProbe);
    SZrPoolTypeLayout mappedLayout = m5_layout(
            ZR_POOL_GC_SCAN_MAPPED, &mappedProbe);
    SZrPoolConfig config = m5_config(ZR_POOL_CONCURRENCY_THREAD_LOCAL);
    SZrPool *freePool = ZR_NULL;
    SZrPool *mappedPool = ZR_NULL;
    SM5PerItemClassStorage **perItemObjects;
    SZrPoolStats freeStats;
    SZrPoolStats mappedStats;
    uint64_t freeSlots;
    uint64_t freeBytes;
    uint64_t mappedSlots;
    uint64_t mappedBytes;
    uint64_t perItemVisits;
    uint64_t perItemObjectBytes;
    uint64_t perItemChecksum;
    clock_t freeTicks;
    clock_t mappedTicks;
    clock_t perItemTicks;

    perItemObjects = (SM5PerItemClassStorage **)calloc(
            ZR_M5_ELEMENT_COUNT, sizeof(*perItemObjects));
    TEST_ASSERT_NOT_NULL(perItemObjects);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Create(&freeLayout, &config, &freePool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Create(&mappedLayout, &config, &mappedPool));

    for (TZrSize index = 0u; index < ZR_M5_ELEMENT_COUNT; index++) {
        SM5PoolElement value = {
                (uint64_t)index + 1u,
                {(uint64_t)index ^ UINT64_C(0x5a5a), 0u, 0u}};
        SZrPoolHandle handle;

        perItemObjects[index] = (SM5PerItemClassStorage *)calloc(
                1u, sizeof(*perItemObjects[index]));
        TEST_ASSERT_NOT_NULL(perItemObjects[index]);
        perItemObjects[index]->value = value;
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_OK, ZrPool_Deliver(freePool, &value, &handle));
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_OK, ZrPool_Deliver(mappedPool, &value, &handle));
    }

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_GetStats(freePool, &freeStats));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_GetStats(mappedPool, &mappedStats));
    TEST_ASSERT_EQUAL_UINT64(
            expectedSlabAllocations, freeStats.slabAllocationCount);
    TEST_ASSERT_EQUAL_UINT64(
            expectedSlabAllocations, mappedStats.slabAllocationCount);
    TEST_ASSERT_LESS_THAN_UINT64(
            ZR_M5_ELEMENT_COUNT, freeStats.slabAllocationCount);
    TEST_ASSERT_LESS_THAN_UINT64(
            ZR_M5_ELEMENT_COUNT, mappedStats.slabAllocationCount);

    freeTicks = m5_scan_pool(freePool, &freeSlots, &freeBytes);
    mappedTicks = m5_scan_pool(mappedPool, &mappedSlots, &mappedBytes);
    perItemTicks = m5_scan_per_item_baseline(
            perItemObjects,
            &perItemVisits,
            &perItemObjectBytes,
            &perItemChecksum);

    TEST_ASSERT_EQUAL_UINT64(0u, freeSlots);
    TEST_ASSERT_EQUAL_UINT64(0u, freeBytes);
    TEST_ASSERT_EQUAL_UINT64(0u, freeProbe.scanCount);
    TEST_ASSERT_EQUAL_UINT64(expectedScanVisits, mappedSlots);
    TEST_ASSERT_EQUAL_UINT64(expectedMappedBytes, mappedBytes);
    TEST_ASSERT_EQUAL_UINT64(expectedScanVisits, mappedProbe.scanCount);
    TEST_ASSERT_EQUAL_UINT64(expectedScanVisits, perItemVisits);
    TEST_ASSERT_GREATER_THAN_UINT64(mappedBytes, perItemObjectBytes);
    TEST_ASSERT_NOT_EQUAL(0u, mappedProbe.checksum);
    TEST_ASSERT_NOT_EQUAL(0u, perItemChecksum);

    printf(
            "M5_METRIC allocation_count per_item=%" PRIu64
            " gcfree_slabs=%" PRIu64 " gcmapped_slabs=%" PRIu64 "\n",
            (uint64_t)ZR_M5_ELEMENT_COUNT,
            freeStats.slabAllocationCount,
            mappedStats.slabAllocationCount);
    printf(
            "M5_METRIC gc_pause_ticks gcfree=%lld gcmapped=%lld per_item=%lld\n",
            (long long)freeTicks,
            (long long)mappedTicks,
            (long long)perItemTicks);
    printf(
            "M5_METRIC scan_work gcfree_slots=%" PRIu64
            " gcfree_bytes=%" PRIu64 " gcmapped_slots=%" PRIu64
            " gcmapped_bytes=%" PRIu64 " per_item_visits=%" PRIu64
            " per_item_object_bytes=%" PRIu64 "\n",
            freeSlots,
            freeBytes,
            mappedSlots,
            mappedBytes,
            perItemVisits,
            perItemObjectBytes);

    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&freePool));
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&mappedPool));
    for (TZrSize index = 0u; index < ZR_M5_ELEMENT_COUNT; index++) {
        free(perItemObjects[index]);
    }
    free(perItemObjects);
}

static clock_t m5_validate_handle(
        SZrPool *pool,
        SZrPoolHandle handle) {
    clock_t start = clock();

    for (TZrSize index = 0u; index < ZR_M5_VALIDATION_COUNT; index++) {
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_OK, ZrPool_Validate(pool, handle));
    }
    return clock() - start;
}

static void test_m5_thread_local_and_concurrent_modes_report_separately(void) {
    SM5ScanProbe probe = {0};
    SZrPoolTypeLayout layout = m5_layout(ZR_POOL_GC_SCAN_FREE, &probe);
    SZrPoolConfig threadLocalConfig = m5_config(
            ZR_POOL_CONCURRENCY_THREAD_LOCAL);
    SZrPoolConfig concurrentConfig = m5_config(
            ZR_POOL_CONCURRENCY_CONCURRENT);
    SZrPool *threadLocalPool = ZR_NULL;
    SZrPool *concurrentPool = ZR_NULL;
    SM5PoolElement value = {1u, {2u, 3u, 4u}};
    SZrPoolHandle threadLocalHandle;
    SZrPoolHandle concurrentHandle;
    SZrPoolStats threadLocalStats;
    SZrPoolStats concurrentStats;
    clock_t threadLocalTicks;
    clock_t concurrentTicks;

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Create(&layout, &threadLocalConfig, &threadLocalPool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Create(&layout, &concurrentConfig, &concurrentPool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Deliver(threadLocalPool, &value, &threadLocalHandle));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Deliver(concurrentPool, &value, &concurrentHandle));

    threadLocalTicks = m5_validate_handle(threadLocalPool, threadLocalHandle);
    concurrentTicks = m5_validate_handle(concurrentPool, concurrentHandle);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_GetStats(threadLocalPool, &threadLocalStats));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_GetStats(concurrentPool, &concurrentStats));
    TEST_ASSERT_EQUAL_UINT64(
            ZR_M5_VALIDATION_COUNT,
            threadLocalStats.handleValidationCount);
    TEST_ASSERT_EQUAL_UINT64(
            ZR_M5_VALIDATION_COUNT,
            concurrentStats.handleValidationCount);

    printf(
            "M5_METRIC concurrency_ticks validations=%" PRIu64
            " thread_local=%lld concurrent=%lld\n",
            (uint64_t)ZR_M5_VALIDATION_COUNT,
            (long long)threadLocalTicks,
            (long long)concurrentTicks);

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Destroy(&threadLocalPool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Destroy(&concurrentPool));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_m5_allocation_and_gc_pause_work_matrix);
    RUN_TEST(test_m5_thread_local_and_concurrent_modes_report_separately);
    return UNITY_END();
}
