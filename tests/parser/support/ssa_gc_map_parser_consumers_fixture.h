#ifndef ZR_TEST_SSA_GC_MAP_PARSER_CONSUMERS_FIXTURE_H
#define ZR_TEST_SSA_GC_MAP_PARSER_CONSUMERS_FIXTURE_H

/* Private, metadata-only fixture. No execution or materialization entrance. */
enum { GC_SNAPSHOT_SPANS = 16, GC_SNAPSHOT_BYTES = 16384 };
typedef struct SGcSpanSnapshot {
    const void *pointer;
    size_t bytes;
    unsigned char content[GC_SNAPSHOT_BYTES];
} SGcSpanSnapshot;
typedef struct SGcSnapshot {
    SZrExecIrFunction function;
    SZrContainerSpecializationFacts facts;
    SZrContainerSpecializationPlan plan;
    SZrExecIrAnalysisCache cache;
    SZrExecIrPassContext context;
    unsigned count;
    SGcSpanSnapshot spans[GC_SNAPSHOT_SPANS];
} SGcSnapshot;

static SZrExecIrFunction g_function;
static SZrContainerSpecializationFacts g_facts;
static SZrContainerSpecializationPlan g_plan;
static SZrExecIrAnalysisCache g_cache;
static SZrExecIrPassContext g_context;
static SZrExecIrGcMap *g_borrowed;
static SZrExecIrGcMap *g_saved_map;
static TZrUInt32 g_saved_count, g_saved_capacity;
static unsigned g_bound;
static SGcSnapshot g_before;

static int gc_snapshot_span(const void *pointer) {
    SZrOwnerLiveSpan live;
    SGcSpanSnapshot *copy;
    if (pointer == NULL) return 1;
    if (g_before.count >= GC_SNAPSHOT_SPANS ||
        !ZrTests_ExecOwner_TryLiveSpan(0u, ZrTests_ExecOwner_Stats().records,
                pointer, 1u, 1u, &live) || live.allocation != pointer ||
        live.bytes > GC_SNAPSHOT_BYTES) return 0;
    copy = &g_before.spans[g_before.count++];
    copy->pointer = pointer;
    copy->bytes = live.bytes;
    memcpy(copy->content, pointer, live.bytes);
    return 1;
}

static int gc_snapshot(void) {
    g_before.function = g_function;
    g_before.facts = g_facts;
    g_before.plan = g_plan;
    g_before.cache = g_cache;
    g_before.context = g_context;
    g_before.count = 0u;
    if (!gc_snapshot_span(g_function.instructions) ||
        !gc_snapshot_span(g_function.blocks) ||
        !gc_snapshot_span(g_borrowed) || !gc_snapshot_span(g_saved_map)) return 0;
    if (g_saved_map != NULL &&
        (!gc_snapshot_span(g_saved_map->entries) ||
         !gc_snapshot_span(g_saved_map->slotIndexPool) ||
         !gc_snapshot_span(g_saved_map->inlineRefOffsetPool))) return 0;
    return gc_snapshot_span(g_cache.sccpValues);
}

static int gc_unchanged(unsigned buildPlan) {
    if (memcmp(&g_before.function, &g_function, sizeof(g_function)) != 0 ||
        memcmp(&g_before.facts, &g_facts, sizeof(g_facts)) != 0 ||
        memcmp(&g_before.cache, &g_cache, sizeof(g_cache)) != 0 ||
        memcmp(&g_before.context, &g_context, sizeof(g_context)) != 0) return 0;
    if (!buildPlan && memcmp(&g_before.plan, &g_plan, sizeof(g_plan)) != 0) return 0;
    for (unsigned i = 0u; i < g_before.count; ++i) {
        const SGcSpanSnapshot *copy = &g_before.spans[i];
        SZrOwnerLiveSpan live;
        if (!ZrTests_ExecOwner_TryLiveSpan(0u, ZrTests_ExecOwner_Stats().records,
                copy->pointer, copy->bytes, 1u, &live) ||
            live.allocation != copy->pointer || live.bytes != copy->bytes) return 0;
        if (memcmp(copy->content, copy->pointer, copy->bytes) != 0) return 0;
    }
    return 1;
}

static void gc_diagnostic_equal(const SZrExecIrDiagnostic *expected,
                                const SZrExecIrDiagnostic *actual) {
    TEST_ASSERT_EQUAL_INT(expected->code, actual->code);
    TEST_ASSERT_EQUAL_UINT32(expected->functionToken, actual->functionToken);
    TEST_ASSERT_EQUAL_UINT32(expected->blockId, actual->blockId);
    TEST_ASSERT_EQUAL_UINT32(expected->instructionId, actual->instructionId);
    TEST_ASSERT_EQUAL_UINT32(expected->sourceId, actual->sourceId);
    TEST_ASSERT_EQUAL_UINT32(expected->expectedVersion, actual->expectedVersion);
    TEST_ASSERT_EQUAL_UINT32(expected->actualVersion, actual->actualVersion);
    TEST_ASSERT_EQUAL_UINT64(expected->expectedHash, actual->expectedHash);
    TEST_ASSERT_EQUAL_UINT64(expected->actualHash, actual->actualHash);
}

static void gc_restore_borrow(void) {
    if (g_bound) {
        g_function.gcMap = g_saved_map;
        g_function.gcMapCount = g_saved_count;
        g_function.gcMapCapacity = g_saved_capacity;
        g_bound = 0u;
    }
    free(g_borrowed);
    g_borrowed = NULL;
}

static void gc_release_fixture(void) {
    /* Restoration always precedes every owner Free and every result assertion. */
    gc_restore_borrow();
    ZrParser_ExecIr_AnalysisCacheFree(&g_cache);
    ZrCore_ExecIr_FreeFunction(&g_function);
}

static void gc_build_fixture(unsigned kind) {
    SZrExecIrInstruction instruction = {0};
    TZrExecIrInstructionId id;
    ZrCore_ExecIr_FunctionInit(&g_function);
    g_function.functionToken = 73u;
    g_function.signatureHash = 101u;
    g_function.bindingRowsSchemaVersion = ZR_EXEC_IR_BINDING_ROWS_SCHEMA_LEGACY;
    TEST_ASSERT_EQUAL_UINT32(1u, ZrCore_ExecIr_FunctionAddBlock(&g_function, 0u));
    g_function.entryBlockId = 1u;
    instruction.opcode = ZR_EXEC_IR_OPCODE_NOP;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(&g_function, &instruction, &id));
    TEST_ASSERT_EQUAL_UINT32(1u, id);
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(&g_function, &instruction, &id));
    TEST_ASSERT_EQUAL_UINT32(2u, id);
    g_function.blocks[0].instructionRange = (SZrExecIrRange){.start = 0u, .count = 2u};
    g_function.blocks[0].terminatorInstructionId = 2u;
    if (kind != 0u) {
        SZrExecIrGcMap *map;
        g_function.gcMap = calloc(1u, sizeof(*g_function.gcMap));
        TEST_ASSERT_NOT_NULL(g_function.gcMap);
        g_function.gcMapCount = g_function.gcMapCapacity = 1u;
        map = g_function.gcMap;
        if (kind == 2u) {
            map->entries = calloc(3u, sizeof(*map->entries));
            map->slotIndexPool = calloc(3u, sizeof(*map->slotIndexPool));
            map->inlineRefOffsetPool = calloc(3u, sizeof(*map->inlineRefOffsetPool));
            TEST_ASSERT_NOT_NULL(map->entries);
            TEST_ASSERT_NOT_NULL(map->slotIndexPool);
            TEST_ASSERT_NOT_NULL(map->inlineRefOffsetPool);
            map->entryCount = map->slotIndexCount = map->inlineRefOffsetCount = 2u;
            map->entryCapacity = map->slotIndexCapacity = map->inlineRefOffsetCapacity = 3u;
            for (unsigned i = 0u; i < 2u; ++i) {
                map->entries[i].site = i + 1u;
                map->entries[i].liveRefSlots = (SZrExecIrRange){.start = i, .count = 1u};
                map->entries[i].inlineRefOffsets = (SZrExecIrRange){.start = i, .count = 1u};
            }
            map->slotIndexPool[0] = 3u;
            map->slotIndexPool[1] = 7u;
            map->inlineRefOffsetPool[0] = 8u;
            map->inlineRefOffsetPool[1] = 24u;
        }
    }
    ZrParser_ExecIr_ContainerSpecializationFactsInit(&g_facts);
    g_facts.functionToken = g_function.functionToken;
    g_facts.generation = g_function.contract.generation;
    ZrParser_ExecIr_ContainerSpecializationPlanInit(&g_plan);
    ZrParser_ExecIr_AnalysisCacheInit(&g_cache);
    memset(&g_context, 0, sizeof(g_context));
    g_context.cache = &g_cache;
}

static void gc_admit_map(void) {
    SZrCompactMapCandidate *candidate = &g_facts.mapCandidate;
    candidate->hash.seed = 11u;
    candidate->hash.domain = 22u;
    candidate->hash.version = 3u;
    candidate->hash.flags = ZR_COMPACT_MAP_HASH_FLAG_IMMUTABLE_KEY |
            ZR_COMPACT_MAP_HASH_FLAG_HASH_PURE | ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_PURE |
            ZR_COMPACT_MAP_HASH_FLAG_EQUALITY_TOTAL | ZR_COMPACT_MAP_HASH_FLAG_DOMAIN_STABLE;
    candidate->layout.entrySize = 32u;
    candidate->layout.entryAlignment = 8u;
    candidate->layout.bucketCount = 8u;
    candidate->layout.maxLoadNumerator = 3u;
    candidate->layout.maxLoadDenominator = 4u;
    candidate->layout.hashOffset = 0u;
    candidate->layout.keyOffset = 8u;
    candidate->layout.valueOffset = 16u;
    candidate->layout.nextOffset = 24u;
    candidate->layout.tombstonePolicy = ZR_COMPACT_MAP_TOMBSTONE_MARK;
    candidate->layout.iterationPolicy = ZR_COMPACT_MAP_ITERATION_INSERTION_ORDER;
    candidate->layout.ownershipPolicy = ZR_COMPACT_MAP_OWNERSHIP_OWNED;
    candidate->flags = ZR_COMPACT_MAP_FLAG_CACHE_HASH | ZR_COMPACT_MAP_FLAG_PRESERVE_ITERATION;
    TEST_ASSERT_TRUE(ZrCore_CompactMapCandidate_Finalize(candidate, NULL));
    g_facts.mapKnown = ZR_TRUE;
}

static void gc_bind_malformed(unsigned row) {
    static const TZrUInt32 counts[6] = {2u, 1u, 0u, 1u, 0u, 2u};
    static const TZrUInt32 capacities[6] = {2u, 2u, 0u, 1u, 1u, 1u};
    g_saved_map = g_function.gcMap;
    g_saved_count = g_function.gcMapCount;
    g_saved_capacity = g_function.gcMapCapacity;
    if (row != 3u && row != 4u) {
        /* Even 2/1 has two genuine readable headers; no poison addresses. */
        g_borrowed = calloc(2u, sizeof(*g_borrowed));
        TEST_ASSERT_NOT_NULL(g_borrowed);
        /* Borrow valid inner pools from the separately owned canonical map.
         * Only the header/count/capacity envelope is malformed. */
        if (g_saved_map != NULL) g_borrowed[0] = *g_saved_map;
    }
    g_function.gcMap = g_borrowed;
    g_function.gcMapCount = counts[row];
    g_function.gcMapCapacity = capacities[row];
    g_bound = 1u;
}
#endif
