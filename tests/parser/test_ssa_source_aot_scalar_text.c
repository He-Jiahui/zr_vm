#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "unity.h"
#include "harness/runtime_support.h"
#include "backend_aot/backend_aot_ir_scalar_text.h"
#include "exec_ir/aot_ir_projection_descriptor.h"
#include "zr_vm_parser/canonical_type.h"
#include "exec_ir/exec_ir_dead_source_places.h"
#include "exec_ir/exec_ir_host_aot_target.h"
#include "exec_ir/exec_ir_host_primitive_layout.h"
#include "exec_ir/source/exec_ir_source_frame.h"
#include "exec_ir/source/exec_ir_source_module_contract.h"
#include "support/ssa_literal_script_fixture.h"

static SZrState *g_state;
static SZrSsaLiteralScriptFixture g_fixture;
static SZrExecIrFunction g_compacted;
static SZrAotIrProjection g_projection;
static SZrAotIrProjectionDescriptor g_descriptor;
static SZrAotIrTargetContract g_target;

typedef EZrAotIrStatus (*FEmitter)(const SZrAotIrModule *, TZrUInt32,
        char *, size_t, size_t *, SZrAotIrDiagnostic *);

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrTests_SsaLiteralScriptFixture_Init(&g_fixture, g_state);
    ZrCore_ExecIr_FunctionInit(&g_compacted);
    memset(&g_projection, 0, sizeof(g_projection));
    memset(&g_descriptor, 0, sizeof(g_descriptor));
    memset(&g_target, 0, sizeof(g_target));
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    ZrParser_AotIrProjection_FreeDescriptor(&g_descriptor);
    ZrParser_AotIrProjection_Free(&g_projection);
    ZrCore_ExecIr_FreeFunction(&g_compacted);
    ZrTests_SsaLiteralScriptFixture_Free(&g_fixture);
    if (g_state != ZR_NULL) ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static TZrUInt64 bytes(TZrUInt64 hash, const void *data, size_t size) {
    return ZrTests_SsaLiteralScriptFixture_DigestBytes(hash, data, size);
}

/* Mutation observations over actual live owners. They are never ABI hashes. */
static TZrUInt64 source_observation(void) {
    const SZrFunction *source = g_fixture.compiler.currentFunction;
    TZrUInt64 hash = ZrTests_SsaLiteralScriptFixture_SourceDigest(&g_fixture);
    hash = bytes(hash, &g_fixture.compiler, sizeof(g_fixture.compiler));
    hash = bytes(hash, source, sizeof(*source));
    hash = bytes(hash, source->metadataTokenRecords,
            (size_t)source->metadataTokenRecordLength * sizeof(*source->metadataTokenRecords));
    hash = bytes(hash, source->moduleMetadataTokenRecords,
            (size_t)source->moduleMetadataTokenRecordLength * sizeof(*source->moduleMetadataTokenRecords));
    hash = bytes(hash, source->signatureBlobHeap, source->signatureBlobHeapLength);
    return bytes(hash, source->metadataStringHeap,
            (size_t)source->metadataStringHeapLength * sizeof(*source->metadataStringHeap));
}

static TZrUInt64 graph_observation(const SZrExecIrFunction *function) {
    TZrUInt64 hash = ZrTests_SsaLiteralScriptFixture_FunctionDigest(function);
    if (function->frameLayout != ZR_NULL) {
        hash = bytes(hash, function->frameLayout, sizeof(*function->frameLayout));
        hash = bytes(hash, function->frameLayout->slots,
                (size_t)function->frameLayout->slotCount * sizeof(*function->frameLayout->slots));
    }
    return hash;
}

static TZrUInt64 projection_observation(void) {
    TZrUInt64 hash = bytes(0u, &g_projection, sizeof(g_projection));
#define POOL(field, count) \
    hash = bytes(hash, g_projection.field, (size_t)g_projection.count * sizeof(*g_projection.field))
    POOL(frameSlots, frameSlotCount);
    POOL(opcodes, instructionCount);
    POOL(instructions, instructionCount);
    POOL(operands, operandCount);
    POOL(results, resultCount);
    POOL(memoryTokens, memoryTokenCount);
    POOL(valueSlots, valueSlotCount);
    POOL(slotValues, physicalSlotCount);
    POOL(blocks, blockCount);
    POOL(predecessors, predecessorCount);
    POOL(successors, successorCount);
    POOL(phis, phiCount);
    POOL(phiIncomings, phiIncomingCount);
    POOL(phiCopySources, phiCopyCount);
    POOL(phiCopyDestinations, phiCopyCount);
    POOL(phiCopyEdges, phiCopyCount);
    POOL(phiMoves, phiMoveCount);
    POOL(sourceMaps, sourceMapCount);
    POOL(constants, constantCount);
    POOL(layouts, layoutCount);
    POOL(gcRoots, gcRootCount);
    POOL(deoptStates, deoptStateCount);
    POOL(deoptValues, deoptValueCount);
    POOL(deoptAggregates, deoptAggregateCount);
    POOL(deoptAggregateFields, deoptAggregateFieldCount);
#undef POOL
#define STATE_POOL(field, count) \
    hash = bytes(hash, g_projection.stateMap.field, \
            (size_t)g_projection.stateMap.count * sizeof(*g_projection.stateMap.field))
    STATE_POOL(entries, entryCount);
    STATE_POOL(valuePool, valueCount);
    STATE_POOL(rootPool, rootCount);
    STATE_POOL(ownerStatePool, ownerStateCount);
#undef STATE_POOL
    return hash;
}

static TZrUInt64 inputs_observation(void) {
    const SZrAotIrFunction *function = &g_descriptor.function;
    TZrUInt64 hash = source_observation();
    TZrUInt64 original = graph_observation(ZrTests_SsaLiteralScriptFixture_Function(&g_fixture));
    TZrUInt64 compacted = graph_observation(&g_compacted);
    TZrUInt64 projected = projection_observation();
    hash = bytes(hash, &original, sizeof(original));
    hash = bytes(hash, &compacted, sizeof(compacted));
    hash = bytes(hash, &projected, sizeof(projected));
    hash = bytes(hash, &g_target, sizeof(g_target));
    hash = bytes(hash, &g_fixture.module, sizeof(g_fixture.module));
    hash = bytes(hash, g_fixture.module.constants,
            (size_t)g_fixture.module.constantCount * sizeof(*g_fixture.module.constants));
    hash = bytes(hash, g_fixture.module.layouts,
            (size_t)g_fixture.module.layoutCount * sizeof(*g_fixture.module.layouts));
    hash = bytes(hash, &g_descriptor, sizeof(g_descriptor));
#define DESCRIPTOR_POOL(field, count) \
    hash = bytes(hash, function->field, (size_t)function->count * sizeof(*function->field))
    DESCRIPTOR_POOL(instructions, instructionCount);
    DESCRIPTOR_POOL(blocks, blockCount);
    DESCRIPTOR_POOL(frameSlots, frameSlotCount);
    DESCRIPTOR_POOL(sourceMaps, sourceMapCount);
    DESCRIPTOR_POOL(operandPool, operandCount);
    DESCRIPTOR_POOL(resultPool, resultCount);
    DESCRIPTOR_POOL(valueSlotPool, valueSlotCount);
    DESCRIPTOR_POOL(successorPool, successorCount);
    DESCRIPTOR_POOL(memoryTokenPool, memoryTokenCount);
    DESCRIPTOR_POOL(phiIncomingPool, phiIncomingCount);
    DESCRIPTOR_POOL(gcRootPool, gcRootCount);
    DESCRIPTOR_POOL(deoptStates, deoptStateCount);
    DESCRIPTOR_POOL(deoptValuePool, deoptValueCount);
    DESCRIPTOR_POOL(deoptAggregates, deoptAggregateCount);
    DESCRIPTOR_POOL(deoptAggregateFields, deoptAggregateFieldCount);
#undef DESCRIPTOR_POOL
    hash = bytes(hash, g_descriptor.module.constantPool,
            (size_t)g_descriptor.module.constantCount * sizeof(*g_descriptor.module.constantPool));
    return bytes(hash, g_descriptor.module.layoutPool,
            (size_t)g_descriptor.module.layoutCount * sizeof(*g_descriptor.module.layoutPool));
}

static TZrTypeId return_type(void) {
    const SZrCanonicalTypeNode *callable = ZrParser_CanonicalType_Find(
            g_fixture.compiler.semanticContext, g_fixture.compiler.preSemanticIr.callableTypeId);
    const SZrCanonicalTypeNode *primitive;
    TEST_ASSERT_NOT_NULL(callable);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, callable->kind);
    primitive = ZrParser_CanonicalType_Find(g_fixture.compiler.semanticContext,
            callable->data.function.returnTypeId);
    TEST_ASSERT_NOT_NULL(primitive);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_PRIMITIVE, primitive->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, primitive->data.primitive.valueType);
    return callable->data.function.returnTypeId;
}

static void project_source(EZrSsaLiteralScriptSource source) {
    const SZrExecIrFunction *original;
    SZrExecIrDiagnostic diagnostic = {0};
    SZrExecIrLayout row;
    SZrExecIrRange range;
    TZrTypeId type;
    TZrUInt64 sourceBefore;
    TZrUInt32 index;
    ZrTests_SsaLiteralScriptFixture_Prepare(&g_fixture, source);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_BindSourceModuleContract(&g_fixture.compiler,
            &g_fixture.module, &diagnostic), "PRECONDITION: actual source binder GREEN");
    original = ZrTests_SsaLiteralScriptFixture_Function(&g_fixture);
    sourceBefore = source_observation();
    type = return_type();
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(original, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &g_fixture.compiler.preSemanticIr, g_fixture.compiler.semanticContext,
            original, &g_compacted, &diagnostic), "PRECONDITION: true dead-place compaction");
    TEST_ASSERT_EQUAL_UINT32(original->id, g_compacted.id);
    TEST_ASSERT_EQUAL_UINT32(original->functionToken, g_compacted.functionToken);
    TEST_ASSERT_EQUAL_UINT64(original->signatureHash, g_compacted.signatureHash);
    TEST_ASSERT_EQUAL_UINT64(original->contract.moduleHash, g_compacted.contract.moduleHash);
    TEST_ASSERT_EQUAL_UINT32(3u, g_compacted.instructionCount);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_CONSTANT, g_compacted.instructions[0].opcode);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_NOP, g_compacted.instructions[1].opcode);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_RETURN, g_compacted.instructions[2].opcode);
    TEST_ASSERT_EQUAL_UINT32(1u, g_compacted.valueCount);
    TEST_ASSERT_EQUAL_UINT32(original->values[0].id, g_compacted.values[0].id);
    TEST_ASSERT_EQUAL_UINT32(original->values[0].definitionInstructionId,
            g_compacted.values[0].definitionInstructionId);
    TEST_ASSERT_EQUAL_UINT32(original->entryBlockId, g_compacted.entryBlockId);
    for (index = 0u; index < g_compacted.instructionCount; ++index)
        TEST_ASSERT_EQUAL_UINT32(original->instructions[index].sourceId, g_compacted.instructions[index].sourceId);
    ZrTests_SsaLiteralScriptFixture_AssertSourceMaps(&g_fixture, &g_compacted);
    TEST_ASSERT_EQUAL_UINT32(type, g_compacted.values[0].typeToken);
    TEST_ASSERT_EQUAL_UINT32(0u, g_fixture.module.layoutCount);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_MakeHostPrimitiveLayout(g_fixture.compiler.semanticContext,
            type, 1u, &row, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(type, row.typeToken);
    TEST_ASSERT_EQUAL_UINT32(sizeof(TZrInt64), row.byteSize);
    TEST_ASSERT_EQUAL_UINT32(alignof(TZrInt64), row.byteAlign);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, row.layoutHash);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ModuleAppendLayout(&g_fixture.module, &row, 1u, &range));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_AttachPrimitiveSourceFrame(&g_compacted,
            g_fixture.compiler.semanticContext, g_fixture.module.layouts,
            g_fixture.module.layoutCount, 0u, &diagnostic));
    TEST_ASSERT_NOT_NULL(g_compacted.frameLayout);
    TEST_ASSERT_EQUAL_UINT32(1u, g_compacted.frameLayout->slotCount);
    TEST_ASSERT_EQUAL_UINT32(g_compacted.values[0].id, g_compacted.frameLayout->slots[0].slotId);
    TEST_ASSERT_EQUAL_UINT64(g_compacted.frameLayout->layoutHash, g_compacted.contract.layoutHash);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(&g_compacted, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LowerAotWithCanonicalCallable(&g_compacted,
            g_fixture.module.constants, g_fixture.module.constantCount,
            g_fixture.module.layouts, g_fixture.module.layoutCount,
            g_fixture.compiler.semanticContext, g_fixture.compiler.preSemanticIr.callableTypeId,
            &g_projection, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(3u, g_projection.instructionCount);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_OPCODE_NOP, g_projection.opcodes[1]);
    TEST_ASSERT_EQUAL_UINT32(g_compacted.functionToken, g_projection.functionToken);
    TEST_ASSERT_EQUAL_UINT64(g_compacted.signatureHash, g_projection.signatureHash);
    TEST_ASSERT_EQUAL_UINT64(g_compacted.contract.moduleHash, g_projection.contract.moduleHash);
    TEST_ASSERT_EQUAL_UINT32(1u, g_projection.frameSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, g_projection.valueSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, g_projection.physicalSlotCount);
    TEST_ASSERT_EQUAL_UINT32(0u, g_projection.valueSlots[0]);
    TEST_ASSERT_EQUAL_MEMORY(g_compacted.values, g_projection.slotValues, sizeof(*g_compacted.values));
    TEST_ASSERT_EQUAL_MEMORY(g_compacted.frameLayout->slots, g_projection.frameSlots,
            sizeof(*g_projection.frameSlots));
    TEST_ASSERT_EQUAL_MEMORY(g_fixture.module.constants, g_projection.constants,
            (size_t)g_projection.constantCount * sizeof(*g_projection.constants));
    TEST_ASSERT_EQUAL_INT64(g_fixture.expectedReturn, (TZrInt64)g_projection.constants[0].bits);
    TEST_ASSERT_EQUAL_MEMORY(g_fixture.module.layouts, g_projection.layouts,
            (size_t)g_projection.layoutCount * sizeof(*g_projection.layouts));
    TEST_ASSERT_EQUAL_UINT32(g_compacted.sourceMapCount, g_projection.sourceMapCount);
    for (index = 0u; index < g_compacted.sourceMapCount; ++index) {
        const SZrExecIrSourceMap *map = &g_compacted.sourceMaps[index];
        const SZrExecIrProjectionSourceMap *out = &g_projection.sourceMaps[index];
        TEST_ASSERT_EQUAL_UINT32(map->instructionId - 1u, out->pc);
        TEST_ASSERT_EQUAL_UINT32(map->sourceId, out->sourceId);
        TEST_ASSERT_EQUAL_UINT32(map->startOffset, out->startOffset);
        TEST_ASSERT_EQUAL_UINT32(map->endOffset, out->endOffset);
        TEST_ASSERT_EQUAL_UINT32(map->startLine, out->startLine);
        TEST_ASSERT_EQUAL_UINT32(map->startColumn, out->startColumn);
        TEST_ASSERT_EQUAL_UINT32(map->endLine, out->endLine);
        TEST_ASSERT_EQUAL_UINT32(map->endColumn, out->endColumn);
    }
    TEST_ASSERT_NOT_NULL(original->stateMap);
    TEST_ASSERT_NOT_NULL(g_compacted.stateMap);
    TEST_ASSERT_TRUE(g_projection.stateMapPresent);
    TEST_ASSERT_EQUAL_UINT32(original->stateMap->functionToken, g_projection.stateMap.functionToken);
    TEST_ASSERT_EQUAL_UINT64(original->stateMap->signatureHash, g_projection.stateMap.signatureHash);
    TEST_ASSERT_EQUAL_UINT64(original->stateMap->generation, g_projection.stateMap.generation);
#define EMPTY_STATE(field) \
    TEST_ASSERT_EQUAL_UINT32(0u, original->stateMap->field); \
    TEST_ASSERT_EQUAL_UINT32(0u, g_compacted.stateMap->field); \
    TEST_ASSERT_EQUAL_UINT32(0u, g_projection.stateMap.field)
    EMPTY_STATE(entryCount);
    EMPTY_STATE(valueCount);
    EMPTY_STATE(rootCount);
    EMPTY_STATE(ownerStateCount);
#undef EMPTY_STATE
    TEST_ASSERT_FALSE(g_projection.runnable);
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_observation());
}

#if defined(_WIN32) && defined(_WIN64) && defined(_MSC_VER) && \
        (defined(_M_X64) || defined(_M_AMD64)) && !defined(_M_ARM64) && !defined(_M_ARM64EC)

static void assert_executable(void) {
    SZrAotIrDiagnostic diagnostic = {0};
    SZrAotIrCallableAbi abi = {0};
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AOT_IR_OK,
            ZrCore_AotIr_ValidateModule(&g_descriptor.module, &diagnostic),
            "PRECONDITION: actual descriptor Core validity");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AOT_IR_OK, ZrCore_AotIr_RequireExecutableAbi(
            &g_descriptor.module, g_descriptor.function.id, &abi, &diagnostic),
            "PRECONDITION: actual descriptor executable ABI");
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64, abi.kind);
    TEST_ASSERT_EQUAL_UINT32(return_type(), abi.returnTypeToken);
}

static void prepare(EZrSsaLiteralScriptSource source) {
    SZrAotIrDiagnostic diagnostic = {0};
    TZrUInt32 index;
    project_source(source);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_MakeHostNoArgsI64AotTarget(
            g_fixture.compiler.semanticContext, g_fixture.compiler.preSemanticIr.callableTypeId,
            &g_fixture.module.layouts[0], &g_target, &diagnostic), "PRECONDITION: actual host target");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_AotIrProjection_BuildDescriptor(&g_projection, &g_target,
            &g_fixture.module.contract, &g_descriptor, &diagnostic), "PRECONDITION: real source descriptor");
    TEST_ASSERT_EQUAL_PTR(&g_projection, g_descriptor.owner);
    TEST_ASSERT_EQUAL_PTR(&g_descriptor.function, g_descriptor.module.functions);
    TEST_ASSERT_EQUAL_MEMORY(&g_fixture.module.contract, &g_descriptor.module.contract,
            sizeof(g_fixture.module.contract));
    TEST_ASSERT_EQUAL_MEMORY(&g_projection.contract, &g_descriptor.function.contract,
            sizeof(g_projection.contract));
    TEST_ASSERT_EQUAL_UINT32(3u, g_descriptor.function.instructionCount);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_OPCODE_CONSTANT, g_descriptor.instructions[0].opcode);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_OPCODE_NOP, g_descriptor.instructions[1].opcode);
    TEST_ASSERT_EQUAL_UINT32(0u, g_descriptor.instructions[1].results.count);
    TEST_ASSERT_TRUE(g_descriptor.instructions[1].results.offset != 0u);
    TEST_ASSERT_EQUAL_UINT32(g_descriptor.instructions[0].results.offset +
            g_descriptor.instructions[0].results.count,
            g_descriptor.instructions[1].results.offset);
    TEST_ASSERT_EQUAL_UINT32(g_descriptor.function.callableAbi.returnTypeToken,
            g_descriptor.instructions[1].typeToken);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_OPCODE_RETURN, g_descriptor.instructions[2].opcode);
    TEST_ASSERT_EQUAL_UINT32(g_compacted.blocks[0].terminatorInstructionId, g_descriptor.instructions[2].id);
    TEST_ASSERT_EQUAL_UINT32(1u, g_descriptor.module.constantCount);
    TEST_ASSERT_EQUAL_INT64(g_fixture.expectedReturn, (TZrInt64)g_descriptor.constants[0].bits);
    TEST_ASSERT_EQUAL_UINT32(1u, g_descriptor.module.layoutCount);
    TEST_ASSERT_EQUAL_PTR(&g_projection.stateMap, g_descriptor.function.logicalStateMap);
    TEST_ASSERT_EQUAL_UINT32(g_projection.sourceMapCount, g_descriptor.function.sourceMapCount);
    for (index = 0u; index < g_projection.instructionCount; ++index)
        TEST_ASSERT_EQUAL_UINT32(g_projection.instructions[index].sourceId,
                g_descriptor.instructions[index].sourceId);
    for (index = 0u; index < g_projection.sourceMapCount; ++index) {
        const SZrExecIrProjectionSourceMap *map = &g_projection.sourceMaps[index];
        const SZrAotIrSourceMap *out = &g_descriptor.sourceMaps[index];
        TEST_ASSERT_EQUAL_UINT32(map->pc + 1u, out->instructionId);
        TEST_ASSERT_EQUAL_UINT32(map->sourceId, out->sourceId);
        TEST_ASSERT_EQUAL_UINT32(map->startOffset, out->startOffset);
        TEST_ASSERT_EQUAL_UINT32(map->endOffset, out->endOffset);
        TEST_ASSERT_EQUAL_UINT32(map->startLine, out->startLine);
        TEST_ASSERT_EQUAL_UINT32(map->startColumn, out->startColumn);
        TEST_ASSERT_EQUAL_UINT32(map->endLine, out->endLine);
        TEST_ASSERT_EQUAL_UINT32(map->endColumn, out->endColumn);
    }
    assert_executable();
}

/* Consume the whole frozen formatter grammar. No substring-only acceptance:
 * this establishes one zero-parameter i64 function and one exact return. */
static void consume(const char **cursor, const char *end, const char *literal) {
    size_t count = strlen(literal);
    TEST_ASSERT_TRUE((size_t)(end - *cursor) >= count);
    TEST_ASSERT_EQUAL_MEMORY(literal, *cursor, count);
    *cursor += count;
}

static TZrUInt64 decimal(const char **cursor, const char *end) {
    TZrUInt64 value = 0u;
    const char *start = *cursor;
    while (*cursor != end && **cursor >= '0' && **cursor <= '9') {
        TZrUInt64 digit = (TZrUInt64)(**cursor - '0');
        TEST_ASSERT_TRUE(value <= (UINT64_MAX - digit) / 10u);
        value = value * 10u + digit;
        ++*cursor;
    }
    TEST_ASSERT_TRUE(*cursor != start);
    return value;
}

static void parse_text(const char *text, size_t length, TZrBool llvm) {
    const char *cursor = text;
    const char *end = text + length;
    TEST_ASSERT_EQUAL_CHAR('\0', text[length]);
    TEST_ASSERT_EQUAL_UINT64(length, strlen(text));
    consume(&cursor, end, llvm ? "define i64 @zr_aot_scalar_fn_"
            : "#include <stdint.h>\nint64_t zr_aot_scalar_fn_");
    TEST_ASSERT_EQUAL_UINT64(g_descriptor.function.id, decimal(&cursor, end));
    consume(&cursor, end, llvm ? "() {\nentry:\n  ret i64 " : "(void) {\n    return INT64_C(");
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)g_fixture.expectedReturn, decimal(&cursor, end));
    consume(&cursor, end, llvm ? "\n}\n" : ");\n}\n");
    TEST_ASSERT_EQUAL_PTR(end, cursor);
}

static void emit_success(FEmitter emitter, TZrBool llvm, EZrSsaLiteralScriptSource source,
        TZrBool nullDiagnostic) {
    char text[256];
    char featureMessage[768];
    size_t length = 99u;
    SZrAotIrDiagnostic diagnostic = {0};
    TZrUInt64 before;
    EZrAotIrStatus status;
    prepare(source);
    before = inputs_observation();
    memset(text, 0xa5, sizeof(text));
    status = emitter(&g_descriptor.module, g_descriptor.function.id, text, sizeof(text),
            &length, nullDiagnostic ? ZR_NULL : &diagnostic);
    (void)snprintf(featureMessage, sizeof(featureMessage),
            "FEATURE RED: existing scalar emitter must accept actual source descriptor"
            " status=%d instruction=%lu nop=%lu results={%lu,%lu} operands={%lu,%lu}"
            " flags=%lu type=%lu match=%lu layout=%lu effects={%lu,%lu} binding=%lu deopt=%lu"
            " successors={%lu,%lu} phi={%lu,%lu} memoryIn={%lu,%lu} memoryOut={%lu,%lu}",
            (int)status, (unsigned long)diagnostic.instructionId,
            (unsigned long)g_descriptor.instructions[1].id,
            (unsigned long)g_descriptor.instructions[1].results.offset,
            (unsigned long)g_descriptor.instructions[1].results.count,
            (unsigned long)g_descriptor.instructions[1].operands.offset,
            (unsigned long)g_descriptor.instructions[1].operands.count,
            (unsigned long)g_descriptor.instructions[1].flags,
            (unsigned long)g_descriptor.instructions[1].typeToken,
            (unsigned long)g_descriptor.instructions[1].matchTypeToken,
            (unsigned long)g_descriptor.instructions[1].layoutId,
            (unsigned long)g_descriptor.instructions[1].effectIn,
            (unsigned long)g_descriptor.instructions[1].effectOut,
            (unsigned long)g_descriptor.instructions[1].bindingRow,
            (unsigned long)g_descriptor.instructions[1].deoptId,
            (unsigned long)g_descriptor.instructions[1].successors.offset,
            (unsigned long)g_descriptor.instructions[1].successors.count,
            (unsigned long)g_descriptor.instructions[1].phiIncoming.offset,
            (unsigned long)g_descriptor.instructions[1].phiIncoming.count,
            (unsigned long)g_descriptor.instructions[1].memoryIn.offset,
            (unsigned long)g_descriptor.instructions[1].memoryIn.count,
            (unsigned long)g_descriptor.instructions[1].memoryOut.offset,
            (unsigned long)g_descriptor.instructions[1].memoryOut.count);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AOT_IR_OK, status, featureMessage);
    if (!nullDiagnostic) TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, diagnostic.status);
    TEST_ASSERT_TRUE(length < sizeof(text));
    parse_text(text, length, llvm);
    TEST_ASSERT_EQUAL_UINT64(before, inputs_observation());
    TEST_ASSERT_FALSE(g_projection.runnable);
}

static void test_prerequisite_real_nine_descriptor(void) { prepare(ZR_TEST_SSA_LITERAL_SCRIPT_NINE); }
static void test_prerequisite_real_eight_descriptor(void) { prepare(ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT); }
static void test_real_nine_c_text(void) { emit_success(backend_aot_ir_c_emit_const_i64, ZR_FALSE, ZR_TEST_SSA_LITERAL_SCRIPT_NINE, ZR_FALSE); }
static void test_real_eight_c_text(void) { emit_success(backend_aot_ir_c_emit_const_i64, ZR_FALSE, ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT, ZR_FALSE); }
static void test_real_nine_llvm_text(void) { emit_success(backend_aot_ir_llvm_emit_const_i64, ZR_TRUE, ZR_TEST_SSA_LITERAL_SCRIPT_NINE, ZR_FALSE); }
static void test_real_eight_llvm_text(void) { emit_success(backend_aot_ir_llvm_emit_const_i64, ZR_TRUE, ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT, ZR_FALSE); }
static void test_c_null_diagnostic(void) { emit_success(backend_aot_ir_c_emit_const_i64, ZR_FALSE, ZR_TEST_SSA_LITERAL_SCRIPT_NINE, ZR_TRUE); }
static void test_llvm_null_diagnostic(void) { emit_success(backend_aot_ir_llvm_emit_const_i64, ZR_TRUE, ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT, ZR_TRUE); }

static void capacity_boundary(FEmitter emitter, TZrBool llvm) {
    char text[256];
    size_t length = 99u, requiredLength;
    TZrUInt64 before;
    SZrAotIrDiagnostic diagnostic = {0};
    prepare(ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    before = inputs_observation();
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, emitter(&g_descriptor.module, g_descriptor.function.id,
            text, sizeof(text), &length, &diagnostic));
    requiredLength = length;
    TEST_ASSERT_TRUE(requiredLength > 0u && requiredLength < sizeof(text));
    parse_text(text, length, llvm);
    memset(text, 0xa5, sizeof(text));
    length = 99u;
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_RANGE, emitter(&g_descriptor.module,
            g_descriptor.function.id, text, requiredLength, &length, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_RANGE, diagnostic.status);
    TEST_ASSERT_EQUAL_UINT64(requiredLength + 1u, diagnostic.expected);
    TEST_ASSERT_EQUAL_UINT64(requiredLength, diagnostic.actual);
    TEST_ASSERT_EQUAL_CHAR('\0', text[0]);
    TEST_ASSERT_EQUAL_UINT64(0u, length);
    TEST_ASSERT_EQUAL_UINT8(0xa5u, (unsigned char)text[requiredLength]);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, emitter(&g_descriptor.module, g_descriptor.function.id,
            text, requiredLength + 1u, &length, &diagnostic));
    TEST_ASSERT_EQUAL_UINT64(requiredLength, length);
    parse_text(text, length, llvm);
    TEST_ASSERT_EQUAL_UINT64(before, inputs_observation());
}

static void test_c_exact_capacity_boundary(void) { capacity_boundary(backend_aot_ir_c_emit_const_i64, ZR_FALSE); }
static void test_llvm_exact_capacity_boundary(void) { capacity_boundary(backend_aot_ir_llvm_emit_const_i64, ZR_TRUE); }

static void invalid_outputs(FEmitter emitter) {
    char text[256];
    size_t length = 99u;
    SZrAotIrDiagnostic diagnostic = {0};
    TZrUInt64 before;
    prepare(ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    before = inputs_observation();
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_ARGUMENT, emitter(&g_descriptor.module,
            g_descriptor.function.id, ZR_NULL, sizeof(text), &length, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_ARGUMENT, diagnostic.status);
    TEST_ASSERT_EQUAL_UINT64(0u, length);
    text[0] = 'x';
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_ARGUMENT, emitter(&g_descriptor.module,
            g_descriptor.function.id, text, sizeof(text), ZR_NULL, &diagnostic));
    TEST_ASSERT_EQUAL_CHAR('\0', text[0]);
    text[0] = 'x';
    length = 99u;
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_ARGUMENT, emitter(&g_descriptor.module,
            g_descriptor.function.id, text, 0u, &length, ZR_NULL));
    TEST_ASSERT_EQUAL_CHAR('x', text[0]);
    TEST_ASSERT_EQUAL_UINT64(0u, length);
    TEST_ASSERT_EQUAL_UINT64(before, inputs_observation());
}

static void test_c_required_outputs(void) { invalid_outputs(backend_aot_ir_c_emit_const_i64); }
static void test_llvm_required_outputs(void) { invalid_outputs(backend_aot_ir_llvm_emit_const_i64); }

static void extra_actual_layout(FEmitter emitter) {
    char text[256];
    size_t length = 99u;
    SZrAotIrDiagnostic diagnostic = {0};
    SZrExecIrDiagnostic rowDiagnostic = {0};
    SZrExecIrLayout row;
    SZrExecIrRange range;
    SZrAotIrModule expanded, expandedBefore;
    SZrAotIrCallableAbi abi = {0};
    TZrUInt64 before;
    prepare(ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_MakeHostPrimitiveLayout(g_fixture.compiler.semanticContext,
            return_type(), 2u, &row, &rowDiagnostic));
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ModuleAppendLayout(&g_fixture.module, &row, 1u, &range));
    TEST_ASSERT_EQUAL_UINT32(1u, range.offset);
    TEST_ASSERT_EQUAL_UINT32(2u, g_fixture.module.layoutCount);
    /* A read-only consumer view of the actual expanded, still-live row owner.
     * The original descriptor and projection remain the actual initial chain. */
    memcpy(&expanded, &g_descriptor.module, sizeof(expanded));
    expanded.layoutPool = g_fixture.module.layouts;
    expanded.layoutCount = g_fixture.module.layoutCount;
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_ValidateModule(&expanded, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_RequireExecutableAbi(&expanded,
            g_descriptor.function.id, &abi, &diagnostic));
    memcpy(&expandedBefore, &expanded, sizeof(expandedBefore));
    before = inputs_observation();
    text[0] = 'x';
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_UNSUPPORTED, emitter(&expanded,
            g_descriptor.function.id, text, sizeof(text), &length, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_UNSUPPORTED, diagnostic.status);
    TEST_ASSERT_EQUAL_UINT32(g_descriptor.function.id, diagnostic.functionId);
    TEST_ASSERT_EQUAL_CHAR('\0', text[0]);
    TEST_ASSERT_EQUAL_UINT64(0u, length);
    TEST_ASSERT_EQUAL_UINT64(before, inputs_observation());
    TEST_ASSERT_EQUAL_MEMORY(&expandedBefore, &expanded, sizeof(expanded));
}

static void test_c_extra_actual_layout_is_unsupported(void) { extra_actual_layout(backend_aot_ir_c_emit_const_i64); }
static void test_llvm_extra_actual_layout_is_unsupported(void) { extra_actual_layout(backend_aot_ir_llvm_emit_const_i64); }

static void nop_refusal(FEmitter emitter, TZrBool coreInvalid) {
    SZrAotIrInstruction saved, perturbed, observed;
    SZrAotIrInstruction *nop;
    SZrAotIrDiagnostic coreDiagnostic = {0}, diagnostic = {0};
    SZrAotIrCallableAbi abi = {0};
    EZrAotIrStatus coreStatus, status;
    char text[256];
    size_t length = 99u;
    TZrUInt64 before;
    prepare(ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    nop = &g_descriptor.instructions[1];
    TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_OPCODE_NOP, nop->opcode);
    before = inputs_observation();
    memcpy(&saved, nop, sizeof(saved));
    if (coreInvalid) nop->flags |= (ZR_EXEC_IR_INSTRUCTION_FLAG_KNOWN_MASK + 1u);
    else nop->typeToken = saved.typeToken == 1u ? 2u : 1u;
    memcpy(&perturbed, nop, sizeof(perturbed));
    coreStatus = ZrCore_AotIr_RequireExecutableAbi(&g_descriptor.module,
            g_descriptor.function.id, &abi, &coreDiagnostic);
    text[0] = 'x';
    status = emitter(&g_descriptor.module, g_descriptor.function.id, text, sizeof(text), &length, &diagnostic);
    memcpy(&observed, nop, sizeof(observed));
    memcpy(nop, &saved, sizeof(saved));
    if (!coreInvalid) {
        TEST_ASSERT_TRUE(perturbed.typeToken != 0u);
        TEST_ASSERT_TRUE(perturbed.typeToken != saved.typeToken);
    }
    TEST_ASSERT_EQUAL_MEMORY(&perturbed, &observed, sizeof(observed));
    TEST_ASSERT_EQUAL_INT(coreInvalid ? ZR_AOT_IR_INVALID_OPCODE : ZR_AOT_IR_OK, coreStatus);
    TEST_ASSERT_EQUAL_INT(coreInvalid ? ZR_AOT_IR_INVALID_OPCODE : ZR_AOT_IR_UNSUPPORTED, status);
    TEST_ASSERT_EQUAL_INT(status, diagnostic.status);
    TEST_ASSERT_EQUAL_UINT32(g_descriptor.function.id, diagnostic.functionId);
    TEST_ASSERT_EQUAL_UINT32(saved.id, diagnostic.instructionId);
    if (coreInvalid) TEST_ASSERT_EQUAL_MEMORY(&coreDiagnostic, &diagnostic, sizeof(diagnostic));
    TEST_ASSERT_EQUAL_CHAR('\0', text[0]);
    TEST_ASSERT_EQUAL_UINT64(0u, length);
    TEST_ASSERT_EQUAL_UINT64(before, inputs_observation());
}

static void test_c_nop_semantic_type_is_unsupported(void) { nop_refusal(backend_aot_ir_c_emit_const_i64, ZR_FALSE); }
static void test_llvm_nop_semantic_type_is_unsupported(void) { nop_refusal(backend_aot_ir_llvm_emit_const_i64, ZR_FALSE); }
static void test_c_nop_core_error_propagates(void) { nop_refusal(backend_aot_ir_c_emit_const_i64, ZR_TRUE); }
static void test_llvm_nop_core_error_propagates(void) { nop_refusal(backend_aot_ir_llvm_emit_const_i64, ZR_TRUE); }

/* Core validates these individual records; the scalar consumer also requires
 * their cross-record agreement. Restore whole owners before any assertions. */
static void source_layout_mismatch(FEmitter emitter, TZrBool rowWidth) {
    SZrAotIrFrameSlot savedSlot, perturbedSlot, observedSlot;
    SZrExecIrLayout savedRow, perturbedRow, observedRow;
    SZrAotIrDiagnostic moduleDiagnostic = {0}, abiDiagnostic = {0};
    SZrAotIrDiagnostic diagnostic, expectedDiagnostic = {0};
    SZrAotIrCallableAbi abi = {0};
    EZrAotIrStatus moduleStatus, abiStatus, status;
    TZrUInt32 mutatedToken, functionId, instructionId;
    TZrUInt64 before;
    char text[256];
    size_t length = 99u;
    prepare(rowWidth ? ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT : ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    TEST_ASSERT_EQUAL_UINT32(1u, g_descriptor.function.frameSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, g_descriptor.module.layoutCount);
    TEST_ASSERT_EQUAL_UINT32(8u, g_descriptor.layouts[0].byteSize);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, g_descriptor.layouts[0].layoutHash);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_OPCODE_CONSTANT, g_descriptor.instructions[0].opcode);
    functionId = g_descriptor.function.id;
    instructionId = g_descriptor.instructions[0].id;
    before = inputs_observation();
    memcpy(&savedSlot, &g_descriptor.frameSlots[0], sizeof(savedSlot));
    memcpy(&savedRow, &g_descriptor.layouts[0], sizeof(savedRow));
    mutatedToken = savedSlot.typeToken == 1u ? 2u : 1u;
    if (rowWidth) g_descriptor.layouts[0].byteSize = 4u;
    else g_descriptor.frameSlots[0].typeToken = mutatedToken;
    memcpy(&perturbedSlot, &g_descriptor.frameSlots[0], sizeof(perturbedSlot));
    memcpy(&perturbedRow, &g_descriptor.layouts[0], sizeof(perturbedRow));
    moduleStatus = ZrCore_AotIr_ValidateModule(&g_descriptor.module, &moduleDiagnostic);
    abiStatus = ZrCore_AotIr_RequireExecutableAbi(&g_descriptor.module,
            functionId, &abi, &abiDiagnostic);
    memset(text, 0xa5, sizeof(text));
    memset(&diagnostic, 0xa5, sizeof(diagnostic));
    status = emitter(&g_descriptor.module, functionId, text, sizeof(text), &length, &diagnostic);
    memcpy(&observedSlot, &g_descriptor.frameSlots[0], sizeof(observedSlot));
    memcpy(&observedRow, &g_descriptor.layouts[0], sizeof(observedRow));
    memcpy(&g_descriptor.frameSlots[0], &savedSlot, sizeof(savedSlot));
    memcpy(&g_descriptor.layouts[0], &savedRow, sizeof(savedRow));
    expectedDiagnostic.status = ZR_AOT_IR_INVALID_LAYOUT;
    expectedDiagnostic.functionId = functionId;
    expectedDiagnostic.instructionId = instructionId;
    expectedDiagnostic.expected = rowWidth ? 8u : abi.returnTypeToken;
    expectedDiagnostic.actual = rowWidth ? 4u : mutatedToken;
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, moduleStatus);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, abiStatus);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64, abi.kind);
    TEST_ASSERT_EQUAL_UINT32(savedSlot.typeToken, abi.returnTypeToken);
    TEST_ASSERT_EQUAL_MEMORY(&perturbedSlot, &observedSlot, sizeof(observedSlot));
    TEST_ASSERT_EQUAL_MEMORY(&perturbedRow, &observedRow, sizeof(observedRow));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_LAYOUT, status);
    TEST_ASSERT_EQUAL_MEMORY(&expectedDiagnostic, &diagnostic, sizeof(diagnostic));
    TEST_ASSERT_EQUAL_CHAR('\0', text[0]);
    TEST_ASSERT_EQUAL_UINT64(0u, length);
    TEST_ASSERT_EQUAL_UINT64(before, inputs_observation());
}

static void test_c_actual_frame_type_mismatch(void) { source_layout_mismatch(backend_aot_ir_c_emit_const_i64, ZR_FALSE); }
static void test_llvm_actual_frame_type_mismatch(void) { source_layout_mismatch(backend_aot_ir_llvm_emit_const_i64, ZR_FALSE); }
static void test_c_actual_layout_width_mismatch(void) { source_layout_mismatch(backend_aot_ir_c_emit_const_i64, ZR_TRUE); }
static void test_llvm_actual_layout_width_mismatch(void) { source_layout_mismatch(backend_aot_ir_llvm_emit_const_i64, ZR_TRUE); }

#else
static void test_other_host_actual_target_is_unsupported(void) {
    SZrAotIrDiagnostic diagnostic = {0};
    SZrAotIrTargetContract before, observed;
    TZrUInt64 inputs;
    TZrBool result;
    project_source(ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    inputs = inputs_observation();
    memset(&g_target, 0xa5, sizeof(g_target));
    memcpy(&before, &g_target, sizeof(before));
    result = ZrParser_ExecIr_MakeHostNoArgsI64AotTarget(g_fixture.compiler.semanticContext,
            g_fixture.compiler.preSemanticIr.callableTypeId, &g_fixture.module.layouts[0],
            &g_target, &diagnostic);
    memcpy(&observed, &g_target, sizeof(observed));
    memset(&g_target, 0, sizeof(g_target));
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_UNSUPPORTED, diagnostic.status);
    TEST_ASSERT_EQUAL_MEMORY(&before, &observed, sizeof(before));
    TEST_ASSERT_EQUAL_UINT64(inputs, inputs_observation());
}
#endif

int main(int argc, char **argv) {
    TZrBool prerequisitesOnly = ZR_FALSE, featuresOnly = ZR_FALSE;
    if (argc == 2 && strcmp(argv[1], "--prerequisites-only") == 0) prerequisitesOnly = ZR_TRUE;
    else if (argc == 2 && strcmp(argv[1], "--features-only") == 0) featuresOnly = ZR_TRUE;
    else if (argc != 1) return 2;
    UNITY_BEGIN();
#if defined(_WIN32) && defined(_WIN64) && defined(_MSC_VER) && \
        (defined(_M_X64) || defined(_M_AMD64)) && !defined(_M_ARM64) && !defined(_M_ARM64EC)
    if (!featuresOnly) {
        RUN_TEST(test_prerequisite_real_nine_descriptor);
        RUN_TEST(test_prerequisite_real_eight_descriptor);
    }
    if (!prerequisitesOnly) {
        RUN_TEST(test_real_nine_c_text);
        RUN_TEST(test_real_eight_c_text);
        RUN_TEST(test_real_nine_llvm_text);
        RUN_TEST(test_real_eight_llvm_text);
        if (!featuresOnly) {
            RUN_TEST(test_c_null_diagnostic);
            RUN_TEST(test_llvm_null_diagnostic);
            RUN_TEST(test_c_exact_capacity_boundary);
            RUN_TEST(test_llvm_exact_capacity_boundary);
            RUN_TEST(test_c_required_outputs);
            RUN_TEST(test_llvm_required_outputs);
            RUN_TEST(test_c_extra_actual_layout_is_unsupported);
            RUN_TEST(test_llvm_extra_actual_layout_is_unsupported);
            RUN_TEST(test_c_nop_semantic_type_is_unsupported);
            RUN_TEST(test_llvm_nop_semantic_type_is_unsupported);
            RUN_TEST(test_c_nop_core_error_propagates);
            RUN_TEST(test_llvm_nop_core_error_propagates);
            RUN_TEST(test_c_actual_frame_type_mismatch);
            RUN_TEST(test_llvm_actual_frame_type_mismatch);
            RUN_TEST(test_c_actual_layout_width_mismatch);
            RUN_TEST(test_llvm_actual_layout_width_mismatch);
        }
    }
#else
    (void)prerequisitesOnly;
    (void)featuresOnly;
    RUN_TEST(test_other_host_actual_target_is_unsupported);
#endif
    return UNITY_END();
}
