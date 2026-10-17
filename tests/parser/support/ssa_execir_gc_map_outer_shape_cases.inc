/* Pure Core metadata tests. Included after the existing allocator cases. */
typedef struct SGcShapeSnapshot {
    unsigned char bytes[8192];
    size_t count;
} SGcShapeSnapshot;

static void gc_shape_bytes(SGcShapeSnapshot *s, const void *p, size_t n) {
    expect_true(n <= sizeof(s->bytes) - s->count, "GC shape snapshot overflow");
    if (n) memcpy(s->bytes + s->count, p, n);
    s->count += n;
}

static SGcShapeSnapshot gc_shape_snapshot(const SZrExecIrFunction *f) {
    SGcShapeSnapshot s = {{0}, 0};
    gc_shape_bytes(&s, f, sizeof(*f));
#define GC_SHAPE_BYTES(field, capacity) \
    gc_shape_bytes(&s, f->field, (size_t)f->capacity * sizeof(*f->field))
    GC_SHAPE_BYTES(values, valueCapacity);
    GC_SHAPE_BYTES(blocks, blockCapacity);
    GC_SHAPE_BYTES(instructions, instructionCapacity);
#undef GC_SHAPE_BYTES
    if (f->gcMap) {
        const SZrExecIrGcMap *g = f->gcMap;
        gc_shape_bytes(&s, g, sizeof(*g));
        gc_shape_bytes(&s, g->entries, (size_t)g->entryCapacity * sizeof(*g->entries));
        gc_shape_bytes(&s, g->slotIndexPool, (size_t)g->slotIndexCapacity * sizeof(*g->slotIndexPool));
        gc_shape_bytes(&s, g->inlineRefOffsetPool, (size_t)g->inlineRefOffsetCapacity * sizeof(*g->inlineRefOffsetPool));
    }
    return s;
}

static int gc_shape_same(const SGcShapeSnapshot *a, const SGcShapeSnapshot *b) {
    return a->count == b->count && memcmp(a->bytes, b->bytes, a->count) == 0;
}

static int gc_shape_diag(const SZrExecIrDiagnostic *a, const SZrExecIrDiagnostic *b) {
    return a->code == b->code && a->functionToken == b->functionToken &&
        a->blockId == b->blockId && a->instructionId == b->instructionId &&
        a->sourceId == b->sourceId && a->expectedVersion == b->expectedVersion &&
        a->actualVersion == b->actualVersion && a->expectedHash == b->expectedHash &&
        a->actualHash == b->actualHash;
}

static void gc_shape_function(SZrExecIrFunction *f, TZrMetadataToken token, unsigned mode) {
    SZrExecIrInstruction instruction = {0};
    ZrCore_ExecIr_FunctionInit(f);
    f->id = 1u; f->functionToken = token; f->signatureHash = 234u;
    expect_true(ZrCore_ExecIr_FunctionAddBlock(f, ZR_EXEC_IR_BLOCK_FLAG_ENTRY) == 1u,
                "GC shape entry block creation failed");
    expect_true(ZrCore_ExecIr_FunctionAddValue(f, 17u, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                    ZR_EXEC_IR_NULLABILITY_NULLABLE) == 1u, "GC shape value creation failed");
    instruction.opcode = ZR_EXEC_IR_OPCODE_NOP;
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(f, &instruction, NULL), "GC shape NOP append failed");
    instruction.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    expect_true(ZrCore_ExecIr_FunctionAppendInstruction(f, &instruction, NULL), "GC shape return append failed");
    f->blocks[0].instructionRange.count = 2u; f->blocks[0].terminatorInstructionId = 2u;
    /* Zero spare capacity bytes so byte snapshots include deterministic storage. */
    memset(f->values + f->valueCount, 0, (f->valueCapacity - f->valueCount) * sizeof(*f->values));
    memset(f->blocks + f->blockCount, 0, (f->blockCapacity - f->blockCount) * sizeof(*f->blocks));
    memset(f->instructions + f->instructionCount, 0, (f->instructionCapacity - f->instructionCount) * sizeof(*f->instructions));
    if (!mode) return;
    f->gcMap = calloc(1u, sizeof(*f->gcMap));
    expect_true(f->gcMap != NULL, "GC shape header allocation failed");
    f->gcMapCount = f->gcMapCapacity = 1u;
    if (mode == 1u) return;
    SZrExecIrGcMap *g = f->gcMap;
    g->entries = calloc(3u, sizeof(*g->entries));
    g->slotIndexPool = calloc(3u, sizeof(*g->slotIndexPool));
    g->inlineRefOffsetPool = calloc(3u, sizeof(*g->inlineRefOffsetPool));
    expect_true(g->entries && g->slotIndexPool && g->inlineRefOffsetPool, "GC shape pools allocation failed");
    g->entryCount = g->slotIndexCount = g->inlineRefOffsetCount = 2u;
    g->entryCapacity = g->slotIndexCapacity = g->inlineRefOffsetCapacity = 3u;
    g->safepointId = 31u; g->sourceId = 32u;
    for (unsigned i = 0; i < 2u; ++i) {
        g->entries[i].site = i + 1u;
        g->entries[i].liveRefSlots.start = i; g->entries[i].liveRefSlots.count = 1u;
        g->entries[i].inlineRefOffsets.start = i; g->entries[i].inlineRefOffsets.count = 1u;
        g->slotIndexPool[i] = i; g->inlineRefOffsetPool[i] = 16u + i * 8u;
    }
}

static void test_gc_map_outer_shape_rejections(void) {
    static const unsigned counts[] = {2u, 1u, 0u, 1u, 0u, 2u};
    static const unsigned capacities[] = {2u, 2u, 0u, 1u, 1u, 1u};
    for (unsigned row = 0; row < 6u; ++row) {
        for (unsigned entry = 0; entry < 2u; ++entry) {
            for (unsigned nullDiag = 0; nullDiag < 2u; ++nullDiag) {
                SZrExecIrFunction source, keeper;
                SZrExecIrDiagnostic d, expected = {0};
                SZrExecIrGcMap *storage = calloc(2u, sizeof(*storage));
                SZrExecIrGcMap storageBefore[2];
                TZrBool ok, healthy;
                int unchanged, keeperUnchanged = 1, diagnosticOk;
                gc_shape_function(&source, 901u, 0u); gc_shape_function(&keeper, 902u, 2u);
                expect_true(storage != NULL, "GC outer malformed storage allocation failed");
                memcpy(storageBefore, storage, sizeof(storageBefore));
                source.gcMap = row == 3u || row == 4u ? NULL : storage;
                source.gcMapCount = counts[row]; source.gcMapCapacity = capacities[row];
                SGcShapeSnapshot before = gc_shape_snapshot(&source), old = gc_shape_snapshot(&keeper);
                expected.code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
                expected.functionToken = source.functionToken;
                memset(&d, 0xA5, sizeof(d));
#if defined(ZR_EXEC_IR_TEST_ALLOCATOR)
                ZrCore_ExecIr_TestAllocatorReset();
#endif
                ok = entry ? ZrCore_ExecIr_VerifyFunction(&source, ZR_EXEC_IR_VERIFY_STRUCTURE, nullDiag ? NULL : &d)
                           : ZrCore_ExecIr_CloneFunction(&source, &keeper, nullDiag ? NULL : &d);
#if defined(ZR_EXEC_IR_TEST_ALLOCATOR)
                TZrUInt64 attempts = ZrCore_ExecIr_TestAllocatorCount();
                ZrCore_ExecIr_TestAllocatorReset();
#endif
                SGcShapeSnapshot after = gc_shape_snapshot(&source);
                unchanged = gc_shape_same(&before, &after) && memcmp(storageBefore, storage, sizeof(storageBefore)) == 0;
                /* Unexpected successful clone has released the old keeper: never follow old pointers. */
                if (!ok || entry) { after = gc_shape_snapshot(&keeper); keeperUnchanged = gc_shape_same(&old, &after); }
                diagnosticOk = nullDiag || gc_shape_diag(&d, &expected);
                source.gcMap = NULL; source.gcMapCount = source.gcMapCapacity = 0u;
                ZrCore_ExecIr_GcMapFree(&storage[0]); ZrCore_ExecIr_GcMapFree(&storage[1]); free(storage);
                memset(&d, 0xA5, sizeof(d));
                healthy = entry ? ZrCore_ExecIr_VerifyFunction(&source, ZR_EXEC_IR_VERIFY_STRUCTURE, &d)
                                : ZrCore_ExecIr_CloneFunction(&source, &keeper, &d);
                SZrExecIrDiagnostic zero = {0};
                int cleared = gc_shape_diag(&d, &zero);
                ZrCore_ExecIr_FreeFunction(&keeper); ZrCore_ExecIr_FreeFunction(&source);
                expect_true(!ok, "GC outer malformed shape was accepted");
                expect_true(diagnosticOk && unchanged && keeperUnchanged, "GC outer refusal changed input/keeper or diagnostic");
#if defined(ZR_EXEC_IR_TEST_ALLOCATOR)
                expect_true(attempts == 0u, "GC outer rejection allocated storage");
#endif
                expect_true(healthy && cleared, "GC outer healthy retry failed or left diagnostic poison");
            }
        }
    }
}

static void test_gc_map_legal_lifecycle(void) {
    for (unsigned mode = 0; mode < 3u; ++mode) {
        SZrExecIrFunction source, clone;
        SZrExecIrDiagnostic d, zero = {0};
        gc_shape_function(&source, 911u, mode); gc_shape_function(&clone, 912u, 1u);
        memset(&d, 0xA5, sizeof(d));
        expect_true(ZrCore_ExecIr_VerifyFunction(&source, ZR_EXEC_IR_VERIFY_STRUCTURE, &d) &&
                    gc_shape_diag(&d, &zero), "GC legal source failed structure verification");
        SGcShapeSnapshot before = gc_shape_snapshot(&source);
        memset(&d, 0xA5, sizeof(d));
        expect_true(ZrCore_ExecIr_CloneFunction(&source, &clone, &d), "GC legal clone failed");
        expect_true(gc_shape_diag(&d, &zero), "GC legal clone left diagnostic poison");
        expect_true(clone.id == source.id && clone.functionToken == source.functionToken &&
                    clone.values[0].id == source.values[0].id && clone.values[0].typeToken == source.values[0].typeToken,
                    "GC clone changed stable tokens or local IDs");
        expect_true(clone.gcMapCount == (mode ? 1u : 0u) && clone.gcMapCapacity == (mode ? 1u : 0u), "GC legal clone outer cardinality changed");
        if (mode) {
            expect_true(clone.gcMap && clone.gcMap != source.gcMap, "GC legal clone aliased header");
            SZrExecIrGcMap expected = *source.gcMap;
            expected.entries = clone.gcMap->entries; expected.entryCapacity = clone.gcMap->entryCapacity;
            expected.slotIndexPool = clone.gcMap->slotIndexPool; expected.slotIndexCapacity = clone.gcMap->slotIndexCapacity;
            expected.inlineRefOffsetPool = clone.gcMap->inlineRefOffsetPool; expected.inlineRefOffsetCapacity = clone.gcMap->inlineRefOffsetCapacity;
            expect_true(memcmp(&expected, clone.gcMap, sizeof(expected)) == 0, "GC clone lost logical header fields");
            if (mode == 2u) {
                expect_true(clone.gcMap->entries != source.gcMap->entries && clone.gcMap->slotIndexPool != source.gcMap->slotIndexPool &&
                            clone.gcMap->inlineRefOffsetPool != source.gcMap->inlineRefOffsetPool, "GC clone aliased inner pool");
                expect_true(memcmp(clone.gcMap->entries, source.gcMap->entries, 2u * sizeof(*source.gcMap->entries)) == 0 &&
                            memcmp(clone.gcMap->slotIndexPool, source.gcMap->slotIndexPool, 2u * sizeof(*source.gcMap->slotIndexPool)) == 0 &&
                            memcmp(clone.gcMap->inlineRefOffsetPool, source.gcMap->inlineRefOffsetPool, 2u * sizeof(*source.gcMap->inlineRefOffsetPool)) == 0,
                            "GC clone changed logical pool elements");
                clone.gcMap->entries[0].site = 2u; clone.gcMap->slotIndexPool[0] = 7u; clone.gcMap->inlineRefOffsetPool[0] = 77u;
            } else expect_true(!clone.gcMap->entries && !clone.gcMap->slotIndexPool && !clone.gcMap->inlineRefOffsetPool, "GC present empty clone gained pools");
        } else expect_true(clone.gcMap == NULL, "GC absent clone gained header");
        SGcShapeSnapshot after = gc_shape_snapshot(&source);
        expect_true(gc_shape_same(&before, &after), "GC clone mutation changed source");
        ZrCore_ExecIr_FreeFunction(&source); ZrCore_ExecIr_FreeFunction(&source);
        expect_true(clone.values[0].typeToken == 17u, "GC clone lost metadata after source free");
        if (mode == 2u) expect_true(clone.gcMap->entries[1].site == 2u && clone.gcMap->slotIndexPool[0] == 7u &&
                                   clone.gcMap->inlineRefOffsetPool[0] == 77u, "GC clone invalid after source free");
        ZrCore_ExecIr_FreeFunction(&clone); ZrCore_ExecIr_FreeFunction(&clone);
    }
}

#if defined(ZR_EXEC_IR_TEST_ALLOCATOR)
static void test_gc_map_clone_allocation_sweep(void) {
    SZrExecIrFunction source, keeper;
    SZrExecIrDiagnostic d, zero = {0};
    TZrUInt64 original = ZrCore_ExecIr_TestAllocatorLiveCount();
    gc_shape_function(&source, 921u, 2u); gc_shape_function(&keeper, 922u, 2u);
    ZrCore_ExecIr_TestAllocatorReset();
    expect_true(ZrCore_ExecIr_CloneFunction(&source, &keeper, &d), "GC allocation sweep measurement failed");
    TZrUInt64 count = ZrCore_ExecIr_TestAllocatorCount();
    expect_true(count > 0u, "GC allocation sweep measured no allocations");
    ZrCore_ExecIr_TestAllocatorReset();
    for (TZrUInt64 ordinal = 1u; ordinal <= count; ++ordinal) {
        SGcShapeSnapshot sourceBefore = gc_shape_snapshot(&source), keeperBefore = gc_shape_snapshot(&keeper);
        TZrUInt64 baseline = ZrCore_ExecIr_TestAllocatorLiveCount();
        ZrCore_ExecIr_TestAllocatorFailAt(ordinal); memset(&d, 0xA5, sizeof(d));
        TZrBool ok = ZrCore_ExecIr_CloneFunction(&source, &keeper, &d);
        TZrUInt64 attempts = ZrCore_ExecIr_TestAllocatorCount(), live = ZrCore_ExecIr_TestAllocatorLiveCount();
        SGcShapeSnapshot sourceAfter = gc_shape_snapshot(&source);
        int unchanged = gc_shape_same(&sourceBefore, &sourceAfter), keeperUnchanged = 1;
        if (!ok) { SGcShapeSnapshot after = gc_shape_snapshot(&keeper); keeperUnchanged = gc_shape_same(&keeperBefore, &after); }
        SZrExecIrDiagnostic expected = {0}; expected.code = ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
        expected.functionToken = source.functionToken; expected.expectedVersion = d.expectedVersion;
        int diagnosticOk = gc_shape_diag(&d, &expected) && (d.expectedVersion == 1u || d.expectedVersion == 2u);
        /* Preserve each site's real requested count; outer-header OOM requests one,
         * pools/arrays request their logical counts. No ordinal order is assumed. */
        printf("GC CloneFunction fault %llu/%llu code=%u token=%u block=%u instruction=%u source=%u expectedVersion=%u actualVersion=%u expectedHash=%llu actualHash=%llu\n",
            (unsigned long long)ordinal, (unsigned long long)count, (unsigned)d.code, (unsigned)d.functionToken,
            (unsigned)d.blockId, (unsigned)d.instructionId, (unsigned)d.sourceId, (unsigned)d.expectedVersion,
            (unsigned)d.actualVersion, (unsigned long long)d.expectedHash, (unsigned long long)d.actualHash);
        /* Observe cleanup and keeper before Reset, which does not free live owners. */
        ZrCore_ExecIr_TestAllocatorReset();
        memset(&d, 0xA5, sizeof(d));
        TZrBool retry = ZrCore_ExecIr_CloneFunction(&source, &keeper, &d);
        int cleared = gc_shape_diag(&d, &zero);
        if (ok || !unchanged || !keeperUnchanged || live != baseline || !diagnosticOk || attempts != ordinal || !retry || !cleared) {
            ZrCore_ExecIr_FreeFunction(&keeper); ZrCore_ExecIr_FreeFunction(&source);
            expect_true(0, "GC allocation failure lost diagnostic, ownership, input, cleanup or retry");
        }
    }
    ZrCore_ExecIr_TestAllocatorReset();
    ZrCore_ExecIr_FreeFunction(&keeper); ZrCore_ExecIr_FreeFunction(&source);
    expect_true(ZrCore_ExecIr_TestAllocatorLiveCount() == original, "GC allocation sweep leaked storage");
}
#endif
