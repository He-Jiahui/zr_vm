#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/exec_ir_dead_source_places.h"
#include "zr_vm_parser/exec_ir_host_primitive_layout.h"
#include "zr_vm_parser/exec_ir_projections.h"
#include "zr_vm_parser/exec_ir_source_frame.h"
#include "support/ssa_literal_script_fixture.h"

static SZrState *g_state;
static SZrSsaLiteralScriptFixture g_fixture;
static SZrExecIrFunction g_output, g_mutated;
static SZrExecIrOracleExecutionResult g_oracle;
static SZrAotIrProjection g_projection;

void setUp(void) {
    memset(&g_projection, 0, sizeof(g_projection));
    ZrCore_ExecIr_FunctionInit(&g_output);
    ZrCore_ExecIr_FunctionInit(&g_mutated);
    ZrCore_ExecIr_OracleResultInit(&g_oracle);
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrTests_SsaLiteralScriptFixture_Init(&g_fixture, g_state);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    ZrParser_AotIrProjection_Free(&g_projection);
    ZrCore_ExecIr_OracleResultFree(&g_oracle);
    ZrCore_ExecIr_FreeFunction(&g_mutated);
    ZrCore_ExecIr_FreeFunction(&g_output);
    ZrTests_SsaLiteralScriptFixture_Free(&g_fixture);
    if (g_state != ZR_NULL) ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static TZrUInt64 bytes(TZrUInt64 hash, const void *data, size_t size) {
    return ZrTests_SsaLiteralScriptFixture_DigestBytes(hash, data, size);
}

/* The shared observation includes pointer identity but not frame contents. */
static TZrUInt64 function_observation_digest(const SZrExecIrFunction *function) {
    TZrUInt64 hash = ZrTests_SsaLiteralScriptFixture_FunctionDigest(function);
    if (function->frameLayout != ZR_NULL) {
        const SZrExecIrFrameLayout *frame = function->frameLayout;
        hash = bytes(hash, frame, sizeof(*frame));
        hash = bytes(hash, frame->slots, (size_t)frame->slotCount * sizeof(*frame->slots));
    }
    return hash;
}

/* A shallow observation header masks exactly the two successful publications;
 * all arrays are observed at their actual addresses, never copied or replayed. */
static TZrUInt64 body_digest(const SZrExecIrFunction *function) {
    SZrExecIrFunction header = *function;
    header.frameLayout = ZR_NULL;
    header.contract.layoutHash = 0u;
    return ZrTests_SsaLiteralScriptFixture_FunctionDigest(&header);
}

static TZrUInt64 tables_digest(void) {
    TZrUInt64 hash = bytes(0u, &g_fixture.module, sizeof(g_fixture.module));
    hash = bytes(hash, g_fixture.module.constants,
            (size_t)g_fixture.module.constantCount * sizeof(*g_fixture.module.constants));
    return bytes(hash, g_fixture.module.layouts,
            (size_t)g_fixture.module.layoutCount * sizeof(*g_fixture.module.layouts));
}

static void assert_verified(const SZrExecIrFunction *function) {
    SZrExecIrDiagnostic diagnostic = {0};
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_ExecIr_VerifyFunction(function,
            ZR_EXEC_IR_VERIFY_ALL, &diagnostic), "PRECONDITION: actual graph VERIFY_ALL");
}

static void assert_oracle(const SZrExecIrFunction *function, TZrUInt32 placeCalls) {
    ZrTests_SsaLiteralScriptFixture_AssertOracle(&g_fixture, function, placeCalls, &g_oracle);
    ZrCore_ExecIr_OracleResultFree(&g_oracle);
    ZrCore_ExecIr_OracleResultInit(&g_oracle);
}

static TZrTypeId actual_return_type(void) {
    const SZrCanonicalTypeNode *callable = ZrParser_CanonicalType_Find(
            g_fixture.compiler.semanticContext, g_fixture.compiler.preSemanticIr.callableTypeId);
    const SZrCanonicalTypeNode *primitive;
    TZrTypeId typeId;
    TEST_ASSERT_NOT_NULL(callable);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, callable->kind);
    typeId = callable->data.function.returnTypeId;
    primitive = ZrParser_CanonicalType_Find(g_fixture.compiler.semanticContext, typeId);
    TEST_ASSERT_NOT_NULL(primitive);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_PRIMITIVE, primitive->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, primitive->data.primitive.valueType);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, primitive->structuralHash);
    return typeId;
}

static void prepare(EZrSsaLiteralScriptSource source) {
    SZrExecIrDiagnostic diagnostic = {0};
    SZrExecIrLayout row;
    SZrExecIrRange range;
    SZrExecIrFunction *original;
    TZrTypeId typeId;
    TZrUInt64 sourceBefore;
    ZrTests_SsaLiteralScriptFixture_Prepare(&g_fixture, source);
    original = ZrTests_SsaLiteralScriptFixture_Function(&g_fixture);
    assert_verified(original);
    assert_oracle(original, 1u);
    sourceBefore = ZrTests_SsaLiteralScriptFixture_SourceDigest(&g_fixture);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &g_fixture.compiler.preSemanticIr, g_fixture.compiler.semanticContext,
            original, &g_output, &diagnostic), "PRECONDITION: actual same-context source compaction");
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, ZrTests_SsaLiteralScriptFixture_SourceDigest(&g_fixture));
    assert_verified(&g_output);
    assert_oracle(&g_output, 0u);
    ZrTests_SsaLiteralScriptFixture_AssertSourceMaps(&g_fixture, &g_output);
    typeId = actual_return_type();
    TEST_ASSERT_EQUAL_UINT32(0u, g_fixture.module.layoutCount);
    TEST_ASSERT_EQUAL_UINT32(0u, g_fixture.module.layoutCapacity);
    TEST_ASSERT_NULL(g_fixture.module.layouts);
    TEST_ASSERT_TRUE(g_fixture.module.layoutCount < UINT32_MAX);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_MakeHostPrimitiveLayout(
            g_fixture.compiler.semanticContext, typeId, g_fixture.module.layoutCount + 1u,
            &row, &diagnostic), "PRECONDITION: actual host primitive layout producer");
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, row.id);
    TEST_ASSERT_EQUAL_UINT32(typeId, row.typeToken);
    TEST_ASSERT_EQUAL_UINT32(sizeof(TZrInt64), row.byteSize);
    TEST_ASSERT_EQUAL_UINT32(alignof(TZrInt64), row.byteAlign);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, row.layoutHash);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ModuleAppendLayout(&g_fixture.module, &row, 1u, &range));
    TEST_ASSERT_EQUAL_UINT32(0u, range.offset);
    TEST_ASSERT_EQUAL_UINT32(1u, range.count);
    TEST_ASSERT_EQUAL_UINT32(1u, g_fixture.module.layoutCount);
    TEST_ASSERT_TRUE(g_fixture.module.layoutCapacity >= 1u);
    TEST_ASSERT_NOT_NULL(g_fixture.module.layouts);
    TEST_ASSERT_EQUAL_UINT32(row.id, g_fixture.module.layouts[0].id);
    TEST_ASSERT_EQUAL_UINT32(row.typeToken, g_fixture.module.layouts[0].typeToken);
    TEST_ASSERT_EQUAL_UINT32(row.byteSize, g_fixture.module.layouts[0].byteSize);
    TEST_ASSERT_EQUAL_UINT32(row.byteAlign, g_fixture.module.layouts[0].byteAlign);
    TEST_ASSERT_EQUAL_UINT64(row.layoutHash, g_fixture.module.layouts[0].layoutHash);
    TEST_ASSERT_NULL(g_output.frameLayout);
    TEST_ASSERT_TRUE(g_output.instructionCount > 0u);
    TEST_ASSERT_TRUE(g_output.valueCount > 0u);
    {
        TZrUInt32 index;
        for (index = 0u; index < g_output.valueCount; ++index) {
            TEST_ASSERT_EQUAL_UINT32(typeId, g_output.values[index].typeToken);
            TEST_ASSERT_EQUAL_UINT32(0u, g_output.values[index].flags);
            TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_OWNERSHIP_UNKNOWN, g_output.values[index].ownership);
            TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_NULLABILITY_NONNULL, g_output.values[index].nullability);
        }
    }
}

static void assert_projection(void) {
    SZrExecIrDiagnostic diagnostic = {0};
    TZrUInt32 index;
    TZrUInt64 before = function_observation_digest(&g_output);
    TZrUInt64 tablesBefore = tables_digest();
    TZrUInt64 sourceBefore = ZrTests_SsaLiteralScriptFixture_SourceDigest(&g_fixture);
    TZrBool success = ZrParser_ExecIr_LowerAotWithCanonicalCallable(&g_output,
            g_fixture.module.constants, g_fixture.module.constantCount,
            g_fixture.module.layouts, g_fixture.module.layoutCount,
            g_fixture.compiler.semanticContext, g_fixture.compiler.preSemanticIr.callableTypeId,
            &g_projection, &diagnostic);
    TEST_ASSERT_EQUAL_UINT64(before, function_observation_digest(&g_output));
    TEST_ASSERT_EQUAL_UINT64(tablesBefore, tables_digest());
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, ZrTests_SsaLiteralScriptFixture_SourceDigest(&g_fixture));
    TEST_ASSERT_TRUE_MESSAGE(success, "FEATURE: same attached frame canonical AOT projection");
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64, g_projection.callableAbi.kind);
    TEST_ASSERT_EQUAL_UINT32(actual_return_type(), g_projection.callableAbi.returnTypeToken);
    TEST_ASSERT_FALSE(g_projection.runnable);
    TEST_ASSERT_EQUAL_UINT64(g_output.frameLayout->layoutHash, g_projection.frameLayoutHash);
    TEST_ASSERT_EQUAL_UINT32(g_output.valueCount, g_projection.valueSlotCount);
    TEST_ASSERT_EQUAL_UINT32(g_output.valueCount, g_projection.physicalSlotCount);
    TEST_ASSERT_EQUAL_UINT32(g_output.frameLayout->slotCount, g_projection.frameSlotCount);
    TEST_ASSERT_NOT_NULL(g_projection.valueSlots);
    TEST_ASSERT_NOT_NULL(g_projection.slotValues);
    TEST_ASSERT_NOT_NULL(g_projection.frameSlots);
    for (index = 0u; index < g_output.valueCount; ++index) {
        const SZrExecIrValue *value = &g_output.values[index];
        TZrUInt32 physical = g_projection.valueSlots[value->id - 1u];
        TEST_ASSERT_TRUE(physical < g_projection.physicalSlotCount);
        TEST_ASSERT_EQUAL_MEMORY(value, &g_projection.slotValues[physical], sizeof(*value));
        TEST_ASSERT_EQUAL_UINT32(value->id, g_projection.frameSlots[physical].slotId);
        TEST_ASSERT_EQUAL_MEMORY(&g_output.frameLayout->slots[physical],
                &g_projection.frameSlots[physical], sizeof(*g_projection.frameSlots));
    }
    TEST_ASSERT_EQUAL_UINT32(g_fixture.module.constantCount, g_projection.constantCount);
    TEST_ASSERT_EQUAL_MEMORY(g_fixture.module.constants, g_projection.constants,
            (size_t)g_projection.constantCount * sizeof(*g_projection.constants));
    TEST_ASSERT_EQUAL_UINT32(g_fixture.module.layoutCount, g_projection.layoutCount);
    TEST_ASSERT_EQUAL_MEMORY(g_fixture.module.layouts, g_projection.layouts,
            (size_t)g_projection.layoutCount * sizeof(*g_projection.layouts));
    TEST_ASSERT_EQUAL_UINT32(g_output.sourceMapCount, g_projection.sourceMapCount);
    TEST_ASSERT_NOT_NULL(g_projection.sourceMaps);
    for (index = 0u; index < g_output.sourceMapCount; ++index) {
        const SZrExecIrSourceMap *source = &g_output.sourceMaps[index];
        const SZrExecIrProjectionSourceMap *projected = &g_projection.sourceMaps[index];
        TEST_ASSERT_EQUAL_UINT32(source->sourceId, projected->sourceId);
        TEST_ASSERT_EQUAL_UINT32(source->instructionId - 1u, projected->pc);
        TEST_ASSERT_EQUAL_UINT32(source->startOffset, projected->startOffset);
        TEST_ASSERT_EQUAL_UINT32(source->endOffset, projected->endOffset);
        TEST_ASSERT_EQUAL_UINT32(source->startLine, projected->startLine);
        TEST_ASSERT_EQUAL_UINT32(source->startColumn, projected->startColumn);
        TEST_ASSERT_EQUAL_UINT32(source->endLine, projected->endLine);
        TEST_ASSERT_EQUAL_UINT32(source->endColumn, projected->endColumn);
    }
}

static void attach(TZrBool withDiagnostic) {
    SZrExecIrDiagnostic diagnostic;
    SZrExecIrDiagnostic clear = {0};
    const SZrExecIrLayout *row = &g_fixture.module.layouts[0];
    const SZrExecIrFrameLayout *frame;
    TZrUInt32 index, cursor = 0u;
    TZrUInt64 before = body_digest(&g_output), tablesBefore = tables_digest();
    TZrUInt64 sourceBefore = ZrTests_SsaLiteralScriptFixture_SourceDigest(&g_fixture);
    TZrBool success;
    memset(&diagnostic, 0xa5, sizeof(diagnostic));
    success = ZrParser_ExecIr_AttachPrimitiveSourceFrame(&g_output,
            g_fixture.compiler.semanticContext, g_fixture.module.layouts,
            g_fixture.module.layoutCount, 0u, withDiagnostic ? &diagnostic : ZR_NULL);
    TEST_ASSERT_EQUAL_UINT64(before, body_digest(&g_output));
    TEST_ASSERT_EQUAL_UINT64(tablesBefore, tables_digest());
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, ZrTests_SsaLiteralScriptFixture_SourceDigest(&g_fixture));
    TEST_ASSERT_TRUE_MESSAGE(success, "FEATURE RED: attach actual compacted primitive source frame");
    if (withDiagnostic) TEST_ASSERT_EQUAL_MEMORY(&clear, &diagnostic, sizeof(diagnostic));
    frame = g_output.frameLayout;
    TEST_ASSERT_NOT_NULL(frame);
    TEST_ASSERT_NOT_NULL(frame->slots);
    TEST_ASSERT_EQUAL_UINT32(g_output.valueCount, frame->logicalSlotCount);
    TEST_ASSERT_EQUAL_UINT32(g_output.valueCount, frame->storageSlotCount);
    TEST_ASSERT_EQUAL_UINT32(g_output.valueCount, frame->slotCount);
    TEST_ASSERT_TRUE(frame->slotCapacity >= frame->slotCount);
    TEST_ASSERT_EQUAL_UINT32(0u, frame->parameterPrefixCount);
    TEST_ASSERT_EQUAL_UINT32(0u, frame->parameterCount);
    TEST_ASSERT_EQUAL_UINT32(g_output.valueCount, frame->localCount);
    TEST_ASSERT_EQUAL_UINT32(row->byteAlign, frame->frameByteAlign);
    for (index = 0u; index < g_output.valueCount; ++index) {
        const SZrExecIrFrameSlot *slot = &frame->slots[index];
        const SZrExecIrValue *value = &g_output.values[index];
        TZrUInt32 prior;
        cursor = (cursor + row->byteAlign - 1u) & ~(row->byteAlign - 1u);
        TEST_ASSERT_EQUAL_UINT32(value->id, slot->slotId);
        TEST_ASSERT_EQUAL_UINT32(value->typeToken, slot->typeToken);
        TEST_ASSERT_EQUAL_UINT32(row->byteSize, slot->byteSize);
        TEST_ASSERT_EQUAL_UINT32(row->byteAlign, slot->byteAlign);
        TEST_ASSERT_EQUAL_UINT32(cursor, slot->byteOffset);
        TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_FRAME_SLOT_VALUE, slot->kind);
        for (prior = 0u; prior < index; ++prior)
            TEST_ASSERT_TRUE(frame->slots[prior].byteOffset + frame->slots[prior].byteSize <= slot->byteOffset);
        cursor += row->byteSize;
    }
    cursor = (cursor + row->byteAlign - 1u) & ~(row->byteAlign - 1u);
    TEST_ASSERT_EQUAL_UINT32(cursor, frame->returnBufferOffset);
    TEST_ASSERT_EQUAL_UINT32(cursor, frame->frameByteSize);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, frame->layoutHash);
    TEST_ASSERT_EQUAL_UINT64(frame->layoutHash, g_output.contract.layoutHash);
    if (g_output.stateMap != ZR_NULL) {
        const SZrExecIrStateMap *original =
                ZrTests_SsaLiteralScriptFixture_Function(&g_fixture)->stateMap;
        TEST_ASSERT_NOT_NULL(original);
        TEST_ASSERT_EQUAL_UINT32(original->functionToken, g_output.stateMap->functionToken);
        TEST_ASSERT_EQUAL_UINT64(original->signatureHash, g_output.stateMap->signatureHash);
        TEST_ASSERT_EQUAL_UINT64(original->generation, g_output.stateMap->generation);
        TEST_ASSERT_EQUAL_UINT32(0u, g_output.stateMap->entryCount);
        TEST_ASSERT_EQUAL_UINT32(0u, g_output.stateMap->valueCount);
        TEST_ASSERT_EQUAL_UINT32(0u, g_output.stateMap->rootCount);
        TEST_ASSERT_EQUAL_UINT32(0u, g_output.stateMap->ownerStateCount);
    }
    assert_verified(&g_output);
    assert_oracle(&g_output, 0u);
    ZrTests_SsaLiteralScriptFixture_AssertSourceMaps(&g_fixture, &g_output);
    assert_projection();
}

typedef struct SRefusalObservation {
    TZrBool success;
    SZrExecIrDiagnostic diagnostic;
    TZrUInt64 functionBefore, functionAfter, outputBefore, outputAfter;
    TZrUInt64 sourceBefore, sourceAfter, tablesBefore, tablesAfter;
} SRefusalObservation;

/* Callers restore temporarily perturbed metadata before the first assertion. */
static SRefusalObservation observe_refusal(SZrExecIrFunction *function,
        const SZrSemanticContext *context, const SZrExecIrLayout *rows,
        TZrUInt32 rowCount, TZrUInt32 limit) {
    SRefusalObservation observation;
    memset(&observation, 0, sizeof(observation));
    observation.functionBefore = function_observation_digest(function != ZR_NULL ? function : &g_output);
    observation.outputBefore = function_observation_digest(&g_output);
    observation.sourceBefore = ZrTests_SsaLiteralScriptFixture_SourceDigest(&g_fixture);
    observation.tablesBefore = tables_digest();
    observation.success = ZrParser_ExecIr_AttachPrimitiveSourceFrame(function,
            context, rows, rowCount, limit, &observation.diagnostic);
    observation.functionAfter = function_observation_digest(function != ZR_NULL ? function : &g_output);
    observation.outputAfter = function_observation_digest(&g_output);
    observation.sourceAfter = ZrTests_SsaLiteralScriptFixture_SourceDigest(&g_fixture);
    observation.tablesAfter = tables_digest();
    return observation;
}

static void assert_refusal(SRefusalObservation observation, EZrExecutionDiagnosticCode code) {
    TEST_ASSERT_FALSE(observation.success);
    TEST_ASSERT_EQUAL_INT(code, observation.diagnostic.code);
    TEST_ASSERT_EQUAL_UINT64(observation.functionBefore, observation.functionAfter);
    TEST_ASSERT_EQUAL_UINT64(observation.outputBefore, observation.outputAfter);
    TEST_ASSERT_EQUAL_UINT64(observation.sourceBefore, observation.sourceAfter);
    TEST_ASSERT_EQUAL_UINT64(observation.tablesBefore, observation.tablesAfter);
}

static SRefusalObservation refuse(SZrExecIrFunction *function) {
    return observe_refusal(function, g_fixture.compiler.semanticContext,
            g_fixture.module.layouts, g_fixture.module.layoutCount, 0u);
}

static void prepare_guard(void) {
    SZrExecIrDiagnostic diagnostic = {0};
    prepare(ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_CloneFunction(&g_output, &g_mutated, &diagnostic));
}

static void test_prerequisite_nine(void) { prepare(ZR_TEST_SSA_LITERAL_SCRIPT_NINE); }
static void test_prerequisite_eight(void) { prepare(ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT); }
static void test_attach_nine(void) { prepare(ZR_TEST_SSA_LITERAL_SCRIPT_NINE); attach(ZR_TRUE); }
static void test_attach_eight(void) { prepare(ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT); attach(ZR_TRUE); }
static void test_attach_null_diagnostic(void) { prepare(ZR_TEST_SSA_LITERAL_SCRIPT_NINE); attach(ZR_FALSE); }

static void test_original_address_unsupported(void) {
    prepare_guard();
    assert_refusal(refuse(ZrTests_SsaLiteralScriptFixture_Function(&g_fixture)), ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
}

static void test_core_valid_external_unsupported(void) {
    SZrExecIrInstruction savedInstruction;
    SZrExecIrValue savedValue;
    SRefusalObservation observation;
    prepare_guard();
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_CONSTANT, g_mutated.instructions[0].opcode);
    savedInstruction = g_mutated.instructions[0];
    savedValue = g_mutated.values[0];
    /* Core-valid contract mutation, not a claim about the source builder. */
    g_mutated.instructions[0].opcode = ZR_EXEC_IR_OPCODE_NOP;
    g_mutated.instructions[0].results.count = 0u;
    g_mutated.values[0].definition = 0u;
    g_mutated.values[0].flags = ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY;
    {
        SZrExecIrDiagnostic diagnostic = {0};
        TZrBool valid = ZrCore_ExecIr_VerifyFunction(&g_mutated, ZR_EXEC_IR_VERIFY_ALL, &diagnostic);
        observation = refuse(&g_mutated);
        g_mutated.instructions[0] = savedInstruction;
        g_mutated.values[0] = savedValue;
        TEST_ASSERT_TRUE_MESSAGE(valid, "PRECONDITION: external contract mutation VERIFY_ALL");
    }
    assert_refusal(observation, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
}

static void test_missing_row(void) {
    prepare_guard();
    assert_refusal(observe_refusal(&g_mutated, g_fixture.compiler.semanticContext,
            ZR_NULL, 0u, 0u), ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
}

static void test_core_valid_missing_definition_unsupported(void) {
    TZrExecIrValueId valueId;
    TZrExecIrInstructionId savedDefinition;
    SZrExecIrDiagnostic diagnostic = {0};
    TZrBool valid;
    SRefusalObservation observation;
    prepare_guard();
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_CONSTANT, g_mutated.instructions[0].opcode);
    TEST_ASSERT_EQUAL_UINT32(1u, g_mutated.instructions[0].results.count);
    valueId = g_mutated.resultPool[g_mutated.instructions[0].results.offset];
    TEST_ASSERT_TRUE(valueId > 0u && valueId <= g_mutated.valueCount);
    savedDefinition = g_mutated.values[valueId - 1u].definition;
    TEST_ASSERT_NOT_EQUAL_UINT32(0u, savedDefinition);
    g_mutated.values[valueId - 1u].definition = 0u;
    valid = ZrCore_ExecIr_VerifyFunction(&g_mutated, ZR_EXEC_IR_VERIFY_ALL, &diagnostic);
    observation = refuse(&g_mutated);
    g_mutated.values[valueId - 1u].definition = savedDefinition;
    TEST_ASSERT_TRUE_MESSAGE(valid, "PRECONDITION: ordinary result with missing definition is Core VERIFY_ALL-valid");
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT32(0u, g_mutated.values[valueId - 1u].flags);
    TEST_ASSERT_EQUAL_UINT32(actual_return_type(), g_mutated.values[valueId - 1u].typeToken);
    assert_refusal(observation, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
}

static void test_duplicate_actual_type_row(void) {
    SZrExecIrRange range;
    SZrExecIrLayout row;
    prepare_guard();
    row = g_fixture.module.layouts[0];
    row.id = g_fixture.module.layoutCount + 1u;
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ModuleAppendLayout(&g_fixture.module, &row, 1u, &range));
    assert_refusal(refuse(&g_mutated), ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
}

static void test_invalid_row_fields(void) {
    TZrUInt32 mutation;
    prepare_guard();
    for (mutation = 0u; mutation < 6u; ++mutation) {
        SZrExecIrLayout saved = g_fixture.module.layouts[0];
        SRefusalObservation observation;
        switch (mutation) {
            case 0u: g_fixture.module.layouts[0].id = 0u; break;
            case 1u: g_fixture.module.layouts[0].layoutHash = 0u; break;
            case 2u: g_fixture.module.layouts[0].byteSize = 0u; break;
            case 3u: g_fixture.module.layouts[0].byteAlign = 0u; break;
            case 4u: g_fixture.module.layouts[0].byteAlign = 3u; break;
            default: g_fixture.module.layouts[0].typeToken = 0u; break;
        }
        observation = refuse(&g_mutated);
        g_fixture.module.layouts[0] = saved;
        assert_refusal(observation, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    }
}

static void test_frame_limit(void) {
    prepare_guard();
    TEST_ASSERT_TRUE(g_fixture.module.layouts[0].byteSize > 1u);
    assert_refusal(observe_refusal(&g_mutated, g_fixture.compiler.semanticContext,
            g_fixture.module.layouts, g_fixture.module.layoutCount,
            g_fixture.module.layouts[0].byteSize - 1u), ZR_EXEC_IR_DIAGNOSTIC_CAPACITY_OVERFLOW);
}

static void test_sealed(void) {
    SRefusalObservation observation;
    prepare_guard();
    g_mutated.sealed = ZR_TRUE;
    observation = refuse(&g_mutated);
    g_mutated.sealed = ZR_FALSE;
    assert_refusal(observation, ZR_EXEC_IR_DIAGNOSTIC_SEALED);
}

static void test_repeat_attached_frame(void) {
    prepare(ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    attach(ZR_TRUE);
    assert_refusal(refuse(&g_output), ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
}

static void bad_return_range(TZrBool sealed) {
    SZrExecIrRange saved;
    SZrExecIrDiagnostic diagnostic = {0};
    TZrBool valid;
    SRefusalObservation observation;
    prepare_guard();
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_RETURN, g_mutated.instructions[2].opcode);
    saved = g_mutated.instructions[2].operands;
    g_mutated.instructions[2].operands.offset = g_mutated.operandCount;
    g_mutated.sealed = sealed;
    valid = ZrCore_ExecIr_VerifyFunction(&g_mutated, ZR_EXEC_IR_VERIFY_ALL, &diagnostic);
    observation = refuse(&g_mutated);
    g_mutated.instructions[2].operands = saved;
    g_mutated.sealed = ZR_FALSE;
    TEST_ASSERT_FALSE(valid);
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, diagnostic.code);
    assert_refusal(observation, diagnostic.code);
    TEST_ASSERT_EQUAL_MEMORY(&diagnostic, &observation.diagnostic, sizeof(diagnostic));
}

static void test_bad_return_range(void) { bad_return_range(ZR_FALSE); }
static void test_bad_return_range_precedes_sealed(void) { bad_return_range(ZR_TRUE); }

static void test_owned_state_pool(void) {
    SZrExecIrStateMap *map;
    prepare_guard();
    map = g_mutated.stateMap;
    TEST_ASSERT_NOT_NULL(map);
    TEST_ASSERT_NULL(map->valuePool);
    TEST_ASSERT_EQUAL_UINT32(0u, g_mutated.instructions[1].results.count);
    map->valuePool = (TZrExecIrValueId *)calloc(1u, sizeof(*map->valuePool));
    TEST_ASSERT_NOT_NULL(map->valuePool);
    map->valueCount = map->valueCapacity = 1u;
    map->valuePool[0] = g_mutated.resultPool[g_mutated.instructions[0].results.offset];
    assert_verified(&g_mutated);
    assert_refusal(refuse(&g_mutated), ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
}

static void test_owned_deopt(void) {
    prepare_guard();
    TEST_ASSERT_NULL(g_mutated.deoptStates);
    TEST_ASSERT_NULL(g_mutated.deoptValues);
    TEST_ASSERT_EQUAL_UINT32(0u, g_mutated.instructions[1].results.count);
    g_mutated.deoptStates = (SZrExecIrDeoptState *)calloc(1u, sizeof(*g_mutated.deoptStates));
    TEST_ASSERT_NOT_NULL(g_mutated.deoptStates);
    g_mutated.deoptStateCount = g_mutated.deoptStateCapacity = 1u;
    g_mutated.deoptValues = (TZrExecIrValueId *)calloc(1u, sizeof(*g_mutated.deoptValues));
    TEST_ASSERT_NOT_NULL(g_mutated.deoptValues);
    g_mutated.deoptValueCount = g_mutated.deoptValueCapacity = 1u;
    g_mutated.deoptStates[0].id = g_mutated.instructions[0].sourceId;
    g_mutated.deoptStates[0].sourceId = g_mutated.instructions[0].sourceId;
    g_mutated.deoptStates[0].resumeId = g_mutated.instructions[0].sourceId;
    g_mutated.deoptStates[0].valueRange.count = 1u;
    g_mutated.deoptValues[0] = g_mutated.resultPool[g_mutated.instructions[0].results.offset];
    assert_verified(&g_mutated);
    assert_refusal(refuse(&g_mutated), ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
}

static void test_required_arguments(void) {
    prepare_guard();
    assert_refusal(observe_refusal(ZR_NULL, g_fixture.compiler.semanticContext,
            g_fixture.module.layouts, g_fixture.module.layoutCount, 0u), ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    assert_refusal(observe_refusal(&g_mutated, ZR_NULL,
            g_fixture.module.layouts, g_fixture.module.layoutCount, 0u), ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
    assert_refusal(observe_refusal(&g_mutated, g_fixture.compiler.semanticContext,
            ZR_NULL, 1u, 0u), ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT);
}

int main(int argc, char **argv) {
    TZrBool prerequisites = ZR_TRUE, features = ZR_TRUE, guards = ZR_TRUE;
    if (argc == 2 && strcmp(argv[1], "--prerequisites-only") == 0) features = guards = ZR_FALSE;
    else if (argc == 2 && strcmp(argv[1], "--features-only") == 0) prerequisites = guards = ZR_FALSE;
    else if (argc == 2 && strcmp(argv[1], "--guards-only") == 0) prerequisites = features = ZR_FALSE;
    else if (argc != 1) {
        (void)fprintf(stderr, "usage: %s [--prerequisites-only|--features-only|--guards-only]\n", argv[0]);
        return EXIT_FAILURE;
    }
    UNITY_BEGIN();
    if (prerequisites) {
        RUN_TEST(test_prerequisite_nine);
        RUN_TEST(test_prerequisite_eight);
    }
    if (features) {
        RUN_TEST(test_attach_nine);
        RUN_TEST(test_attach_eight);
        RUN_TEST(test_attach_null_diagnostic);
    }
    if (guards) {
        RUN_TEST(test_original_address_unsupported);
        RUN_TEST(test_core_valid_external_unsupported);
        RUN_TEST(test_missing_row);
        RUN_TEST(test_core_valid_missing_definition_unsupported);
        RUN_TEST(test_duplicate_actual_type_row);
        RUN_TEST(test_invalid_row_fields);
        RUN_TEST(test_frame_limit);
        RUN_TEST(test_sealed);
        RUN_TEST(test_repeat_attached_frame);
        RUN_TEST(test_bad_return_range);
        RUN_TEST(test_bad_return_range_precedes_sealed);
        RUN_TEST(test_owned_state_pool);
        RUN_TEST(test_owned_deopt);
        RUN_TEST(test_required_arguments);
    }
    return UNITY_END();
}
