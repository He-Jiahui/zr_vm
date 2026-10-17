#include <stdlib.h>
#include <string.h>
#include "unity.h"
#include "exec_ir/exec_ir_interpreter_internal.h"
#include "exec_ir/exec_ir_projections.h"
#include "exec_ir/exec_ir_mutation_internal.h"
/* Fixture allocations share the real provider ledger; Unity keeps its CRT. */
#include "ssa_exec_ir_owned_module_shim.h"

static SZrExecIrFunction g_function;
static SZrExecBcProjection g_projection;
static SZrExecIrGcMap *g_borrowed;

/* Bounded, nonallocating snapshots of every allocation in this small fixture.
 * The ledger supplies actual allocation lengths, including unused capacity. */
enum { SNAPSHOT_SPANS = 40, SNAPSHOT_BYTES = 16384 };
typedef struct SSpanSnapshot {
    const void *pointer;
    size_t bytes;
    unsigned char content[SNAPSHOT_BYTES];
} SSpanSnapshot;
typedef struct SSnapshot {
    SZrExecIrFunction function;
    SZrExecBcProjection projection;
    unsigned count;
    SSpanSnapshot spans[SNAPSHOT_SPANS];
} SSnapshot;
static SSnapshot g_before;

static void snapshot_span(const void *pointer) {
    SZrOwnerLiveSpan span;
    SSpanSnapshot *copy;
    if (pointer == NULL) return;
    TEST_ASSERT_LESS_THAN_UINT(SNAPSHOT_SPANS, g_before.count);
    TEST_ASSERT_TRUE(ZrTests_ExecOwner_TryLiveSpan(0u,
            ZrTests_ExecOwner_Stats().records, pointer, 1u, 1u, &span));
    TEST_ASSERT_TRUE(span.allocation == pointer);
    TEST_ASSERT_TRUE(span.bytes <= SNAPSHOT_BYTES);
    copy = &g_before.spans[g_before.count++];
    copy->pointer = pointer;
    copy->bytes = span.bytes;
    memcpy(copy->content, pointer, span.bytes);
}

static void snapshot(void) {
    g_before.function = g_function;
    g_before.projection = g_projection;
    g_before.count = 0u;
    snapshot_span(g_function.instructions);
    snapshot_span(g_function.blocks);
    snapshot_span(g_borrowed);
    /* The keeper is a real absent-map NOP/RETURN projection. These are all
     * its allocations, including the reserved invalid value-slot element. */
    snapshot_span(g_projection.opcodes);
    snapshot_span(g_projection.instructions);
    snapshot_span(g_projection.blocks);
    snapshot_span(g_projection.valueSlots);
    snapshot_span(g_projection.slotValues);
}

static int unchanged(void) {
    if (memcmp(&g_before.function, &g_function, sizeof(g_function)) != 0 ||
        memcmp(&g_before.projection, &g_projection, sizeof(g_projection)) != 0) return 0;
    for (unsigned i = 0u; i < g_before.count; ++i) {
        const SSpanSnapshot *span = &g_before.spans[i];
        SZrOwnerLiveSpan live;
        if (!ZrTests_ExecOwner_TryLiveSpan(0u, ZrTests_ExecOwner_Stats().records,
                span->pointer, span->bytes, 1u, &live) ||
            live.allocation != span->pointer || live.bytes != span->bytes) return 0;
        if (memcmp(span->content, span->pointer, span->bytes) != 0) return 0;
    }
    return 1;
}

static void diagnostic_equal(const SZrExecIrDiagnostic *expected,
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

static void build_function(void) {
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
}

void setUp(void) {
    ZrTests_ExecOwner_Reset();
    memset(&g_function, 0, sizeof(g_function));
    memset(&g_projection, 0, sizeof(g_projection));
    g_borrowed = NULL;
}

void tearDown(void) {
    SZrOwnerHeapStats end;
    ZrTests_ExecOwner_Disarm();
    if (g_borrowed != NULL && g_function.gcMap == g_borrowed) {
        g_function.gcMap = NULL;
        g_function.gcMapCount = g_function.gcMapCapacity = 0u;
    }
    free(g_borrowed);
    g_borrowed = NULL;
    ZrParser_ExecBcProjection_Free(&g_projection);
    ZrCore_ExecIr_FreeFunction(&g_function);
    end = ZrTests_ExecOwner_Stats();
    /* Capture actual cleanup first; Recover only makes failed-test teardown safe. */
    ZrTests_ExecOwner_Recover();
    TEST_ASSERT_EQUAL_UINT(0u, end.live);
    TEST_ASSERT_EQUAL_UINT(0u, end.errors);
    ZrTests_ExecOwner_AssertReleased(0u, end.records);
}

static TZrBool consume(unsigned projection, SZrExecIrDiagnostic *diagnostic) {
    if (projection) return ZrParser_ExecIr_BuildProjection(&g_function, &g_projection, diagnostic);
    return zr_oracle_validate(&g_function, diagnostic);
}

static void rejection_matrix(unsigned projection) {
    static const unsigned counts[6] = {2u, 1u, 0u, 1u, 0u, 2u};
    static const unsigned capacities[6] = {2u, 2u, 0u, 1u, 1u, 1u};
    for (unsigned row = 0u; row < 6u; ++row) {
        for (unsigned nullDiagnostic = 0u; nullDiagnostic < 2u; ++nullDiagnostic) {
            SZrExecIrDiagnostic actual, expected = {0}, retryDiagnostic, clear = {0};
            SZrOwnerHeapStats before, after;
            TZrBool accepted, retry;
            int preserved;
            build_function();
            TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildProjection(&g_function, &g_projection, NULL));
            if (row != 3u && row != 4u) {
                g_borrowed = calloc(row == 2u ? 1u : 2u, sizeof(*g_borrowed));
                TEST_ASSERT_NOT_NULL(g_borrowed);
            }
            g_function.gcMap = g_borrowed;
            g_function.gcMapCount = counts[row];
            g_function.gcMapCapacity = capacities[row];
            snapshot();
            memset(&actual, 0xa5, sizeof(actual));
            expected.code = ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT;
            expected.functionToken = g_function.functionToken;
            before = ZrTests_ExecOwner_Stats();
            ZrTests_ExecOwner_Arm(1u);
            accepted = consume(projection, nullDiagnostic ? NULL : &actual);
            after = ZrTests_ExecOwner_Stats();
            preserved = unchanged();
            ZrTests_ExecOwner_Disarm();
            /* Unbind and release every borrowed header before Unity can jump. */
            g_function.gcMap = NULL;
            g_function.gcMapCount = g_function.gcMapCapacity = 0u;
            free(g_borrowed);
            g_borrowed = NULL;
            memset(&retryDiagnostic, 0xa5, sizeof(retryDiagnostic));
            retry = consume(projection, &retryDiagnostic);
            ZrParser_ExecBcProjection_Free(&g_projection);
            ZrCore_ExecIr_FreeFunction(&g_function);
            TEST_ASSERT_FALSE_MESSAGE(accepted, "malformed outer shape accepted");
            TEST_ASSERT_EQUAL_UINT(before.attempts, after.attempts);
            TEST_ASSERT_EQUAL_UINT(before.live, after.live);
            TEST_ASSERT_EQUAL_UINT(before.errors, after.errors);
            TEST_ASSERT_EQUAL_UINT(before.failures, after.failures);
            TEST_ASSERT_TRUE(preserved);
            if (!nullDiagnostic) diagnostic_equal(&expected, &actual);
            TEST_ASSERT_TRUE(retry);
            diagnostic_equal(&clear, &retryDiagnostic);
            ZrTests_ExecOwner_AssertReleased(0u, ZrTests_ExecOwner_Stats().records);
        }
    }
}

static void test_validator_rejects_outer_shapes(void) { rejection_matrix(0u); }
static void test_projection_rejects_outer_shapes(void) { rejection_matrix(1u); }

static void legal_projection(unsigned kind) {
    SZrExecIrDiagnostic diagnostic, clear = {0};
    SZrExecIrGcMap expected = {0};
    SZrExecIrGcMapEntry entries[2] = {
        {.site = 1u, .liveRefSlots = {.start = 0u, .count = 1u},
         .inlineRefOffsets = {.start = 0u, .count = 1u}},
        {.site = 2u, .liveRefSlots = {.start = 1u, .count = 1u},
         .inlineRefOffsets = {.start = 1u, .count = 1u}}
    };
    TZrUInt32 slots[2] = {3u, 7u}, offsets[2] = {8u, 24u};
    unsigned sourceFirst, sourceLast;
    build_function();
    sourceFirst = 0u;
    if (kind != 0u) {
        g_function.gcMap = calloc(1u, sizeof(*g_function.gcMap));
        TEST_ASSERT_NOT_NULL(g_function.gcMap);
        g_function.gcMapCount = g_function.gcMapCapacity = 1u;
        if (kind == 2u) {
            SZrExecIrGcMap *map = g_function.gcMap;
            map->entries = calloc(3u, sizeof(*map->entries));
            map->slotIndexPool = calloc(3u, sizeof(*map->slotIndexPool));
            map->inlineRefOffsetPool = calloc(3u, sizeof(*map->inlineRefOffsetPool));
            TEST_ASSERT_NOT_NULL(map->entries);
            TEST_ASSERT_NOT_NULL(map->slotIndexPool);
            TEST_ASSERT_NOT_NULL(map->inlineRefOffsetPool);
            map->entryCount = map->slotIndexCount = map->inlineRefOffsetCount = 2u;
            map->entryCapacity = map->slotIndexCapacity = map->inlineRefOffsetCapacity = 3u;
            memcpy(map->entries, entries, sizeof(entries));
            memcpy(map->slotIndexPool, slots, sizeof(slots));
            memcpy(map->inlineRefOffsetPool, offsets, sizeof(offsets));
        }
        expected = *g_function.gcMap;
    }
    sourceLast = ZrTests_ExecOwner_Stats().records;
    memset(&diagnostic, 0xa5, sizeof(diagnostic));
    TEST_ASSERT_TRUE(zr_oracle_validate(&g_function, &diagnostic));
    diagnostic_equal(&clear, &diagnostic);
    memset(&diagnostic, 0xa5, sizeof(diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BuildProjection(&g_function, &g_projection, &diagnostic));
    diagnostic_equal(&clear, &diagnostic);
    TEST_ASSERT_EQUAL_UINT32(g_function.functionToken, g_projection.functionToken);
    TEST_ASSERT_EQUAL_UINT64(g_function.signatureHash, g_projection.signatureHash);
    TEST_ASSERT_EQUAL_UINT32(kind != 0u, g_projection.gcMapPresent);
    TEST_ASSERT_EQUAL_UINT32(kind != 0u, g_projection.gcMapCount);
    if (kind != 0u) {
        TEST_ASSERT_TRUE(&g_projection.gcMap != g_function.gcMap);
        TEST_ASSERT_EQUAL_UINT32(expected.safepointId, g_projection.gcMap.safepointId);
        TEST_ASSERT_EQUAL_UINT32(expected.sourceId, g_projection.gcMap.sourceId);
        TEST_ASSERT_EQUAL_UINT32(expected.rootRange.start, g_projection.gcMap.rootRange.start);
        TEST_ASSERT_EQUAL_UINT32(expected.rootRange.count, g_projection.gcMap.rootRange.count);
    }
    if (kind == 2u) {
        TEST_ASSERT_TRUE(expected.entries != g_projection.gcMap.entries);
        TEST_ASSERT_TRUE(expected.slotIndexPool != g_projection.gcMap.slotIndexPool);
        TEST_ASSERT_TRUE(expected.inlineRefOffsetPool != g_projection.gcMap.inlineRefOffsetPool);
        g_projection.gcMap.slotIndexPool[0] = 11u;
        g_projection.gcMap.entries[0].site = 2u;
        g_projection.gcMap.inlineRefOffsetPool[0] = 16u;
        TEST_ASSERT_EQUAL_UINT32(3u, g_function.gcMap->slotIndexPool[0]);
        TEST_ASSERT_EQUAL_UINT32(1u, g_function.gcMap->entries[0].site);
        TEST_ASSERT_EQUAL_UINT32(8u, g_function.gcMap->inlineRefOffsetPool[0]);
        g_projection.gcMap.slotIndexPool[0] = slots[0];
        g_projection.gcMap.entries[0] = entries[0];
        g_projection.gcMap.inlineRefOffsetPool[0] = offsets[0];
    }
    ZrCore_ExecIr_FreeFunction(&g_function);
    ZrTests_ExecOwner_AssertReleased(sourceFirst, sourceLast);
    TEST_ASSERT_EQUAL_UINT32(kind == 2u ? 2u : 0u, g_projection.gcMap.entryCount);
    TEST_ASSERT_EQUAL_UINT32(kind == 2u ? 2u : 0u, g_projection.gcMap.slotIndexCount);
    TEST_ASSERT_EQUAL_UINT32(kind == 2u ? 2u : 0u, g_projection.gcMap.inlineRefOffsetCount);
    TEST_ASSERT_EQUAL_UINT32(g_projection.gcMap.entryCount, g_projection.gcMap.entryCapacity);
    TEST_ASSERT_EQUAL_UINT32(g_projection.gcMap.slotIndexCount, g_projection.gcMap.slotIndexCapacity);
    TEST_ASSERT_EQUAL_UINT32(g_projection.gcMap.inlineRefOffsetCount, g_projection.gcMap.inlineRefOffsetCapacity);
    if (kind == 2u) {
        for (unsigned i = 0u; i < 2u; ++i) {
            TEST_ASSERT_EQUAL_UINT32(entries[i].site, g_projection.gcMap.entries[i].site);
            TEST_ASSERT_EQUAL_UINT32(entries[i].liveRefSlots.start, g_projection.gcMap.entries[i].liveRefSlots.start);
            TEST_ASSERT_EQUAL_UINT32(entries[i].liveRefSlots.count, g_projection.gcMap.entries[i].liveRefSlots.count);
            TEST_ASSERT_EQUAL_UINT32(entries[i].inlineRefOffsets.start, g_projection.gcMap.entries[i].inlineRefOffsets.start);
            TEST_ASSERT_EQUAL_UINT32(entries[i].inlineRefOffsets.count, g_projection.gcMap.entries[i].inlineRefOffsets.count);
            TEST_ASSERT_EQUAL_UINT32(slots[i], g_projection.gcMap.slotIndexPool[i]);
            TEST_ASSERT_EQUAL_UINT32(offsets[i], g_projection.gcMap.inlineRefOffsetPool[i]);
        }
    } else {
        TEST_ASSERT_NULL(g_projection.gcMap.entries);
        TEST_ASSERT_NULL(g_projection.gcMap.slotIndexPool);
        TEST_ASSERT_NULL(g_projection.gcMap.inlineRefOffsetPool);
    }
    ZrParser_ExecBcProjection_Free(&g_projection);
    ZrParser_ExecBcProjection_Free(&g_projection);
    ZrCore_ExecIr_FreeFunction(&g_function);
}
static void test_absent_projection(void) { legal_projection(0u); }
static void test_empty_projection(void) { legal_projection(1u); }
static void test_multi_entry_projection(void) { legal_projection(2u); }

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_validator_rejects_outer_shapes);
    RUN_TEST(test_projection_rejects_outer_shapes);
    RUN_TEST(test_absent_projection);
    RUN_TEST(test_empty_projection);
    RUN_TEST(test_multi_entry_projection);
    return UNITY_END();
}
