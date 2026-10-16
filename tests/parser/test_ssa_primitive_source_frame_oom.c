#include <stdlib.h>
#include <string.h>

#define ZR_TEST_SSA_PRIMITIVE_SOURCE_FRAME_OOM_NO_REDIRECT
#include "support/ssa_primitive_source_frame_oom_shim.h"
#undef ZR_TEST_SSA_PRIMITIVE_SOURCE_FRAME_OOM_NO_REDIRECT

#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_parser/canonical_type.h"
#include "exec_ir/exec_ir_dead_source_places.h"
#include "exec_ir/exec_ir_host_primitive_layout.h"
#include "exec_ir/source/exec_ir_source_frame.h"
#include "support/ssa_literal_script_fixture.h"

typedef enum EAllocationKind {
    ALLOCATION_DISABLED = 0,
    ALLOCATION_VALUES,
    ALLOCATION_HEADER
} EAllocationKind;

typedef enum EAllocationOwner {
    OWNER_UNUSED = 0,
    OWNER_TEMPORARY,
    OWNER_FREED,
    OWNER_GRAPH
} EAllocationOwner;

enum { ALLOCATION_RECORD_LIMIT = 4 };
typedef struct SAllocationRecord {
    void *pointer;
    EAllocationKind kind;
    EAllocationOwner owner;
} SAllocationRecord;

typedef struct SAllocationLedger {
    EAllocationKind failure;
    unsigned failureHits, callocCalls, mallocCalls, nonnullFrees, nullFrees;
    unsigned allocationCount, transfers, errors, cleanupCalls;
    size_t callocCount, callocWidth, mallocSize;
    TZrBool cleanupHadSlots, cleanupHadMaps, cleanupCleared;
    SAllocationRecord records[ALLOCATION_RECORD_LIMIT];
} SAllocationLedger;

static SZrState *g_state;
static SZrSsaLiteralScriptFixture g_fixture;
static SZrExecIrFunction g_output;
static SAllocationLedger g_ledger; /* Failure injection defaults to disabled. */
static SZrExecIrFrameLayout *g_publishedHeader;

/* No wrapper asserts or allocates bookkeeping: Unity may longjmp on assertion.
 * This ledger counts only allocation/free call sites in the source-frame TU. */
static void record_allocation(void *pointer, EAllocationKind kind) {
    SAllocationRecord *record;
    if (pointer == ZR_NULL) return;
    if (g_ledger.allocationCount == ALLOCATION_RECORD_LIMIT) {
        ++g_ledger.errors;
        return;
    }
    record = &g_ledger.records[g_ledger.allocationCount++];
    record->pointer = pointer;
    record->kind = kind;
    record->owner = OWNER_TEMPORARY;
}

void *ZrTests_PrimitiveSourceFrame_Calloc(size_t count, size_t width) {
    void *pointer;
    ++g_ledger.callocCalls;
    g_ledger.callocCount = count;
    g_ledger.callocWidth = width;
    if (g_ledger.failure == ALLOCATION_VALUES && g_ledger.callocCalls == 1u) {
        ++g_ledger.failureHits;
        return ZR_NULL;
    }
    pointer = calloc(count, width);
    record_allocation(pointer, ALLOCATION_VALUES);
    return pointer;
}

void *ZrTests_PrimitiveSourceFrame_Malloc(size_t size) {
    void *pointer;
    ++g_ledger.mallocCalls;
    g_ledger.mallocSize = size;
    if (g_ledger.failure == ALLOCATION_HEADER && g_ledger.mallocCalls == 1u) {
        ++g_ledger.failureHits;
        return ZR_NULL;
    }
    pointer = malloc(size);
    record_allocation(pointer, ALLOCATION_HEADER);
    return pointer;
}

void ZrTests_PrimitiveSourceFrame_Free(void *pointer) {
    unsigned i;
    if (pointer == ZR_NULL) {
        ++g_ledger.nullFrees;
        free(pointer);
        return;
    }
    for (i = 0u; i < g_ledger.allocationCount; ++i) {
        SAllocationRecord *record = &g_ledger.records[i];
        if (record->owner == OWNER_TEMPORARY && record->pointer == pointer) {
            record->pointer = ZR_NULL;
            record->owner = OWNER_FREED;
            ++g_ledger.nonnullFrees;
            free(pointer);
            return;
        }
    }
    ++g_ledger.errors;
    free(pointer);
}

void ZrTests_PrimitiveSourceFrame_PackedFree(SZrExecIrPackedFrameLayout *packed) {
    ++g_ledger.cleanupCalls;
    if (packed == ZR_NULL) {
        ++g_ledger.errors;
        ZrParser_ExecIr_PackedFrameLayoutFree(packed);
        return;
    }
    g_ledger.cleanupHadSlots = (TZrBool)(packed->frame.slots != ZR_NULL);
    g_ledger.cleanupHadMaps = (TZrBool)(packed->logicalValueIds != ZR_NULL &&
            packed->logicalToPhysical != ZR_NULL && packed->slotClasses != ZR_NULL);
    ZrParser_ExecIr_PackedFrameLayoutFree(packed);
    /* packed is still a live stack header. Its freed arrays are never read.
     * This observes the real helper call and owner clearing, not heap frees. */
    g_ledger.cleanupCleared = (TZrBool)(packed->logicalValueIds == ZR_NULL &&
            packed->logicalToPhysical == ZR_NULL && packed->slotClasses == ZR_NULL &&
            packed->frame.slots == ZR_NULL && packed->frame.slotCount == 0u &&
            packed->frame.slotCapacity == 0u && packed->frame.layoutHash == 0u &&
            packed->frame.storageSlotCount == 0u && packed->frame.logicalSlotCount == 0u &&
            packed->frame.parameterPrefixCount == 0u && packed->frame.returnBufferOffset == 0u &&
            packed->frame.frameByteSize == 0u && packed->frame.frameByteAlign == 0u &&
            packed->frame.parameterCount == 0u && packed->frame.localCount == 0u);
}

static unsigned temporary_owners(void) {
    unsigned i, count = 0u;
    for (i = 0u; i < g_ledger.allocationCount; ++i)
        if (g_ledger.records[i].owner == OWNER_TEMPORARY) ++count;
    return count;
}

/* Register any actual publication immediately after Attach, before asserting.
 * Core teardown owns this header; its unwrapped free is not counted here. */
static void register_publication(void) {
    unsigned i;
    if (g_output.frameLayout == ZR_NULL) return;
    g_publishedHeader = g_output.frameLayout;
    for (i = 0u; i < g_ledger.allocationCount; ++i) {
        SAllocationRecord *record = &g_ledger.records[i];
        if (record->owner == OWNER_TEMPORARY && record->pointer == g_publishedHeader) {
            if (record->kind != ALLOCATION_HEADER) ++g_ledger.errors;
            record->pointer = ZR_NULL;
            record->owner = OWNER_GRAPH;
            ++g_ledger.transfers;
            return;
        }
    }
    ++g_ledger.errors;
}

void setUp(void) {
    memset(&g_ledger, 0, sizeof(g_ledger));
    g_publishedHeader = ZR_NULL;
    ZrCore_ExecIr_FunctionInit(&g_output);
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrTests_SsaLiteralScriptFixture_Init(&g_fixture, g_state);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    unsigned i;
    g_ledger.failure = ALLOCATION_DISABLED;
    g_publishedHeader = ZR_NULL;
    ZrCore_ExecIr_FreeFunction(&g_output);
    /* Preserve cleanup even if a failed assertion found a leaked direct owner.
     * This fallback is not a wrapped-free observation or a passing assertion. */
    for (i = 0u; i < g_ledger.allocationCount; ++i) {
        SAllocationRecord *record = &g_ledger.records[i];
        if (record->owner == OWNER_TEMPORARY) {
            free(record->pointer);
            record->pointer = ZR_NULL;
            record->owner = OWNER_FREED;
        }
    }
    ZrTests_SsaLiteralScriptFixture_Free(&g_fixture);
    if (g_state != ZR_NULL) ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static TZrUInt64 bytes(TZrUInt64 hash, const void *data, size_t size) {
    return ZrTests_SsaLiteralScriptFixture_DigestBytes(hash, data, size);
}

static TZrUInt64 function_digest(const SZrExecIrFunction *function) {
    TZrUInt64 hash = ZrTests_SsaLiteralScriptFixture_FunctionDigest(function);
    if (function->frameLayout != ZR_NULL) {
        const SZrExecIrFrameLayout *frame = function->frameLayout;
        hash = bytes(hash, frame, sizeof(*frame));
        hash = bytes(hash, frame->slots, (size_t)frame->slotCount * sizeof(*frame->slots));
    }
    return hash;
}

static TZrUInt64 module_digest(void) {
    TZrUInt64 hash = bytes(0u, &g_fixture.module, sizeof(g_fixture.module));
    hash = bytes(hash, g_fixture.module.constants,
            (size_t)g_fixture.module.constantCount * sizeof(*g_fixture.module.constants));
    hash = bytes(hash, g_fixture.module.layouts,
            (size_t)g_fixture.module.layoutCount * sizeof(*g_fixture.module.layouts));
    return bytes(hash, g_fixture.module.sourceMaps,
            (size_t)g_fixture.module.sourceMapCount * sizeof(*g_fixture.module.sourceMaps));
}

static void verify(const SZrExecIrFunction *function) {
    SZrExecIrDiagnostic diagnostic = {0};
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_ExecIr_VerifyFunction(function,
            ZR_EXEC_IR_VERIFY_ALL, &diagnostic), "Actual live graph VERIFY_ALL");
}

static void prepare(void) {
    SZrExecIrDiagnostic diagnostic = {0};
    SZrExecIrLayout row;
    SZrExecIrRange range;
    SZrExecIrFunction *original;
    const SZrCanonicalTypeNode *callable, *primitive;
    TZrTypeId typeId;
    TZrUInt64 sourceBefore;
    ZrTests_SsaLiteralScriptFixture_Prepare(&g_fixture, ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    original = ZrTests_SsaLiteralScriptFixture_Function(&g_fixture);
    verify(original);
    sourceBefore = ZrTests_SsaLiteralScriptFixture_SourceDigest(&g_fixture);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &g_fixture.compiler.preSemanticIr, g_fixture.compiler.semanticContext,
            original, &g_output, &diagnostic), "Actual source dead-place compaction");
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, ZrTests_SsaLiteralScriptFixture_SourceDigest(&g_fixture));
    verify(&g_output);
    ZrTests_SsaLiteralScriptFixture_AssertSourceMaps(&g_fixture, &g_output);
    callable = ZrParser_CanonicalType_Find(g_fixture.compiler.semanticContext,
            g_fixture.compiler.preSemanticIr.callableTypeId);
    TEST_ASSERT_NOT_NULL(callable);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, callable->kind);
    typeId = callable->data.function.returnTypeId;
    primitive = ZrParser_CanonicalType_Find(g_fixture.compiler.semanticContext, typeId);
    TEST_ASSERT_NOT_NULL(primitive);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_PRIMITIVE, primitive->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, primitive->data.primitive.valueType);
    TEST_ASSERT_EQUAL_UINT32(1u, g_output.valueCount);
    TEST_ASSERT_EQUAL_UINT32(typeId, g_output.values[0].typeToken);
    TEST_ASSERT_NULL(g_output.frameLayout);
    TEST_ASSERT_EQUAL_UINT32(0u, g_fixture.module.layoutCount);
    TEST_ASSERT_EQUAL_UINT32(0u, g_fixture.module.layoutCapacity);
    TEST_ASSERT_NULL(g_fixture.module.layouts);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_MakeHostPrimitiveLayout(
            g_fixture.compiler.semanticContext, typeId, 1u, &row, &diagnostic),
            "Actual primitive host layout producer");
    TEST_ASSERT_EQUAL_UINT32(sizeof(TZrInt64), row.byteSize);
    TEST_ASSERT_EQUAL_UINT32(alignof(TZrInt64), row.byteAlign);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, row.layoutHash);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ModuleAppendLayout(&g_fixture.module, &row, 1u, &range));
    TEST_ASSERT_EQUAL_UINT32(0u, range.offset);
    TEST_ASSERT_EQUAL_UINT32(1u, range.count);
    TEST_ASSERT_EQUAL_UINT32(1u, g_fixture.module.layoutCount);
    TEST_ASSERT_NOT_NULL(g_fixture.module.layouts);
    TEST_ASSERT_EQUAL_MEMORY(&row, &g_fixture.module.layouts[0], sizeof(row));
}

typedef struct SObservation {
    SZrExecIrFunction header; /* Observation only; never restored or freed. */
    SZrExecIrFunction *address;
    TZrUInt64 function, body, original, source, module;
} SObservation;

static TZrUInt64 body_digest(const SZrExecIrFunction *function) {
    SZrExecIrFunction header;
    memcpy(&header, function, sizeof(header));
    header.frameLayout = ZR_NULL;
    header.contract.layoutHash = 0u;
    return ZrTests_SsaLiteralScriptFixture_FunctionDigest(&header);
}

static SObservation observe(void) {
    SObservation observation;
    memcpy(&observation.header, &g_output, sizeof(observation.header));
    observation.address = &g_output;
    observation.function = function_digest(&g_output);
    observation.body = body_digest(&g_output);
    observation.original = function_digest(ZrTests_SsaLiteralScriptFixture_Function(&g_fixture));
    observation.source = ZrTests_SsaLiteralScriptFixture_SourceDigest(&g_fixture);
    observation.module = module_digest();
    return observation;
}

static void assert_inputs(const SObservation *before, TZrBool attached) {
    TEST_ASSERT_EQUAL_PTR(before->address, &g_output);
    TEST_ASSERT_EQUAL_UINT64(before->original,
            function_digest(ZrTests_SsaLiteralScriptFixture_Function(&g_fixture)));
    TEST_ASSERT_EQUAL_UINT64(before->source, ZrTests_SsaLiteralScriptFixture_SourceDigest(&g_fixture));
    TEST_ASSERT_EQUAL_UINT64(before->module, module_digest());
    if (attached) {
        SZrExecIrFunction header;
        memcpy(&header, &g_output, sizeof(header));
        header.frameLayout = before->header.frameLayout;
        header.contract.layoutHash = before->header.contract.layoutHash;
        TEST_ASSERT_EQUAL_MEMORY(&before->header, &header, sizeof(header));
        TEST_ASSERT_EQUAL_UINT64(before->body, body_digest(&g_output));
    } else {
        TEST_ASSERT_EQUAL_MEMORY(&before->header, &g_output, sizeof(g_output));
        TEST_ASSERT_EQUAL_UINT64(before->function, function_digest(&g_output));
    }
}

static TZrBool call_frame(SZrExecIrDiagnostic *diagnostic) {
    TZrBool success = ZrParser_ExecIr_AttachPrimitiveSourceFrame(&g_output,
            g_fixture.compiler.semanticContext, g_fixture.module.layouts,
            g_fixture.module.layoutCount, 0u, diagnostic);
    register_publication();
    g_ledger.failure = ALLOCATION_DISABLED;
    return success;
}

static void assert_geometry(TZrBool headerCalled) {
    TEST_ASSERT_EQUAL_UINT32(1u, g_ledger.callocCalls);
    TEST_ASSERT_TRUE(g_ledger.callocCount == (size_t)g_output.valueCount);
    TEST_ASSERT_TRUE(g_ledger.callocWidth == sizeof(SZrExecIrPackedValue));
    TEST_ASSERT_EQUAL_UINT32(headerCalled ? 1u : 0u, g_ledger.mallocCalls);
    if (headerCalled) TEST_ASSERT_TRUE(g_ledger.mallocSize == sizeof(SZrExecIrFrameLayout));
    TEST_ASSERT_EQUAL_UINT32(0u, g_ledger.errors);
}

static void assert_frame(void) {
    const SZrExecIrFrameLayout *frame = g_output.frameLayout;
    const SZrExecIrLayout *row = &g_fixture.module.layouts[0];
    const SZrExecIrValue *value = &g_output.values[0];
    const SZrExecIrFrameSlot *slot;
    TEST_ASSERT_NOT_NULL(frame);
    TEST_ASSERT_EQUAL_PTR(g_publishedHeader, frame);
    TEST_ASSERT_NOT_NULL(frame->slots);
    TEST_ASSERT_EQUAL_UINT32(g_output.valueCount, frame->slotCount);
    TEST_ASSERT_TRUE(frame->slotCapacity >= frame->slotCount);
    TEST_ASSERT_EQUAL_UINT32(g_output.valueCount, frame->logicalSlotCount);
    TEST_ASSERT_EQUAL_UINT32(g_output.valueCount, frame->storageSlotCount);
    TEST_ASSERT_EQUAL_UINT32(g_output.valueCount, frame->localCount);
    TEST_ASSERT_EQUAL_UINT32(0u, frame->parameterPrefixCount);
    TEST_ASSERT_EQUAL_UINT32(0u, frame->parameterCount);
    TEST_ASSERT_EQUAL_UINT32(row->byteSize, frame->frameByteSize);
    TEST_ASSERT_EQUAL_UINT32(row->byteSize, frame->returnBufferOffset);
    TEST_ASSERT_EQUAL_UINT32(row->byteAlign, frame->frameByteAlign);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, frame->layoutHash);
    TEST_ASSERT_EQUAL_UINT64(frame->layoutHash, g_output.contract.layoutHash);
    slot = &frame->slots[0];
    TEST_ASSERT_EQUAL_UINT32(value->id, slot->slotId);
    TEST_ASSERT_EQUAL_UINT32(value->typeToken, slot->typeToken);
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_FRAME_SLOT_VALUE, slot->kind);
    TEST_ASSERT_EQUAL_UINT32(0u, slot->byteOffset);
    TEST_ASSERT_EQUAL_UINT32(row->byteSize, slot->byteSize);
    TEST_ASSERT_EQUAL_UINT32(row->byteAlign, slot->byteAlign);
}

static void fail_then_retry(EAllocationKind failure) {
    SObservation before;
    SZrExecIrDiagnostic diagnostic, expected;
    TZrBool success, headerCalled = (TZrBool)(failure == ALLOCATION_HEADER);
    prepare();
    before = observe();
    memset(&diagnostic, 0xa5, sizeof(diagnostic));
    memset(&expected, 0, sizeof(expected));
    expected.code = ZR_EXEC_IR_DIAGNOSTIC_OUT_OF_MEMORY;
    expected.functionToken = g_output.functionToken;
    g_ledger.failure = failure;
    success = call_frame(&diagnostic);
    TEST_ASSERT_FALSE_MESSAGE(success, "Actual selected allocator returned NULL");
    TEST_ASSERT_EQUAL_UINT32(1u, g_ledger.failureHits);
    assert_geometry(headerCalled);
    TEST_ASSERT_EQUAL_MEMORY(&expected, &diagnostic, sizeof(diagnostic));
    TEST_ASSERT_NULL(g_output.frameLayout);
    TEST_ASSERT_EQUAL_UINT32(headerCalled ? 1u : 0u, g_ledger.allocationCount);
    TEST_ASSERT_EQUAL_UINT32(headerCalled ? 1u : 0u, g_ledger.nonnullFrees);
    TEST_ASSERT_EQUAL_UINT32(0u, temporary_owners());
    TEST_ASSERT_EQUAL_UINT32(0u, g_ledger.transfers);
    TEST_ASSERT_EQUAL_UINT32(headerCalled ? 1u : 0u, g_ledger.cleanupCalls);
    if (headerCalled) {
        TEST_ASSERT_TRUE(g_ledger.cleanupHadSlots);
        TEST_ASSERT_TRUE(g_ledger.cleanupHadMaps);
        TEST_ASSERT_TRUE(g_ledger.cleanupCleared);
    }
    assert_inputs(&before, ZR_FALSE);
    verify(&g_output);
    ZrTests_SsaLiteralScriptFixture_AssertSourceMaps(&g_fixture, &g_output);

    /* Reset only instrumentation. The same live graph and every pool remain. */
    memset(&g_ledger, 0, sizeof(g_ledger));
    memset(&diagnostic, 0xa5, sizeof(diagnostic));
    memset(&expected, 0, sizeof(expected));
    success = call_frame(&diagnostic);
    TEST_ASSERT_TRUE_MESSAGE(success, "Same graph retries after real allocator failure");
    TEST_ASSERT_EQUAL_MEMORY(&expected, &diagnostic, sizeof(diagnostic));
    assert_geometry(ZR_TRUE);
    TEST_ASSERT_EQUAL_UINT32(0u, g_ledger.failureHits);
    TEST_ASSERT_EQUAL_UINT32(2u, g_ledger.allocationCount);
    TEST_ASSERT_EQUAL_UINT32(1u, g_ledger.nonnullFrees);
    TEST_ASSERT_EQUAL_UINT32(0u, temporary_owners());
    TEST_ASSERT_EQUAL_UINT32(1u, g_ledger.transfers);
    TEST_ASSERT_EQUAL_UINT32(1u, g_ledger.cleanupCalls);
    TEST_ASSERT_FALSE(g_ledger.cleanupHadSlots);
    TEST_ASSERT_TRUE(g_ledger.cleanupHadMaps);
    TEST_ASSERT_TRUE(g_ledger.cleanupCleared);
    assert_inputs(&before, ZR_TRUE);
    assert_frame();
    verify(&g_output);
    ZrTests_SsaLiteralScriptFixture_AssertSourceMaps(&g_fixture, &g_output);
}

static void test_values_calloc_failure_same_graph_retry(void) {
    fail_then_retry(ALLOCATION_VALUES);
}

static void test_header_malloc_failure_same_graph_retry(void) {
    fail_then_retry(ALLOCATION_HEADER);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_values_calloc_failure_same_graph_retry);
    RUN_TEST(test_header_malloc_failure_same_graph_retry);
    return UNITY_END();
}
