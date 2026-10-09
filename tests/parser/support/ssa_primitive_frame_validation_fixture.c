#include "ssa_primitive_frame_validation_fixture.h"
#include <stdio.h>
#include <string.h>
#include "unity.h"
SZrExecIrFunction g_pfFunctions[2];
SZrSemanticContext g_pfContext;
SZrCanonicalTypeNode g_pfNodes[2];
SZrExecIrLayout g_pfRows[2];
SZrExecIrPackedFrameLayout g_pfPacked;
/* Saved owning headers identify real producer buffers even during bounded header mutations. */
static SZrExecIrFunction g_owners[2];
static SZrExecIrFrameLayout g_frames[2];
static unsigned g_producerRecords, g_liveRecords[256], g_liveCount;
void Pf_Diagnostic(const SZrExecIrDiagnostic *e, const SZrExecIrDiagnostic *a) {
    TEST_ASSERT_EQUAL_UINT(e->code, a->code);
    TEST_ASSERT_EQUAL_UINT32(e->functionToken, a->functionToken);
    TEST_ASSERT_EQUAL_UINT32(e->blockId, a->blockId);
    TEST_ASSERT_EQUAL_UINT32(e->instructionId, a->instructionId);
    TEST_ASSERT_EQUAL_UINT32(e->sourceId, a->sourceId);
    TEST_ASSERT_EQUAL_UINT32(e->expectedVersion, a->expectedVersion);
    TEST_ASSERT_EQUAL_UINT32(e->actualVersion, a->actualVersion);
    TEST_ASSERT_EQUAL_UINT64(e->expectedHash, a->expectedHash);
    TEST_ASSERT_EQUAL_UINT64(e->actualHash, a->actualHash);
}
SZrExecIrDiagnostic Pf_Error(EZrExecutionDiagnosticCode code) {
    SZrExecIrDiagnostic d = {0};
    d.code = code;
    d.functionToken = g_pfFunctions[0].functionToken;
    return d;
}
static void observe_bytes(SPfObservation *o, const void *p, size_t bytes) {
    if (bytes > PF_BYTES - o->length || (bytes && p == ZR_NULL)) {
        o->error = 1u;
        return;
    }
    if (bytes)
        memcpy(o->bytes + o->length, p, bytes);
    o->length += (unsigned) bytes;
}
void Pf_Observe(SPfObservation *o) {
    memset(o, 0, sizeof(*o));
    o->heap = ZrTests_ExecOwner_Stats();
    observe_bytes(o, &g_pfContext, sizeof(g_pfContext));
    observe_bytes(o, g_pfNodes, sizeof(g_pfNodes));
    observe_bytes(o, g_pfRows, sizeof(g_pfRows));
    for (unsigned i = 0u; i < 2u; ++i) {
        const SZrExecIrFunction *f = &g_owners[i];
        observe_bytes(o, &g_pfFunctions[i], sizeof(g_pfFunctions[i]));
#define POOL(field, count) observe_bytes(o, f->field, (size_t) f->count * sizeof(*f->field))
        POOL(values, valueCapacity);
        POOL(instructions, instructionCapacity);
        POOL(blocks, blockCapacity);
        POOL(operands, operandCapacity);
        POOL(results, resultCapacity);
        POOL(sourceMaps, sourceMapCapacity);
#undef POOL
        observe_bytes(o, f->frameLayout, sizeof(*f->frameLayout));
        observe_bytes(o, g_frames[i].slots, (size_t) g_frames[i].slotCapacity * sizeof(*g_frames[i].slots));
    }
}
void Pf_Healthy(const SPfObservation *a, const SPfObservation *b) {
    TEST_ASSERT_EQUAL_UINT(0u, a->error);
    TEST_ASSERT_EQUAL_UINT(0u, b->error);
    TEST_ASSERT_EQUAL_UINT(a->length, b->length);
    TEST_ASSERT_EQUAL_MEMORY(a->bytes, b->bytes, a->length);
    TEST_ASSERT_EQUAL_UINT(0u, a->heap.errors);
    TEST_ASSERT_EQUAL_UINT(0u, b->heap.errors);
    TEST_ASSERT_EQUAL_UINT(a->heap.live, b->heap.live);
    for (unsigned i = 0u; i < g_liveCount; ++i)
        ZrTests_ExecOwner_AssertLive(g_liveRecords[i], g_liveRecords[i] + 1u);
    ZrTests_ExecOwner_AssertReleased(a->heap.records, b->heap.records);
}
SPfResult Pf_Call(TZrUInt32 limit) {
    SPfResult r;
    Pf_Observe(&r.before);
    memset(&r.diagnostic, 0xA5, sizeof(r.diagnostic));
    r.success = ZrParser_ExecIr_ValidatePrimitiveSourceFrame(&g_pfFunctions[0], &g_pfContext, g_pfRows, 2u, limit,
                                                             &r.diagnostic);
    ZrTests_ExecOwner_Disarm();
    Pf_Observe(&r.after);
    return r;
}
void Pf_Refused(const SPfResult *r, const SZrExecIrDiagnostic *expected) {
    Pf_Healthy(&r->before, &r->after);
    TEST_ASSERT_FALSE(r->success);
    Pf_Diagnostic(expected, &r->diagnostic);
}
void Pf_Feature(void) {
    SPfResult r = Pf_Call(0u);
    SZrExecIrDiagnostic d = {0};
    Pf_Healthy(&r.before, &r.after);
    if (!r.success) {
        d = Pf_Error(ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
        Pf_Diagnostic(&d, &r.diagnostic);
        TEST_ASSERT_EQUAL_UINT(r.before.heap.attempts, r.after.heap.attempts);
        fprintf(stderr, "PRIMITIVE_FRAME_FEATURE code=%u attempts=%u\n", (unsigned) r.diagnostic.code,
                r.after.heap.attempts - r.before.heap.attempts);
    }
    TEST_ASSERT_TRUE_MESSAGE(r.success, "actual const primitive frame validation must succeed");
    memset(&d, 0, sizeof(d));
    Pf_Diagnostic(&d, &r.diagnostic);
}
void Pf_Request(SZrExecIrPackedFrameRequest *r, SZrExecIrPackedValue values[2], TZrUInt32 limit) {
    memset(r, 0, sizeof(*r));
    memset(values, 0, 2u * sizeof(*values));
    r->functionToken = g_pfFunctions[0].functionToken;
    r->values = values;
    r->valueCount = 2u;
    r->frameByteLimit = limit;
    for (unsigned i = 0u; i < 2u; ++i) {
        values[i].valueId = g_pfFunctions[0].values[i].id;
        values[i].typeToken = g_pfFunctions[0].values[i].typeToken;
        values[i].slotClass = ZR_EXEC_IR_PACKED_SLOT_SCALAR;
        values[i].byteSize = g_pfRows[i].byteSize;
        values[i].byteAlign = g_pfRows[i].byteAlign;
        values[i].liveEnd = g_pfFunctions[0].instructionCount;
    }
}
SZrOwnerHeapEvent Pf_FailedEvent(unsigned first, unsigned last) {
    SZrOwnerHeapEvent result = {0};
    unsigned failures = 0u;
    for (unsigned i = first; i < last; ++i) {
        SZrOwnerHeapEvent event = ZrTests_ExecOwner_Event(i);
        if (event.failed) {
            result = event;
            ++failures;
        }
    }
    TEST_ASSERT_EQUAL_UINT(1u, failures);
    return result;
}
static void initialize_spare(SZrExecIrFunction *f) {
#define SPARE(field, count, capacity)                                                                                  \
    do {                                                                                                               \
        TEST_ASSERT_TRUE(f->capacity >= f->count);                                                                     \
        if (f->capacity > f->count) {                                                                                  \
            TEST_ASSERT_NOT_NULL(f->field);                                                                            \
            memset(&f->field[f->count], 0, (size_t) (f->capacity - f->count) * sizeof(*f->field));                     \
        }                                                                                                              \
    } while (0)
    SPARE(values, valueCount, valueCapacity);
    SPARE(instructions, instructionCount, instructionCapacity);
    SPARE(blocks, blockCount, blockCapacity);
    SPARE(operands, operandCount, operandCapacity);
    SPARE(results, resultCount, resultCapacity);
    SPARE(sourceMaps, sourceMapCount, sourceMapCapacity);
#undef SPARE
    if (f->frameLayout != ZR_NULL) {
        SZrExecIrFrameLayout *frame = f->frameLayout;
        TEST_ASSERT_TRUE(frame->slotCapacity >= frame->slotCount);
        if (frame->slotCapacity > frame->slotCount) {
            TEST_ASSERT_NOT_NULL(frame->slots);
            memset(&frame->slots[frame->slotCount], 0,
                   (size_t) (frame->slotCapacity - frame->slotCount) * sizeof(*frame->slots));
        }
    }
}
static void construct(void) {
    SZrExecIrFunction *f = &g_pfFunctions[0];
    SZrExecIrInstruction ins = {0};
    SZrExecIrRange result, operand;
    TZrExecIrValueId ids[2];
    TZrExecIrBlockId block;
    TZrExecIrInstructionId definitions[2], ret;
    SZrExecIrSourceMap maps[3];
    SZrExecIrDiagnostic diagnostic, zero = {0};
    memset(maps, 0, sizeof(maps));
    f->id = 1u;
    f->functionToken = 0x06000031u;
    f->signatureHash = 37u;
    f->contract.schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    f->contract.abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    f->contract.logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    f->contract.generation = 1u;
    f->contract.targetToken = f->functionToken;
    f->contract.signatureHash = f->signatureHash;
    block = ZrCore_ExecIr_FunctionAddBlock(f, ZR_EXEC_IR_BLOCK_FLAG_ENTRY);
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, block);
    f->entryBlockId = block;
    for (unsigned i = 0u; i < 2u; ++i) {
        ids[i] = ZrCore_ExecIr_FunctionAddValue(f, g_pfNodes[i].id, ZR_EXEC_IR_OWNERSHIP_UNKNOWN,
                                                ZR_EXEC_IR_NULLABILITY_NONNULL);
        TEST_ASSERT_NOT_EQUAL_UINT32(0u, ids[i]);
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendResults(f, &ids[i], 1u, &result));
        memset(&ins, 0, sizeof(ins));
        ins.opcode = ZR_EXEC_IR_OPCODE_CONSTANT;
        ins.typeToken = g_pfNodes[i].id;
        ins.resultRange = result;
        ins.sourceId = 501u + i;
        TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(f, &ins, &definitions[i]));
        TEST_ASSERT_EQUAL_UINT32(definitions[i], f->values[ids[i] - 1u].definition);
    }
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendOperands(f, &ids[0], 1u, &operand));
    memset(&ins, 0, sizeof(ins));
    ins.opcode = ZR_EXEC_IR_OPCODE_RETURN;
    ins.typeToken = g_pfNodes[0].id;
    ins.operandRange = operand;
    ins.sourceId = 503u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_FunctionAppendInstruction(f, &ins, &ret));
    f->blocks[block - 1u].instructionRange.start = definitions[0] - 1u;
    f->blocks[block - 1u].instructionRange.count = ret - definitions[0] + 1u;
    f->blocks[block - 1u].terminatorInstructionId = ret;
    for (unsigned i = 0u; i < 3u; ++i) {
        maps[i].sourceId = 501u + i;
        maps[i].instructionId = i < 2u ? definitions[i] : ret;
        maps[i].startOffset = i * 10u;
        maps[i].endOffset = maps[i].startOffset + 5u;
        maps[i].startLine = maps[i].endLine = i + 1u;
        maps[i].endColumn = 5u;
    }
    /* Core exposes no source-map append API. Clone creates owned maps from live declared typed rows. */
    SZrExecIrFunction borrowed = *f;
    borrowed.sourceMaps = maps;
    borrowed.sourceMapCount = borrowed.sourceMapCapacity = 3u;
    TZrBool copied = ZrCore_ExecIr_CloneFunction(&borrowed, &g_pfFunctions[1], &diagnostic);
    if (copied) {
        SZrExecIrFunction prior = *f;
        *f = g_pfFunctions[1];
        g_pfFunctions[1] = prior;
        ZrCore_ExecIr_FreeFunction(&g_pfFunctions[1]);
    }
    /* borrowed owns nothing and is never used after the registered prior owner is freed. */
    TEST_ASSERT_TRUE_MESSAGE(copied, "actual Core source-map clone prerequisite");
    Pf_Diagnostic(&zero, &diagnostic);
    for (unsigned i = 0u; i < 3u; ++i) {
        TEST_ASSERT_EQUAL_UINT32(f->instructions[i].sourceId, f->sourceMaps[i].sourceId);
        TEST_ASSERT_EQUAL_UINT32(i + 1u, f->sourceMaps[i].instructionId);
    }
    TZrUInt32 capacity = f->valueCount + 1u;
    SZrExecIrValue *grown = ZrTests_ExecOwner_Realloc(f->values, (size_t) capacity * sizeof(*grown));
    if (grown != ZR_NULL) {
        f->values = grown;
        f->valueCapacity = capacity;
        memset(&grown[f->valueCount], 0, sizeof(*grown));
    }
    TEST_ASSERT_NOT_NULL(grown);
}
void Pf_SetUp(void) {
    SZrExecIrDiagnostic d, z = {0};
    ZrTests_ExecOwner_AssertBalanced();
    ZrTests_ExecOwner_Reset();
    memset(g_owners, 0, sizeof(g_owners));
    memset(g_frames, 0, sizeof(g_frames));
    g_producerRecords = 0u;
    g_liveCount = 0u;
    for (unsigned i = 0u; i < 2u; ++i)
        ZrCore_ExecIr_FunctionInit(&g_pfFunctions[i]);
    ZrParser_ExecIr_PackedFrameLayoutInit(&g_pfPacked);
    memset(&g_pfContext, 0, sizeof(g_pfContext));
    memset(g_pfNodes, 0, sizeof(g_pfNodes));
    memset(g_pfRows, 0, sizeof(g_pfRows));
    for (unsigned i = 0u; i < 2u; ++i) {
        g_pfNodes[i].id = i ? 271u : 113u;
        g_pfNodes[i].kind = ZR_CANONICAL_TYPE_PRIMITIVE;
        g_pfNodes[i].structuralHash = 700u + i;
        g_pfNodes[i].data.primitive.valueType = ZR_VALUE_TYPE_INT64;
        g_pfRows[i].id = 21u + i;
        g_pfRows[i].typeToken = g_pfNodes[i].id;
        g_pfRows[i].byteSize = i ? 16u : 8u;
        g_pfRows[i].byteAlign = i ? 16u : 8u;
        g_pfRows[i].layoutHash = 900u + i;
    }
    g_pfContext.canonicalTypes.head = (TZrByte *) g_pfNodes;
    g_pfContext.canonicalTypes.elementSize = sizeof(*g_pfNodes);
    g_pfContext.canonicalTypes.length = 2u;
    g_pfContext.canonicalTypes.capacity = 2u;
    g_pfContext.canonicalTypes.isValid = ZR_TRUE;
    construct();
    initialize_spare(&g_pfFunctions[0]);
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_ExecIr_VerifyFunction(&g_pfFunctions[0], ZR_EXEC_IR_VERIFY_ALL, &d),
                             "actual Core constructed prerequisite");
    Pf_Diagnostic(&z, &d);
    TEST_ASSERT_TRUE_MESSAGE(
            ZrParser_ExecIr_AttachPrimitiveSourceFrame(&g_pfFunctions[0], &g_pfContext, g_pfRows, 2u, 0u, &d),
            "actual Attach prerequisite");
    Pf_Diagnostic(&z, &d);
    {
        SZrExecIrFrameLayout *frame = g_pfFunctions[0].frameLayout;
        TZrUInt32 capacity = frame->slotCount + 1u;
        SZrExecIrFrameSlot *grown = ZrTests_ExecOwner_Realloc(frame->slots, (size_t) capacity * sizeof(*grown));
        if (grown != ZR_NULL) {
            frame->slots = grown;
            frame->slotCapacity = capacity;
            memset(&grown[frame->slotCount], 0, sizeof(*grown));
        }
        TEST_ASSERT_NOT_NULL(grown);
        TEST_ASSERT_TRUE(frame->slotCapacity > frame->slotCount);
    }
    initialize_spare(&g_pfFunctions[0]);
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_ExecIr_VerifyFunction(&g_pfFunctions[0], ZR_EXEC_IR_VERIFY_ALL, &d),
                             "actual framed Core Verify prerequisite");
    Pf_Diagnostic(&z, &d);
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_ExecIr_CloneFunction(&g_pfFunctions[0], &g_pfFunctions[1], &d),
                             "actual frame clone prerequisite");
    initialize_spare(&g_pfFunctions[1]);
    Pf_Diagnostic(&z, &d);
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_ExecIr_VerifyFunction(&g_pfFunctions[1], ZR_EXEC_IR_VERIFY_ALL, &d),
                             "actual clone Core Verify prerequisite");
    Pf_Diagnostic(&z, &d);
    for (unsigned i = 0u; i < 2u; ++i) {
        g_owners[i] = g_pfFunctions[i];
        g_frames[i] = *g_pfFunctions[i].frameLayout;
    }
    /* Successful preparation also has freed temporary records; only surviving IDs are input owners. */
    g_producerRecords = ZrTests_ExecOwner_Stats().records;
    for (unsigned i = 0u; i < g_producerRecords; ++i)
        if (ZrTests_ExecOwner_RecordFrees(i) == 0u) {
            TEST_ASSERT_LESS_THAN_UINT(256u, g_liveCount);
            g_liveRecords[g_liveCount++] = i;
        }
}
void Pf_TearDown(void) {
    SZrOwnerHeapStats s;
    ZrTests_ExecOwner_Disarm();
    ZrParser_ExecIr_PackedFrameLayoutFree(&g_pfPacked);
    for (unsigned i = 0u; i < 2u; ++i)
        ZrCore_ExecIr_FreeFunction(&g_pfFunctions[i]);
    s = ZrTests_ExecOwner_Stats();
    if (s.errors || s.live)
        fprintf(stderr, "PRIMITIVE_FRAME_LEDGER errors=%u live=%u before_recovery=1\n", s.errors, s.live);
    else
        ZrTests_ExecOwner_AssertBalanced();
    ZrTests_ExecOwner_Recover();
    TEST_ASSERT_EQUAL_UINT(0u, s.errors);
    TEST_ASSERT_EQUAL_UINT(0u, s.live);
    TEST_ASSERT_EQUAL_UINT(s.records, s.frees);
    ZrTests_ExecOwner_AssertBalanced();
}
