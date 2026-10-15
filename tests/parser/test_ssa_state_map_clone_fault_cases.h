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

/**
 * @struct SStateMapCloneSnapshot
 *
 * ## Summary
 * 保存 Clone fault case 对单张 fixture map 的字节快照，用来判断失败是否改写了 map 记录或池容量范围。
 *
 * ## Constraints and lifecycle
 * map 字段保存完整 struct 字节；其余数组各保存两个元素的容量字节，因此调用前 fixture 必须有四个 capacity 均为二的池。快照内存由测试栈对象拥有，独立于被测 map；它不持有池指针，也不延长池生命周期。
 *
 * ## References
 * 快照由本文件 41 行的 state_map_clone_snapshot 写入，由 50 行的 state_map_clone_assert_snapshot 比较；失败分配测试在 79–80 行记录源与目标。
 */

/**
 * @fn static void state_map_clone_fixture(SZrExecIrStateMap *map, TZrUInt32 seed)
 *
 * ## Summary
 * 构造 Clone fault case 使用的单 entry map，并将四个池扩到两个元素，使逻辑元素和备用容量字节都可观察。
 *
 * ## Parameters
 * @param[out] map 由调用方提供的无资源记录；helper 先调用 map_with_one_entry 初始化并构造一个 live item。
 * @param[in] seed 区分源/目标 fixture 元数据和测试值的种子。
 *
 * ## Returns
 * 无 C 返回值；正常完成时 map 的四池各有 capacity 2、count 1，并包含确定的元数据、逻辑项与备用槽字节。
 *
 * ## Constraints and lifecycle
 * 该 helper 只为测试造数，不验证 Clone 语义。扩容使用测试 TU 自身的 realloc，并非 fault wrapper 截获的 Clone malloc；调用方负责在正常路径用 StateMapFree 释放 map。Unity 分配断言早退时 helper 不回滚已经构造的测试池。
 *
 * ## References
 * 基础 map 由 test_ssa_state_maps.c 55–70 行的 map_with_one_entry 构造；四个 fault case 在本文件 77–78、108–109、166、189 行调用。
 */

/**
 * @fn static void state_map_clone_snapshot(const SZrExecIrStateMap *map, SStateMapCloneSnapshot *snapshot)
 *
 * ## Summary
 * 把已构造 fixture map 的记录字节及四池各两个元素的容量字节复制到调用方快照，以便之后比较失败状态。
 *
 * ## Parameters
 * @param[in] map 四个池均至少有两个容量槽且地址在调用期间有效的 fixture。
 * @param[out] snapshot 可写的独立快照对象；不持有 map 池的所有权。
 *
 * ## Returns
 * 无 C 返回值；完成后快照覆盖完整 map 记录及四个预留容量片段。
 *
 * ## Constraints and lifecycle
 * 函数不分配内存，只使用 memcpy；map/snapshot 必须有效且不得以导致复制目标别名的方式重叠。快照内容在被覆盖或离开作用域前仅用于字节断言。
 *
 * ## References
 * 实现位于本文件 41–48 行；分配失败测试分别在 79、80 行保存两个 map。
 */

/**
 * @fn static void state_map_clone_assert_snapshot(const SZrExecIrStateMap *map, const SStateMapCloneSnapshot *snapshot)
 *
 * ## Summary
 * 通过 Unity 字节断言比较 map 记录与四个容量快照，验证某个 Clone 失败场景是否完整保留该 map。
 *
 * ## Parameters
 * @param[in] map 当前待比较 map；其结构与各池地址须仍可读取。
 * @param[in] snapshot 先前由 state_map_clone_snapshot 写入的完整快照。
 *
 * ## Returns
 * 无 C 返回值；所有比较通过时正常返回；任一 Unity 断言失败会 longjmp 离开当前 case。
 *
 * ## Constraints and lifecycle
 * 该 helper 只比较快照实际保存的 struct 和每池两个元素，不检查更大容量范围或语义有效性。Unity 提前失败可跳过当前 case 后续释放语句。
 *
 * ## References
 * 字节比较位于本文件 52–60 行；分配失败 case 在 92–93 行比较源与目标，成功 Clone case 在 121 行比较源。
 */

/**
 * @fn void test_state_map_clone_preserves_both_maps_on_each_pool_allocation_failure(void)
 *
 * ## Summary
 * 对 Clone 的四个连续池分配位置逐个注入失败，确认每次失败均报告所选 malloc ordinal、无残留 clone 分配且源/目标快照不变。
 *
 * ## Returns
 * 本测试无 C 返回值；断言均通过时正常结束。Unity 断言失败只跳出当前 case，runner 随后继续 teardown 与其余用例。
 *
 * ## Constraints and lifecycle
 * 每轮使用独立的两个 capacity=2 fixture；注入在采样计数后、Unity 断言前关闭。测试记录完整 map 及四池各两个槽，因此证明这些失败点不修改快照内容并回收已分配 clone 池；不覆盖自然 malloc OOM、其他 translation unit 的分配或并发。断言提前失败可能跳过本轮 map 的局部 Free。
 *
 * ## References
 * 失败 ordinal 循环及 Clone 调用位于本文件 66–86 行，断言和清理位于 88–95 行；Unity main 在 test_ssa_state_maps.c 1379 行注册。
 */

/**
 * @fn void test_state_map_clone_replaces_destination_after_last_pool_success(void)
 *
 * ## Summary
 * 将失败 ordinal 设在第四个池分配之后，验证完整 Clone 替换已有目标、四个副本池独立，且释放源 map 后目标逻辑数据仍可读。
 *
 * ## Returns
 * 本测试无 C 返回值；断言均通过时正常结束。Unity 断言失败仅离开当前 case，runner 会继续 teardown 与剩余用例。
 *
 * ## Constraints and lifecycle
 * 当前 fixture 四池各有一个逻辑元素、容量为二。断言检查四次 clone 分配、metadata/count/capacity 与四个地址独立，但逐池内容比较只针对首个逻辑元素；释放源后继续读取四池首元素，再释放目标并确认 tracked outstanding 为零。测试不证明所有容量尾槽都复制，也不覆盖自然分配失败。提前断言失败会跳过局部清理。
 *
 * ## References
 * 目标替换和计数采样位于本文件 107–115 行，Clone 后状态、释放源后的读取及目标清理位于 117–152 行；Unity main 在 test_ssa_state_maps.c 1380 行注册。
 */

/**
 * @fn void test_state_map_clone_skips_empty_pools_without_allocating(void)
 *
 * ## Summary
 * 用带 metadata 但四池为空的源替换一个已分配目标，并在第一个被测分配 ordinal 处 armed，验证空池克隆成功且没有触发分配。
 *
 * ## Returns
 * 本测试无 C 返回值；断言均通过时正常结束。Unity 断言失败仅退出当前 case，runner 继续 teardown 与其他用例。
 *
 * ## Constraints and lifecycle
 * 只覆盖四池均 count/capacity 为零的 map；断言检查没有失败或分配尝试、目标与源记录字节相同及 outstanding 为零。目标先通过 fixture 持有常规测试池；这些池不属于 clone allocator 的 tracked allocations。正常路径释放两张 map；断言早退可能跳过局部释放。
 *
 * ## References
 * 空源初始化及注入调用位于本文件 162–171 行，断言和释放位于 173–179 行；Unity main 在 test_ssa_state_maps.c 1381 行注册。
 */

/**
 * @fn void test_state_map_self_clone_does_not_allocate_or_change_pools(void)
 *
 * ## Summary
 * 对同一条已分配 fixture map 执行 self-clone，验证别名短路成功、不触发被注入的首次分配，并保持记录及池字节快照。
 *
 * ## Returns
 * 本测试无 C 返回值；断言均通过时正常结束。Unity 断言失败只离开当前 case，runner 后续仍继续运行。
 *
 * ## Constraints and lifecycle
 * 测试只覆盖 source 与 destination 为同一 map 地址；它不证明畸形 self-map 会被验证，因为实现的同址路径在形状检查前返回。快照比较后正常释放 map 并检查 tracked allocations 归零；断言提前失败可能跳过释放。
 *
 * ## References
 * 快照、ordinal 1 注入和 self-clone 调用位于本文件 189–195 行，断言与清理位于 197–202 行；Unity main 在 test_ssa_state_maps.c 1382 行注册。
 */
