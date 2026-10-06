#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/exec_ir_dead_source_places.h"
#include "zr_vm_parser/exec_ir_host_aot_target.h"
#include "zr_vm_parser/exec_ir_host_primitive_layout.h"
#include "zr_vm_parser/exec_ir_projections.h"
#include "zr_vm_parser/exec_ir_source_frame.h"
#include "support/ssa_literal_script_fixture.h"

enum { FIXTURE_COUNT = 2 };
static SZrState *g_state;
static SZrSsaLiteralScriptFixture g_fixtures[FIXTURE_COUNT];
static SZrExecIrFunction g_compacted[FIXTURE_COUNT];
static SZrAotIrProjection g_projections[FIXTURE_COUNT];

void setUp(void) {
    TZrUInt32 index;
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    for (index = 0u; index < FIXTURE_COUNT; ++index) {
        ZrTests_SsaLiteralScriptFixture_Init(&g_fixtures[index], g_state);
        ZrCore_ExecIr_FunctionInit(&g_compacted[index]);
        memset(&g_projections[index], 0, sizeof(g_projections[index]));
    }
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    TZrUInt32 index;
    for (index = 0u; index < FIXTURE_COUNT; ++index)
        ZrParser_AotIrProjection_Free(&g_projections[index]);
    for (index = 0u; index < FIXTURE_COUNT; ++index) {
        ZrCore_ExecIr_FreeFunction(&g_compacted[index]);
        ZrTests_SsaLiteralScriptFixture_Free(&g_fixtures[index]);
    }
    if (g_state != ZR_NULL) ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static TZrUInt64 digest(TZrUInt64 previous, const void *data, size_t size) {
    return ZrTests_SsaLiteralScriptFixture_DigestBytes(previous, data, size);
}

static TZrUInt64 graph_digest(const SZrExecIrFunction *function) {
    TZrUInt64 hash = ZrTests_SsaLiteralScriptFixture_FunctionDigest(function);
    if (function->frameLayout != ZR_NULL) {
        hash = digest(hash, function->frameLayout, sizeof(*function->frameLayout));
        hash = digest(hash, function->frameLayout->slots,
                (size_t)function->frameLayout->slotCount * sizeof(*function->frameLayout->slots));
    }
    return hash;
}

static TZrUInt64 projection_digest(const SZrAotIrProjection *projection) {
    TZrUInt64 hash = digest(0u, projection, sizeof(*projection));
#define OBSERVE_POOL(field, count) \
    hash = digest(hash, projection->field, (size_t)projection->count * sizeof(*projection->field))
    OBSERVE_POOL(frameSlots, frameSlotCount);
    OBSERVE_POOL(opcodes, instructionCount);
    OBSERVE_POOL(instructions, instructionCount);
    OBSERVE_POOL(operands, operandCount);
    OBSERVE_POOL(results, resultCount);
    OBSERVE_POOL(memoryTokens, memoryTokenCount);
    OBSERVE_POOL(valueSlots, valueSlotCount);
    OBSERVE_POOL(slotValues, physicalSlotCount);
    OBSERVE_POOL(blocks, blockCount);
    OBSERVE_POOL(predecessors, predecessorCount);
    OBSERVE_POOL(successors, successorCount);
    OBSERVE_POOL(phis, phiCount);
    OBSERVE_POOL(phiIncomings, phiIncomingCount);
    OBSERVE_POOL(phiCopySources, phiCopyCount);
    OBSERVE_POOL(phiCopyDestinations, phiCopyCount);
    OBSERVE_POOL(phiCopyEdges, phiCopyCount);
    OBSERVE_POOL(phiMoves, phiMoveCount);
    OBSERVE_POOL(sourceMaps, sourceMapCount);
    OBSERVE_POOL(constants, constantCount);
    OBSERVE_POOL(layouts, layoutCount);
    OBSERVE_POOL(gcRoots, gcRootCount);
    OBSERVE_POOL(deoptStates, deoptStateCount);
    OBSERVE_POOL(deoptValues, deoptValueCount);
    OBSERVE_POOL(deoptAggregates, deoptAggregateCount);
    OBSERVE_POOL(deoptAggregateFields, deoptAggregateFieldCount);
#undef OBSERVE_POOL
    hash = digest(hash, projection->stateMap.entries,
            (size_t)projection->stateMap.entryCount * sizeof(*projection->stateMap.entries));
    hash = digest(hash, projection->stateMap.valuePool,
            (size_t)projection->stateMap.valueCount * sizeof(*projection->stateMap.valuePool));
    hash = digest(hash, projection->stateMap.rootPool,
            (size_t)projection->stateMap.rootCount * sizeof(*projection->stateMap.rootPool));
    return digest(hash, projection->stateMap.ownerStatePool,
            (size_t)projection->stateMap.ownerStateCount * sizeof(*projection->stateMap.ownerStatePool));
}

/* Observations cover actual live allocations and pointer identity. They are
 * mutation detectors within one test, never the target's ABI fingerprint. */
static TZrUInt64 observation(TZrUInt32 index) {
    const SZrSsaLiteralScriptFixture *fixture = &g_fixtures[index];
    TZrUInt64 hash = ZrTests_SsaLiteralScriptFixture_SourceDigest(fixture);
    TZrUInt64 graph = graph_digest(ZrTests_SsaLiteralScriptFixture_Function(&g_fixtures[index]));
    TZrUInt64 compacted = graph_digest(&g_compacted[index]);
    TZrUInt64 projection = projection_digest(&g_projections[index]);
    hash = digest(hash, &graph, sizeof(graph));
    hash = digest(hash, &compacted, sizeof(compacted));
    hash = digest(hash, &projection, sizeof(projection));
    hash = digest(hash, &fixture->module, sizeof(fixture->module));
    hash = digest(hash, fixture->module.constants,
            (size_t)fixture->module.constantCount * sizeof(*fixture->module.constants));
    return digest(hash, fixture->module.layouts,
            (size_t)fixture->module.layoutCount * sizeof(*fixture->module.layouts));
}

static TZrTypeId return_type(TZrUInt32 index) {
    const SZrSsaLiteralScriptFixture *fixture = &g_fixtures[index];
    const SZrCanonicalTypeNode *callable = ZrParser_CanonicalType_Find(
            fixture->compiler.semanticContext, fixture->compiler.preSemanticIr.callableTypeId);
    const SZrCanonicalTypeNode *primitive;
    TEST_ASSERT_NOT_NULL(callable);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, callable->kind);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, callable->structuralHash);
    TEST_ASSERT_EQUAL_UINT32(0u, callable->data.function.parameterContracts.length);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_RECEIVER_NONE, callable->data.function.receiverEffect);
    TEST_ASSERT_EQUAL_UINT32(ZR_CANONICAL_CALLABLE_EFFECT_NONE, callable->data.function.effectFlags);
    primitive = ZrParser_CanonicalType_Find(fixture->compiler.semanticContext,
            callable->data.function.returnTypeId);
    TEST_ASSERT_NOT_NULL(primitive);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_PRIMITIVE, primitive->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, primitive->data.primitive.valueType);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, primitive->structuralHash);
    return callable->data.function.returnTypeId;
}

static void assert_verified(const SZrExecIrFunction *function) {
    SZrExecIrDiagnostic diagnostic = {0};
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_ExecIr_VerifyFunction(function,
            ZR_EXEC_IR_VERIFY_ALL, &diagnostic), "PRECONDITION: actual graph VERIFY_ALL");
}

static void prepare(TZrUInt32 index) {
    SZrSsaLiteralScriptFixture *fixture = &g_fixtures[index];
    SZrExecIrFunction *function = &g_compacted[index];
    SZrAotIrProjection *projection = &g_projections[index];
    const SZrExecIrFunction *source;
    SZrExecIrDiagnostic diagnostic = {0};
    SZrExecIrLayout row;
    SZrExecIrRange range;
    TZrUInt64 sourceBefore;
    TZrTypeId typeId;
    TZrUInt32 instruction;
    ZrTests_SsaLiteralScriptFixture_Prepare(fixture, index == 0u
            ? ZR_TEST_SSA_LITERAL_SCRIPT_NINE : ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT);
    source = ZrTests_SsaLiteralScriptFixture_Function(fixture);
    assert_verified(source);
    sourceBefore = ZrTests_SsaLiteralScriptFixture_SourceDigest(fixture);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &fixture->compiler.preSemanticIr, fixture->compiler.semanticContext,
            source, function, &diagnostic), "PRECONDITION: real same-context compaction");
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, ZrTests_SsaLiteralScriptFixture_SourceDigest(fixture));
    assert_verified(function);
    TEST_ASSERT_EQUAL_UINT32(source->id, function->id);
    TEST_ASSERT_EQUAL_UINT32(source->functionToken, function->functionToken);
    TEST_ASSERT_EQUAL_UINT64(source->signatureHash, function->signatureHash);
    TEST_ASSERT_EQUAL_UINT32(3u, function->instructionCount);
    TEST_ASSERT_EQUAL_UINT32(1u, function->valueCount);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_CONSTANT, function->instructions[0].opcode);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_NOP, function->instructions[1].opcode);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_RETURN, function->instructions[2].opcode);
    for (instruction = 0u; instruction < function->instructionCount; ++instruction) {
        TEST_ASSERT_EQUAL_UINT32(source->instructions[instruction].sourceId,
                function->instructions[instruction].sourceId);
    }
    ZrTests_SsaLiteralScriptFixture_AssertSourceMaps(fixture, function);
    typeId = return_type(index);
    TEST_ASSERT_EQUAL_UINT32(typeId, function->values[0].typeToken);
    TEST_ASSERT_EQUAL_UINT32(1u, function->values[0].id);
    TEST_ASSERT_EQUAL_UINT32(0u, fixture->module.layoutCount);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_MakeHostPrimitiveLayout(
            fixture->compiler.semanticContext, typeId, fixture->module.layoutCount + 1u,
            &row, &diagnostic), "PRECONDITION: actual host INT64 row");
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, row.id);
    TEST_ASSERT_EQUAL_UINT32(typeId, row.typeToken);
    TEST_ASSERT_EQUAL_UINT32(sizeof(TZrInt64), row.byteSize);
    TEST_ASSERT_EQUAL_UINT32(alignof(TZrInt64), row.byteAlign);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, row.layoutHash);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ModuleAppendLayout(&fixture->module, &row, 1u, &range));
    TEST_ASSERT_EQUAL_UINT32(0u, range.offset);
    TEST_ASSERT_EQUAL_UINT32(1u, range.count);
    TEST_ASSERT_EQUAL_UINT32(1u, fixture->module.layoutCount);
    TEST_ASSERT_TRUE(fixture->module.layoutCapacity >= fixture->module.layoutCount);
    TEST_ASSERT_EQUAL_MEMORY(&row, &fixture->module.layouts[0], sizeof(row));
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_AttachPrimitiveSourceFrame(function,
            fixture->compiler.semanticContext, fixture->module.layouts,
            fixture->module.layoutCount, 0u, &diagnostic), "PRECONDITION: actual owned frame");
    TEST_ASSERT_NOT_NULL(function->frameLayout);
    TEST_ASSERT_NOT_NULL(function->frameLayout->slots);
    TEST_ASSERT_EQUAL_UINT32(1u, function->frameLayout->slotCount);
    TEST_ASSERT_EQUAL_UINT32(function->values[0].id, function->frameLayout->slots[0].slotId);
    TEST_ASSERT_EQUAL_UINT32(typeId, function->frameLayout->slots[0].typeToken);
    TEST_ASSERT_EQUAL_UINT32(row.byteSize, function->frameLayout->slots[0].byteSize);
    TEST_ASSERT_EQUAL_UINT32(row.byteAlign, function->frameLayout->slots[0].byteAlign);
    TEST_ASSERT_EQUAL_UINT64(function->frameLayout->layoutHash, function->contract.layoutHash);
    assert_verified(function);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_LowerAotWithCanonicalCallable(function,
            fixture->module.constants, fixture->module.constantCount,
            fixture->module.layouts, fixture->module.layoutCount,
            fixture->compiler.semanticContext, fixture->compiler.preSemanticIr.callableTypeId,
            projection, &diagnostic), "PRECONDITION: actual canonical AOT projection");
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64, projection->callableAbi.kind);
    TEST_ASSERT_EQUAL_UINT32(typeId, projection->callableAbi.returnTypeToken);
    TEST_ASSERT_EQUAL_UINT32(function->functionToken, projection->functionToken);
    TEST_ASSERT_EQUAL_UINT64(function->signatureHash, projection->signatureHash);
    TEST_ASSERT_EQUAL_UINT32(function->instructionCount, projection->instructionCount);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_OPCODE_NOP, projection->opcodes[1]);
    TEST_ASSERT_EQUAL_UINT64(function->frameLayout->layoutHash, projection->frameLayoutHash);
    TEST_ASSERT_EQUAL_UINT32(1u, projection->frameSlotCount);
    TEST_ASSERT_EQUAL_MEMORY(function->frameLayout->slots, projection->frameSlots,
            sizeof(*projection->frameSlots));
    TEST_ASSERT_EQUAL_UINT32(1u, projection->valueSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, projection->physicalSlotCount);
    TEST_ASSERT_EQUAL_UINT32(0u, projection->valueSlots[0]);
    TEST_ASSERT_EQUAL_MEMORY(function->values, projection->slotValues, sizeof(*function->values));
    TEST_ASSERT_EQUAL_UINT32(fixture->module.constantCount, projection->constantCount);
    TEST_ASSERT_EQUAL_MEMORY(fixture->module.constants, projection->constants,
            (size_t)projection->constantCount * sizeof(*projection->constants));
    TEST_ASSERT_EQUAL_INT64(fixture->expectedReturn, (TZrInt64)projection->constants[0].bits);
    TEST_ASSERT_EQUAL_UINT32(1u, projection->layoutCount);
    TEST_ASSERT_EQUAL_MEMORY(fixture->module.layouts, projection->layouts, sizeof(row));
    TEST_ASSERT_EQUAL_UINT32(function->sourceMapCount, projection->sourceMapCount);
    for (instruction = 0u; instruction < function->sourceMapCount; ++instruction) {
        const SZrExecIrSourceMap *map = &function->sourceMaps[instruction];
        const SZrExecIrProjectionSourceMap *projected = &projection->sourceMaps[instruction];
        TEST_ASSERT_EQUAL_UINT32(map->instructionId - 1u, projected->pc);
        TEST_ASSERT_EQUAL_UINT32(map->sourceId, projected->sourceId);
        TEST_ASSERT_EQUAL_UINT32(map->startOffset, projected->startOffset);
        TEST_ASSERT_EQUAL_UINT32(map->endOffset, projected->endOffset);
        TEST_ASSERT_EQUAL_UINT32(map->startLine, projected->startLine);
        TEST_ASSERT_EQUAL_UINT32(map->startColumn, projected->startColumn);
        TEST_ASSERT_EQUAL_UINT32(map->endLine, projected->endLine);
        TEST_ASSERT_EQUAL_UINT32(map->endColumn, projected->endColumn);
    }
    TEST_ASSERT_NOT_NULL(source->stateMap);
    TEST_ASSERT_NOT_NULL(function->stateMap);
    TEST_ASSERT_TRUE(projection->stateMapPresent);
    TEST_ASSERT_EQUAL_UINT32(source->stateMap->functionToken, function->stateMap->functionToken);
    TEST_ASSERT_EQUAL_UINT64(source->stateMap->signatureHash, function->stateMap->signatureHash);
    TEST_ASSERT_EQUAL_UINT64(source->stateMap->generation, function->stateMap->generation);
    TEST_ASSERT_EQUAL_UINT32(function->stateMap->functionToken, projection->stateMap.functionToken);
    TEST_ASSERT_EQUAL_UINT64(function->stateMap->signatureHash, projection->stateMap.signatureHash);
    TEST_ASSERT_EQUAL_UINT64(function->stateMap->generation, projection->stateMap.generation);
    TEST_ASSERT_EQUAL_UINT32(0u, function->stateMap->entryCount);
    TEST_ASSERT_EQUAL_UINT32(0u, function->stateMap->valueCount);
    TEST_ASSERT_EQUAL_UINT32(0u, function->stateMap->rootCount);
    TEST_ASSERT_EQUAL_UINT32(0u, function->stateMap->ownerStateCount);
    TEST_ASSERT_EQUAL_UINT32(0u, projection->stateMap.entryCount);
    TEST_ASSERT_EQUAL_UINT32(0u, projection->stateMap.valueCount);
    TEST_ASSERT_EQUAL_UINT32(0u, projection->stateMap.rootCount);
    TEST_ASSERT_EQUAL_UINT32(0u, projection->stateMap.ownerStateCount);
    TEST_ASSERT_FALSE(projection->runnable);
}

static void test_prerequisite_real_nine_frame_and_projection(void) { prepare(0u); }
static void test_prerequisite_real_eight_frame_and_projection(void) { prepare(1u); }

#if defined(_WIN32) && defined(_WIN64) && defined(_MSC_VER) && \
        (defined(_M_X64) || defined(_M_AMD64)) && !defined(_M_ARM64) && !defined(_M_ARM64EC)

/* Independent arithmetic byte extraction, not the producer's encoder. */
static void expected_le(TZrByte *bytes, size_t *cursor, TZrUInt64 value, size_t width) {
    size_t index;
    for (index = 0u; index < width; ++index) {
        bytes[(*cursor)++] = (TZrByte)(value % UINT64_C(256));
        value /= UINT64_C(256);
    }
}

static TZrUInt64 expected_triple_hash(void) {
    static const char domain[] = "zr.aotir.target-triple";
    static const char triple[] = "x86_64-pc-windows-msvc";
    TZrByte bytes[sizeof(domain) - 1u + 8u + sizeof(triple) - 1u];
    size_t cursor = sizeof(domain) - 1u;
    memcpy(bytes, domain, cursor);
    expected_le(bytes, &cursor, 1u, 4u);
    expected_le(bytes, &cursor, sizeof(triple) - 1u, 4u);
    memcpy(bytes + cursor, triple, sizeof(triple) - 1u);
    return ZrCore_Hash_CreateStable64(bytes, sizeof(bytes));
}

static TZrUInt64 expected_abi_hash(TZrUInt64 tripleHash) {
    static const char domain[] = "zr.aotir.host.noargs-i64.abi";
    /* Schema, ABI version, triple hash, then eleven u32 ABI facts. */
    TZrByte bytes[sizeof(domain) - 1u + 16u + 44u];
    size_t cursor = sizeof(domain) - 1u;
    memcpy(bytes, domain, cursor);
    expected_le(bytes, &cursor, 1u, 4u);
    expected_le(bytes, &cursor, ZR_AOT_IR_TARGET_ABI_VERSION, 4u);
    expected_le(bytes, &cursor, tripleHash, 8u);
    expected_le(bytes, &cursor, 8u, 4u); /* pointer size */
    expected_le(bytes, &cursor, 8u, 4u); /* CHAR_BIT */
    expected_le(bytes, &cursor, 0u, 4u); /* little endian */
    expected_le(bytes, &cursor, ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64, 4u);
    expected_le(bytes, &cursor, 0u, 4u); /* explicit parameters */
    expected_le(bytes, &cursor, 0u, 4u); /* implicit parameters */
    expected_le(bytes, &cursor, 1u, 4u); /* signed return */
    expected_le(bytes, &cursor, 64u, 4u);
    expected_le(bytes, &cursor, 8u, 4u); /* return bytes */
    expected_le(bytes, &cursor, 8u, 4u); /* return alignment */
    expected_le(bytes, &cursor, 1u, 4u); /* Win64 C direct I64 in RAX */
    TEST_ASSERT_EQUAL_UINT32(sizeof(bytes), cursor);
    return ZrCore_Hash_CreateStable64(bytes, cursor);
}

static void assert_target(const SZrAotIrTargetContract *target) {
    SZrAotIrDiagnostic diagnostic = {0};
    const TZrUInt16 endian = 1u;
    TEST_ASSERT_EQUAL_UINT32(8u, sizeof(void *));
    TEST_ASSERT_EQUAL_UINT32(8u, CHAR_BIT);
    TEST_ASSERT_EQUAL_UINT32(8u, sizeof(TZrInt64));
    TEST_ASSERT_EQUAL_UINT32(8u, alignof(TZrInt64));
    TEST_ASSERT_EQUAL_UINT8(1u, *(const TZrByte *)&endian);
    TEST_ASSERT_EQUAL_UINT32(ZR_AOT_IR_TARGET_ABI_VERSION, target->abiVersion);
    TEST_ASSERT_EQUAL_UINT32(8u, target->pointerSize);
    TEST_ASSERT_EQUAL_UINT32(0u, target->endianness);
    TEST_ASSERT_EQUAL_UINT32(0u, target->requiredCapabilities);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, target->targetTripleHash);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, target->abiHash);
    TEST_ASSERT_EQUAL_UINT64(expected_triple_hash(), target->targetTripleHash);
    TEST_ASSERT_EQUAL_UINT64(expected_abi_hash(expected_triple_hash()), target->abiHash);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_ValidateTarget(target, &diagnostic));
}

static SZrAotIrTargetContract make_target(TZrUInt32 index, TZrBool withDiagnostic) {
    const SZrSsaLiteralScriptFixture *fixture = &g_fixtures[index];
    SZrAotIrTargetContract target;
    SZrAotIrDiagnostic diagnostic;
    SZrAotIrDiagnostic cleared;
    TZrUInt64 before = observation(index);
    TZrBool success;
    memset(&target, 0xa5, sizeof(target));
    memset(&diagnostic, 0xa5, sizeof(diagnostic));
    memset(&cleared, 0, sizeof(cleared));
    success = ZrParser_ExecIr_MakeHostNoArgsI64AotTarget(fixture->compiler.semanticContext,
            fixture->compiler.preSemanticIr.callableTypeId, &fixture->module.layouts[0],
            &target, withDiagnostic ? &diagnostic : ZR_NULL);
    TEST_ASSERT_EQUAL_UINT64(before, observation(index));
    TEST_ASSERT_TRUE_MESSAGE(success, "FEATURE RED: real noargs I64 host AOT target");
    if (withDiagnostic) TEST_ASSERT_EQUAL_MEMORY(&cleared, &diagnostic, sizeof(diagnostic));
    assert_target(&target);
    return target;
}

static void assert_equal_target(const SZrAotIrTargetContract *first,
        const SZrAotIrTargetContract *second) {
    TEST_ASSERT_EQUAL_UINT32(first->abiVersion, second->abiVersion);
    TEST_ASSERT_EQUAL_UINT32(first->pointerSize, second->pointerSize);
    TEST_ASSERT_EQUAL_UINT32(first->endianness, second->endianness);
    TEST_ASSERT_EQUAL_UINT32(first->requiredCapabilities, second->requiredCapabilities);
    TEST_ASSERT_EQUAL_UINT64(first->targetTripleHash, second->targetTripleHash);
    TEST_ASSERT_EQUAL_UINT64(first->abiHash, second->abiHash);
}

static void test_real_nine_host_target(void) {
    prepare(0u);
    (void)make_target(0u, ZR_TRUE);
}

static void test_real_eight_target_equals_real_nine(void) {
    SZrAotIrTargetContract nine, eight;
    prepare(0u);
    prepare(1u);
    TEST_ASSERT_TRUE(g_fixtures[0].compiler.semanticContext != g_fixtures[1].compiler.semanticContext);
    TEST_ASSERT_NOT_EQUAL_UINT64(g_projections[0].constants[0].bits, g_projections[1].constants[0].bits);
    /* Actual contexts are independent; local IDs may coincide. Equal targets
     * establish exclusion of these different body bits, not different local IDs. */
    nine = make_target(0u, ZR_TRUE);
    eight = make_target(1u, ZR_TRUE);
    assert_equal_target(&nine, &eight);
}

static void test_repeat_and_null_diagnostic_preserve_real_inputs(void) {
    SZrAotIrTargetContract first, repeated, withoutDiagnostic;
    TZrUInt64 before;
    prepare(0u);
    before = observation(0u);
    first = make_target(0u, ZR_TRUE);
    repeated = make_target(0u, ZR_TRUE);
    withoutDiagnostic = make_target(0u, ZR_FALSE);
    assert_equal_target(&first, &repeated);
    assert_equal_target(&first, &withoutDiagnostic);
    TEST_ASSERT_EQUAL_UINT64(before, observation(0u));
}

typedef struct SRefusal {
    TZrBool success;
    SZrAotIrDiagnostic diagnostic;
    SZrAotIrTargetContract before, after;
} SRefusal;

/* This performs no input digests or assertions: callers first restore any
 * temporary metadata, then inspect complete live observations safely. */
static SRefusal refuse(const SZrSemanticContext *context, TZrTypeId callableId,
        const SZrExecIrLayout *row, TZrBool outputPresent) {
    SRefusal result;
    memset(&result, 0, sizeof(result));
    memset(&result.before, 0xa5, sizeof(result.before));
    memcpy(&result.after, &result.before, sizeof(result.after));
    result.success = ZrParser_ExecIr_MakeHostNoArgsI64AotTarget(context, callableId,
            row, outputPresent ? &result.after : ZR_NULL, &result.diagnostic);
    return result;
}

static void assert_refusal(SRefusal result, EZrAotIrStatus status, TZrUInt64 before) {
    TEST_ASSERT_FALSE(result.success);
    TEST_ASSERT_EQUAL_INT(status, result.diagnostic.status);
    TEST_ASSERT_EQUAL_MEMORY(&result.before, &result.after, sizeof(result.after));
    TEST_ASSERT_EQUAL_UINT64(before, observation(0u));
}

static void test_null_required_arguments(void) {
    SZrSsaLiteralScriptFixture *fixture = &g_fixtures[0];
    TZrUInt64 before;
    prepare(0u);
    before = observation(0u);
    assert_refusal(refuse(ZR_NULL, fixture->compiler.preSemanticIr.callableTypeId,
            &fixture->module.layouts[0], ZR_TRUE), ZR_AOT_IR_INVALID_ARGUMENT, before);
    assert_refusal(refuse(fixture->compiler.semanticContext, fixture->compiler.preSemanticIr.callableTypeId,
            ZR_NULL, ZR_TRUE), ZR_AOT_IR_INVALID_ARGUMENT, before);
    assert_refusal(refuse(fixture->compiler.semanticContext, fixture->compiler.preSemanticIr.callableTypeId,
            &fixture->module.layouts[0], ZR_FALSE), ZR_AOT_IR_INVALID_ARGUMENT, before);
}

static void test_zero_callable_id(void) {
    TZrUInt64 before;
    prepare(0u);
    before = observation(0u);
    assert_refusal(refuse(g_fixtures[0].compiler.semanticContext, 0u,
            &g_fixtures[0].module.layouts[0], ZR_TRUE), ZR_AOT_IR_INVALID_ID, before);
}

static void test_real_primitive_id_is_not_callable(void) {
    TZrUInt64 before;
    TZrTypeId typeId;
    prepare(0u);
    typeId = return_type(0u);
    before = observation(0u);
    assert_refusal(refuse(g_fixtures[0].compiler.semanticContext, typeId,
            &g_fixtures[0].module.layouts[0], ZR_TRUE), ZR_AOT_IR_INVALID_SIGNATURE, before);
}

enum ERowContradiction { ROW_ID, ROW_TYPE, ROW_SIZE, ROW_ALIGN, ROW_HASH };
static void contradict_row(enum ERowContradiction kind) {
    SZrSsaLiteralScriptFixture *fixture = &g_fixtures[0];
    SZrExecIrLayout saved, perturbed, observed;
    SZrExecIrLayout *row;
    SRefusal result;
    TZrUInt64 before;
    prepare(0u);
    before = observation(0u);
    row = &fixture->module.layouts[0];
    saved = *row;
    switch (kind) {
        case ROW_ID: row->id = 0u; break;
        case ROW_TYPE: row->typeToken = fixture->compiler.preSemanticIr.callableTypeId; break;
        case ROW_SIZE: row->byteSize /= 2u; break;
        case ROW_ALIGN: row->byteAlign /= 2u; break;
        case ROW_HASH: row->layoutHash ^= UINT64_C(1); break;
    }
    perturbed = *row;
    result = refuse(fixture->compiler.semanticContext, fixture->compiler.preSemanticIr.callableTypeId,
            row, ZR_TRUE);
    observed = *row;
    *row = saved;
    TEST_ASSERT_EQUAL_MEMORY(&perturbed, &observed, sizeof(observed));
    assert_refusal(result, ZR_AOT_IR_INVALID_LAYOUT, before);
}

static void test_zero_actual_row_identity(void) { contradict_row(ROW_ID); }
static void test_actual_row_type_contradiction(void) { contradict_row(ROW_TYPE); }
static void test_actual_row_size_contradiction(void) { contradict_row(ROW_SIZE); }
static void test_actual_row_alignment_contradiction(void) { contradict_row(ROW_ALIGN); }
static void test_actual_row_hash_contradiction(void) { contradict_row(ROW_HASH); }

static void test_actual_context_length_exceeds_capacity(void) {
    SZrSsaLiteralScriptFixture *fixture = &g_fixtures[0];
    SZrArray *nodes;
    SZrArray saved, perturbed, observed;
    TZrUInt64 before;
    SRefusal result;
    prepare(0u);
    nodes = &fixture->compiler.semanticContext->canonicalTypes;
    TEST_ASSERT_TRUE(nodes->capacity < SIZE_MAX);
    TEST_ASSERT_NOT_NULL(nodes->head);
    before = observation(0u);
    saved = *nodes;
    nodes->length = nodes->capacity + 1u;
    perturbed = *nodes;
    result = refuse(fixture->compiler.semanticContext, fixture->compiler.preSemanticIr.callableTypeId,
            &fixture->module.layouts[0], ZR_TRUE);
    observed = *nodes;
    *nodes = saved;
    TEST_ASSERT_EQUAL_MEMORY(&perturbed, &observed, sizeof(observed));
    assert_refusal(result, ZR_AOT_IR_INVALID_RANGE, before);
}

#else
/* Compilable independent refusal coverage on other hosts. No claim of dynamic
 * non-Windows evidence is made by this source branch. */
static void test_other_host_reports_unsupported(void) {
    SZrAotIrTargetContract target, before;
    SZrAotIrDiagnostic diagnostic = {0};
    TZrUInt64 inputs;
    TZrBool success;
    prepare(0u);
    inputs = observation(0u);
    memset(&target, 0xa5, sizeof(target));
    memcpy(&before, &target, sizeof(before));
    success = ZrParser_ExecIr_MakeHostNoArgsI64AotTarget(g_fixtures[0].compiler.semanticContext,
            g_fixtures[0].compiler.preSemanticIr.callableTypeId, &g_fixtures[0].module.layouts[0],
            &target, &diagnostic);
    TEST_ASSERT_FALSE(success);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_UNSUPPORTED, diagnostic.status);
    TEST_ASSERT_EQUAL_MEMORY(&before, &target, sizeof(target));
    TEST_ASSERT_EQUAL_UINT64(inputs, observation(0u));
}
#endif

int main(int argc, char **argv) {
    TZrBool prerequisitesOnly = ZR_FALSE, featuresOnly = ZR_FALSE;
    if (argc == 2 && strcmp(argv[1], "--prerequisites-only") == 0) prerequisitesOnly = ZR_TRUE;
    else if (argc == 2 && strcmp(argv[1], "--features-only") == 0) featuresOnly = ZR_TRUE;
    else if (argc != 1) return 2;
    UNITY_BEGIN();
    if (!featuresOnly) {
        RUN_TEST(test_prerequisite_real_nine_frame_and_projection);
        RUN_TEST(test_prerequisite_real_eight_frame_and_projection);
    }
    if (!prerequisitesOnly) {
#if defined(_WIN32) && defined(_WIN64) && defined(_MSC_VER) && \
        (defined(_M_X64) || defined(_M_AMD64)) && !defined(_M_ARM64) && !defined(_M_ARM64EC)
        RUN_TEST(test_real_nine_host_target);
        RUN_TEST(test_real_eight_target_equals_real_nine);
        RUN_TEST(test_repeat_and_null_diagnostic_preserve_real_inputs);
        if (!featuresOnly) {
            RUN_TEST(test_null_required_arguments);
            RUN_TEST(test_zero_callable_id);
            RUN_TEST(test_real_primitive_id_is_not_callable);
            RUN_TEST(test_zero_actual_row_identity);
            RUN_TEST(test_actual_row_type_contradiction);
            RUN_TEST(test_actual_row_size_contradiction);
            RUN_TEST(test_actual_row_alignment_contradiction);
            RUN_TEST(test_actual_row_hash_contradiction);
            RUN_TEST(test_actual_context_length_exceeds_capacity);
        }
#else
        RUN_TEST(test_other_host_reports_unsupported);
#endif
    }
    return UNITY_END();
}
