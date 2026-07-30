#include <stdint.h>
#include <string.h>

#include "unity.h"

#include "zr_vm_lib_container/generational_pool.h"

typedef struct SPoolProbeValue {
    int value;
    int marker;
} SPoolProbeValue;

typedef struct SPoolProbe {
    uint64_t initializeCount;
    uint64_t abortInitializeCount;
    uint64_t dropCount;
    uint64_t scanCount;
} SPoolProbe;

static TZrBool initialize_probe(
        void *destination,
        const void *source,
        void *context) {
    SPoolProbe *probe = (SPoolProbe *)context;
    const SPoolProbeValue *value = (const SPoolProbeValue *)source;

    probe->initializeCount++;
    memcpy(destination, source, sizeof(*value));
    return value->value >= 0 ? ZR_TRUE : ZR_FALSE;
}

static void abort_initialize_probe(void *destination, void *context) {
    SPoolProbe *probe = (SPoolProbe *)context;
    SPoolProbeValue *value = (SPoolProbeValue *)destination;

    probe->abortInitializeCount++;
    value->value = 0;
    value->marker = 0;
}

static void drop_probe(void *element, void *context) {
    SPoolProbe *probe = (SPoolProbe *)context;
    SPoolProbeValue *value = (SPoolProbeValue *)element;

    probe->dropCount++;
    value->value = 0;
    value->marker = 0;
}

static void scan_probe(void *element, void *context) {
    SPoolProbe *probe = (SPoolProbe *)context;
    const SPoolProbeValue *value = (const SPoolProbeValue *)element;

    TEST_ASSERT_EQUAL_INT(0x5a5a, value->marker);
    probe->scanCount++;
}

static SZrPoolTypeLayout probe_layout(
        SPoolProbe *probe,
        EZrPoolGcScanKind scanKind) {
    SZrPoolTypeLayout layout;

    memset(&layout, 0, sizeof(layout));
    layout.elementSize = sizeof(SPoolProbeValue);
    layout.elementAlignment = 16u;
    layout.gcScanKind = scanKind;
    layout.initialize = initialize_probe;
    layout.abortInitialize = abort_initialize_probe;
    layout.drop = drop_probe;
    layout.scan = scanKind == ZR_POOL_GC_SCAN_FREE ? ZR_NULL : scan_probe;
    layout.context = probe;
    return layout;
}

static SZrPoolConfig probe_config(TZrSize slabCapacity) {
    SZrPoolConfig config;

    config.slabCapacity = slabCapacity;
    config.generationLimit = UINT64_MAX;
    config.concurrencyMode = ZR_POOL_CONCURRENCY_THREAD_LOCAL;
    return config;
}

static void test_partial_initialization_rolls_back_before_slot_reuse(void) {
    SPoolProbe probe = {0};
    SZrPoolTypeLayout layout = probe_layout(&probe, ZR_POOL_GC_SCAN_FREE);
    SZrPoolConfig config = probe_config(1u);
    SZrPool *pool = ZR_NULL;
    SZrPoolHandle rejectedHandle = {UINT64_MAX, SIZE_MAX, UINT64_MAX};
    SZrPoolHandle acceptedHandle = {0};
    SZrPoolStats stats;
    SPoolProbeValue rejected = {-1, 0x5a5a};
    SPoolProbeValue accepted = {7, 0x5a5a};

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Create(&layout, &config, &pool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_CONSTRUCTION_FAILED,
            ZrPool_Deliver(pool, &rejected, &rejectedHandle));
    TEST_ASSERT_EQUAL_UINT64(0u, rejectedHandle.poolId);
    TEST_ASSERT_EQUAL_UINT64(1u, probe.initializeCount);
    TEST_ASSERT_EQUAL_UINT64(1u, probe.abortInitializeCount);
    TEST_ASSERT_EQUAL_UINT64(0u, probe.dropCount);

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Deliver(pool, &accepted, &acceptedHandle));
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)acceptedHandle.slotIndex);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Recycle(pool, acceptedHandle));
    TEST_ASSERT_EQUAL_UINT64(1u, probe.dropCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_GetStats(pool, &stats));
    TEST_ASSERT_EQUAL_UINT64(1u, stats.constructionFailureCount);
    TEST_ASSERT_EQUAL_UINT64(1u, stats.partialCleanupCount);
    TEST_ASSERT_EQUAL_UINT64(1u, stats.dropCount);

    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
    TEST_ASSERT_EQUAL_UINT64(1u, probe.dropCount);
}

static void test_barriered_cards_and_scan_classes_report_separately(void) {
    SPoolProbe barrierProbe = {0};
    SPoolProbe mappedProbe = {0};
    SPoolProbe freeProbe = {0};
    SZrPoolTypeLayout barrierLayout = probe_layout(
            &barrierProbe, ZR_POOL_GC_SCAN_BARRIERED);
    SZrPoolTypeLayout mappedLayout = probe_layout(
            &mappedProbe, ZR_POOL_GC_SCAN_MAPPED);
    SZrPoolTypeLayout freeLayout = probe_layout(
            &freeProbe, ZR_POOL_GC_SCAN_FREE);
    SZrPoolConfig config = probe_config(2u);
    SZrPool *barrierPool = ZR_NULL;
    SZrPool *mappedPool = ZR_NULL;
    SZrPool *freePool = ZR_NULL;
    SZrPoolHandle barrierHandles[2];
    SZrPoolHandle mappedHandles[2];
    SZrPoolHandle freeHandle;
    SZrPoolGuard writer = {0};
    SZrPoolStats stats;
    uint64_t scannedSlots;
    uint64_t scannedBytes;

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Create(&barrierLayout, &config, &barrierPool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Create(&mappedLayout, &config, &mappedPool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Create(&freeLayout, &config, &freePool));
    for (int index = 0; index < 2; index++) {
        SPoolProbeValue value = {index, 0x5a5a};

        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_OK,
                ZrPool_Deliver(barrierPool, &value, &barrierHandles[index]));
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_OK,
                ZrPool_Deliver(mappedPool, &value, &mappedHandles[index]));
    }
    {
        SPoolProbeValue value = {3, 0x5a5a};
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_OK,
                ZrPool_Deliver(freePool, &value, &freeHandle));
    }

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_GetStats(barrierPool, &stats));
    TEST_ASSERT_EQUAL_UINT64(2u, (uint64_t)stats.dirtySlotCount);
    TEST_ASSERT_EQUAL_UINT64(2u, stats.barrierMarkCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Scan(barrierPool, &scannedSlots, &scannedBytes));
    TEST_ASSERT_EQUAL_UINT64(2u, scannedSlots);
    TEST_ASSERT_EQUAL_UINT64(2u * sizeof(SPoolProbeValue), scannedBytes);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Scan(barrierPool, &scannedSlots, &scannedBytes));
    TEST_ASSERT_EQUAL_UINT64(0u, scannedSlots);
    TEST_ASSERT_EQUAL_UINT64(0u, scannedBytes);

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_TryBorrow(barrierPool, barrierHandles[0], &writer));
    ((SPoolProbeValue *)ZrPoolGuard_Value(&writer))->value = 42;
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Scan(barrierPool, &scannedSlots, &scannedBytes));
    TEST_ASSERT_EQUAL_UINT64(1u, scannedSlots);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPoolGuard_Release(&writer));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Scan(barrierPool, &scannedSlots, &scannedBytes));
    TEST_ASSERT_EQUAL_UINT64(1u, scannedSlots);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_GetStats(barrierPool, &stats));
    TEST_ASSERT_EQUAL_UINT64(0u, (uint64_t)stats.dirtySlotCount);

    for (int pass = 0; pass < 2; pass++) {
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_OK,
                ZrPool_Scan(mappedPool, &scannedSlots, &scannedBytes));
        TEST_ASSERT_EQUAL_UINT64(2u, scannedSlots);
        TEST_ASSERT_EQUAL_UINT64(
                2u * sizeof(SPoolProbeValue), scannedBytes);
    }
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK,
            ZrPool_Scan(freePool, &scannedSlots, &scannedBytes));
    TEST_ASSERT_EQUAL_UINT64(0u, scannedSlots);
    TEST_ASSERT_EQUAL_UINT64(0u, scannedBytes);

    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&barrierPool));
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&mappedPool));
    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&freePool));
}

static void test_hot_guard_projection_and_churn_do_not_revalidate(void) {
    const TZrSize hotIterationCount = 1000000u;
    const TZrSize churnCount = 100000u;
    SPoolProbe probe = {0};
    SZrPoolTypeLayout layout = probe_layout(&probe, ZR_POOL_GC_SCAN_FREE);
    SZrPoolConfig config = probe_config(1u);
    SZrPool *pool = ZR_NULL;
    SZrPoolHandle original;
    SZrPoolHandle current;
    SZrPoolGuard writer = {0};
    SZrPoolStats beforeHotLoop;
    SZrPoolStats afterHotLoop;
    SPoolProbeValue value = {0, 0x5a5a};
    SPoolProbeValue *direct;

    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Create(&layout, &config, &pool));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Deliver(pool, &value, &original));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_TryBorrow(pool, original, &writer));
    direct = (SPoolProbeValue *)ZrPoolGuard_Value(&writer);
    TEST_ASSERT_NOT_NULL(direct);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_GetStats(pool, &beforeHotLoop));
    for (TZrSize index = 0u; index < hotIterationCount; index++) {
        direct->value++;
    }
    TEST_ASSERT_EQUAL_UINT64(hotIterationCount, (uint64_t)direct->value);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_GetStats(pool, &afterHotLoop));
    TEST_ASSERT_EQUAL_UINT64(
            beforeHotLoop.handleValidationCount,
            afterHotLoop.handleValidationCount);
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPoolGuard_Release(&writer));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_Recycle(pool, original));

    for (TZrSize index = 0u; index < churnCount; index++) {
        value.value = (int)index;
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_OK, ZrPool_Deliver(pool, &value, &current));
        TEST_ASSERT_EQUAL_UINT64(
                (uint64_t)original.slotIndex, (uint64_t)current.slotIndex);
        TEST_ASSERT_EQUAL_INT(
                ZR_POOL_STATUS_OK, ZrPool_Recycle(pool, current));
    }
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_HANDLE_STALE, ZrPool_Validate(pool, original));
    TEST_ASSERT_EQUAL_INT(
            ZR_POOL_STATUS_OK, ZrPool_GetStats(pool, &afterHotLoop));
    TEST_ASSERT_EQUAL_UINT64(churnCount, afterHotLoop.reuseCount);
    TEST_ASSERT_EQUAL_UINT64(churnCount + 1u, afterHotLoop.dropCount);

    TEST_ASSERT_EQUAL_INT(ZR_POOL_STATUS_OK, ZrPool_Destroy(&pool));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_partial_initialization_rolls_back_before_slot_reuse);
    RUN_TEST(test_barriered_cards_and_scan_classes_report_separately);
    RUN_TEST(test_hot_guard_projection_and_churn_do_not_revalidate);
    return UNITY_END();
}
