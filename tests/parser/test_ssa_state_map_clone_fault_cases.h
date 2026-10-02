#ifndef ZR_TEST_SSA_STATE_MAP_CLONE_FAULT_CASES_H
#define ZR_TEST_SSA_STATE_MAP_CLONE_FAULT_CASES_H

/* One live item plus one spare item makes both logical and capacity bytes
 * observable. The snapshots own their bytes rather than sharing pool pointers. */
typedef struct SStateMapCloneSnapshot {
    unsigned char map[sizeof(SZrExecIrStateMap)];
    unsigned char entries[2u * sizeof(SZrExecIrStateMapEntry)];
    unsigned char values[2u * sizeof(TZrExecIrValueId)];
    unsigned char roots[2u * sizeof(TZrExecIrValueId)];
    unsigned char owners[2u * sizeof(TZrUInt32)];
} SStateMapCloneSnapshot;

static void state_map_clone_fixture(SZrExecIrStateMap *map, TZrUInt32 seed) {
    map_with_one_entry(map, ZR_EXEC_IR_STATE_AFTER_EFFECT,
                       ZR_EXEC_IR_STATE_MAP_BOUNDARY_GC);
    map->entries = realloc(map->entries, 2u * sizeof(*map->entries));
    map->valuePool = realloc(map->valuePool, 2u * sizeof(*map->valuePool));
    map->rootPool = realloc(map->rootPool, 2u * sizeof(*map->rootPool));
    map->ownerStatePool = realloc(map->ownerStatePool,
                                   2u * sizeof(*map->ownerStatePool));
    TEST_ASSERT_NOT_NULL(map->entries);
    TEST_ASSERT_NOT_NULL(map->valuePool);
    TEST_ASSERT_NOT_NULL(map->rootPool);
    TEST_ASSERT_NOT_NULL(map->ownerStatePool);
    map->entryCapacity = map->valueCapacity = map->rootCapacity =
            map->ownerStateCapacity = 2u;
    memset(&map->entries[1], 0x5a, sizeof(map->entries[1]));
    map->functionToken = seed;
    map->signatureHash = (TZrUInt64)seed + 1u;
    map->generation = (TZrUInt64)seed + 2u;
    map->entries[0].sourceId = seed + 3u;
    map->valuePool[0] = seed + 4u;
    map->valuePool[1] = seed + 5u;
    map->rootPool[0] = seed + 6u;
    map->rootPool[1] = seed + 7u;
    map->ownerStatePool[0] = ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED;
    map->ownerStatePool[1] = seed + 8u;
}

static void state_map_clone_snapshot(const SZrExecIrStateMap *map,
                                      SStateMapCloneSnapshot *snapshot) {
    memcpy(snapshot->map, map, sizeof(snapshot->map));
    memcpy(snapshot->entries, map->entries, sizeof(snapshot->entries));
    memcpy(snapshot->values, map->valuePool, sizeof(snapshot->values));
    memcpy(snapshot->roots, map->rootPool, sizeof(snapshot->roots));
    memcpy(snapshot->owners, map->ownerStatePool, sizeof(snapshot->owners));
}

static void state_map_clone_assert_snapshot(
        const SZrExecIrStateMap *map, const SStateMapCloneSnapshot *snapshot) {
    TEST_ASSERT_EQUAL_MEMORY(snapshot->map, map, sizeof(snapshot->map));
    TEST_ASSERT_EQUAL_MEMORY(snapshot->entries, map->entries,
                             sizeof(snapshot->entries));
    TEST_ASSERT_EQUAL_MEMORY(snapshot->values, map->valuePool,
                             sizeof(snapshot->values));
    TEST_ASSERT_EQUAL_MEMORY(snapshot->roots, map->rootPool,
                             sizeof(snapshot->roots));
    TEST_ASSERT_EQUAL_MEMORY(snapshot->owners, map->ownerStatePool,
                             sizeof(snapshot->owners));
}

void test_state_map_clone_preserves_both_maps_on_each_pool_allocation_failure(void) {
    size_t ordinal;

    for (ordinal = 1u; ordinal <= 4u; ++ordinal) {
        SZrExecIrStateMap source;
        SZrExecIrStateMap destination;
        SStateMapCloneSnapshot sourceBefore;
        SStateMapCloneSnapshot destinationBefore;
        TZrBool result;
        int failed;
        size_t attempts;
        size_t outstanding;

        TEST_ASSERT_EQUAL(0u, ssa_state_map_clone_outstanding_allocations());
        state_map_clone_fixture(&source, 101u);
        state_map_clone_fixture(&destination, 701u);
        state_map_clone_snapshot(&source, &sourceBefore);
        state_map_clone_snapshot(&destination, &destinationBefore);
        ssa_state_map_clone_fail_allocation(ordinal);
        result = ZrCore_ExecIr_StateMapClone(&source, &destination);
        failed = ssa_state_map_clone_allocation_failed();
        attempts = ssa_state_map_clone_allocation_attempts();
        outstanding = ssa_state_map_clone_outstanding_allocations();
        ssa_state_map_clone_fail_allocation(0u);

        TEST_ASSERT_FALSE(result);
        TEST_ASSERT_TRUE(failed);
        TEST_ASSERT_EQUAL(ordinal, attempts);
        TEST_ASSERT_EQUAL(0u, outstanding);
        state_map_clone_assert_snapshot(&source, &sourceBefore);
        state_map_clone_assert_snapshot(&destination, &destinationBefore);
        ZrCore_ExecIr_StateMapFree(&source);
        ZrCore_ExecIr_StateMapFree(&destination);
    }
}

void test_state_map_clone_replaces_destination_after_last_pool_success(void) {
    SZrExecIrStateMap source;
    SZrExecIrStateMap destination;
    SStateMapCloneSnapshot sourceBefore;
    TZrBool result;
    int failed;
    size_t attempts;

    TEST_ASSERT_EQUAL(0u, ssa_state_map_clone_outstanding_allocations());
    state_map_clone_fixture(&source, 101u);
    state_map_clone_fixture(&destination, 701u);
    state_map_clone_snapshot(&source, &sourceBefore);
    ssa_state_map_clone_fail_allocation(5u);
    result = ZrCore_ExecIr_StateMapClone(&source, &destination);
    failed = ssa_state_map_clone_allocation_failed();
    attempts = ssa_state_map_clone_allocation_attempts();
    ssa_state_map_clone_fail_allocation(0u);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(failed);
    TEST_ASSERT_EQUAL(4u, attempts);
    TEST_ASSERT_EQUAL(4u, ssa_state_map_clone_outstanding_allocations());
    state_map_clone_assert_snapshot(&source, &sourceBefore);
    TEST_ASSERT_EQUAL(source.functionToken, destination.functionToken);
    TEST_ASSERT_EQUAL(source.signatureHash, destination.signatureHash);
    TEST_ASSERT_EQUAL(source.generation, destination.generation);
    TEST_ASSERT_EQUAL(source.entryCount, destination.entryCount);
    TEST_ASSERT_EQUAL(source.valueCount, destination.valueCount);
    TEST_ASSERT_EQUAL(source.rootCount, destination.rootCount);
    TEST_ASSERT_EQUAL(source.ownerStateCount, destination.ownerStateCount);
    TEST_ASSERT_EQUAL(source.entryCount, destination.entryCapacity);
    TEST_ASSERT_EQUAL(source.valueCount, destination.valueCapacity);
    TEST_ASSERT_EQUAL(source.rootCount, destination.rootCapacity);
    TEST_ASSERT_EQUAL(source.ownerStateCount, destination.ownerStateCapacity);
    TEST_ASSERT_NOT_EQUAL(source.entries, destination.entries);
    TEST_ASSERT_NOT_EQUAL(source.valuePool, destination.valuePool);
    TEST_ASSERT_NOT_EQUAL(source.rootPool, destination.rootPool);
    TEST_ASSERT_NOT_EQUAL(source.ownerStatePool, destination.ownerStatePool);
    TEST_ASSERT_EQUAL_MEMORY(source.entries, destination.entries,
                             sizeof(*source.entries));
    TEST_ASSERT_EQUAL_MEMORY(source.valuePool, destination.valuePool,
                             sizeof(*source.valuePool));
    TEST_ASSERT_EQUAL_MEMORY(source.rootPool, destination.rootPool,
                             sizeof(*source.rootPool));
    TEST_ASSERT_EQUAL_MEMORY(source.ownerStatePool, destination.ownerStatePool,
                             sizeof(*source.ownerStatePool));
    ZrCore_ExecIr_StateMapFree(&source);
    TEST_ASSERT_EQUAL(104u, destination.entries[0].sourceId);
    TEST_ASSERT_EQUAL(105u, destination.valuePool[0]);
    TEST_ASSERT_EQUAL(107u, destination.rootPool[0]);
    TEST_ASSERT_EQUAL(ZR_EXEC_IR_STATE_MAP_OWNER_INITIALIZED,
                      destination.ownerStatePool[0]);
    ZrCore_ExecIr_StateMapFree(&destination);
    TEST_ASSERT_EQUAL(0u, ssa_state_map_clone_outstanding_allocations());
}

void test_state_map_clone_skips_empty_pools_without_allocating(void) {
    SZrExecIrStateMap source;
    SZrExecIrStateMap destination;
    TZrBool result;
    int failed;
    size_t attempts;

    ZrCore_ExecIr_StateMapInit(&source);
    source.functionToken = 99u;
    source.signatureHash = 123u;
    source.generation = 7u;
    state_map_clone_fixture(&destination, 701u);
    ssa_state_map_clone_fail_allocation(1u);
    result = ZrCore_ExecIr_StateMapClone(&source, &destination);
    failed = ssa_state_map_clone_allocation_failed();
    attempts = ssa_state_map_clone_allocation_attempts();
    ssa_state_map_clone_fail_allocation(0u);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(failed);
    TEST_ASSERT_EQUAL(0u, attempts);
    TEST_ASSERT_EQUAL_MEMORY(&source, &destination, sizeof(source));
    TEST_ASSERT_EQUAL(0u, ssa_state_map_clone_outstanding_allocations());
    ZrCore_ExecIr_StateMapFree(&source);
    ZrCore_ExecIr_StateMapFree(&destination);
}

void test_state_map_self_clone_does_not_allocate_or_change_pools(void) {
    SZrExecIrStateMap map;
    SStateMapCloneSnapshot before;
    TZrBool result;
    int failed;
    size_t attempts;

    state_map_clone_fixture(&map, 101u);
    state_map_clone_snapshot(&map, &before);
    ssa_state_map_clone_fail_allocation(1u);
    result = ZrCore_ExecIr_StateMapClone(&map, &map);
    failed = ssa_state_map_clone_allocation_failed();
    attempts = ssa_state_map_clone_allocation_attempts();
    ssa_state_map_clone_fail_allocation(0u);

    TEST_ASSERT_TRUE(result);
    TEST_ASSERT_FALSE(failed);
    TEST_ASSERT_EQUAL(0u, attempts);
    state_map_clone_assert_snapshot(&map, &before);
    ZrCore_ExecIr_StateMapFree(&map);
    TEST_ASSERT_EQUAL(0u, ssa_state_map_clone_outstanding_allocations());
}

#endif
