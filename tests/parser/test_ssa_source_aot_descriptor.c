#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_parser/aot_ir_projection_descriptor.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/exec_ir_dead_source_places.h"
#include "zr_vm_parser/exec_ir_host_aot_target.h"
#include "zr_vm_parser/exec_ir_host_primitive_layout.h"
#include "zr_vm_parser/exec_ir_source_frame.h"
#include "zr_vm_parser/exec_ir_source_module_contract.h"
#include "support/ssa_literal_script_fixture.h"

static SZrState *g_state;
static SZrSsaLiteralScriptFixture g_fixture;
static SZrExecIrFunction g_compacted;
static SZrAotIrProjection g_projection;
static SZrAotIrProjectionDescriptor g_descriptor;
static SZrAotIrTargetContract g_target;

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

/* These are observations of live owned objects, not contract hash producers. */
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

static TZrUInt64 module_observation(void) {
    TZrUInt64 hash = bytes(0u, &g_fixture.module, sizeof(g_fixture.module));
    TZrUInt64 graph = ZrTests_SsaLiteralScriptFixture_FunctionDigest(
            ZrTests_SsaLiteralScriptFixture_Function(&g_fixture));
    hash = bytes(hash, &graph, sizeof(graph));
    hash = bytes(hash, g_fixture.module.constants,
            (size_t)g_fixture.module.constantCount * sizeof(*g_fixture.module.constants));
    return bytes(hash, g_fixture.module.layouts,
            (size_t)g_fixture.module.layoutCount * sizeof(*g_fixture.module.layouts));
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

static SZrMetadataTokenRecord *module_row(void) {
    SZrFunction *source = g_fixture.compiler.currentFunction;
    SZrMetadataTokenRecord *found = ZR_NULL;
    TZrUInt32 index;
    for (index = 0u; index < source->metadataTokenRecordLength; ++index) {
        SZrMetadataTokenRecord *row = &source->metadataTokenRecords[index];
        if (ZR_METADATA_TOKEN_TABLE(row->token) == ZR_METADATA_TABLE_MODULE) {
            TEST_ASSERT_NULL(found);
            found = row;
        }
    }
    TEST_ASSERT_NOT_NULL(found);
    return found;
}

static SZrMetadataTokenRecord *paired_row(const SZrMetadataTokenRecord *row) {
    SZrFunction *source = g_fixture.compiler.currentFunction;
    TZrUInt32 index;
    for (index = 0u; index < source->metadataTokenRecordLength; ++index)
        if (source->metadataTokenRecords[index].token == row->relatedToken)
            return &source->metadataTokenRecords[index];
    TEST_FAIL_MESSAGE("PRECONDITION: actual paired MODULE/SIGNATURE row");
    return ZR_NULL;
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

static void prepare_source(EZrSsaLiteralScriptSource source) {
    ZrTests_SsaLiteralScriptFixture_Prepare(&g_fixture, source);
    TEST_ASSERT_NULL(g_fixture.compiler.currentFunction->moduleVersion);
    TEST_ASSERT_EQUAL_UINT32(0u, g_fixture.module.contract.schemaVersion);
    TEST_ASSERT_EQUAL_UINT32(0u, g_fixture.module.contract.abiVersion);
    TEST_ASSERT_EQUAL_UINT32(0u, g_fixture.module.contract.logicalVersion);
    TEST_ASSERT_EQUAL_UINT64(0u,
            ZrTests_SsaLiteralScriptFixture_Function(&g_fixture)->contract.moduleHash);
}

static void project_source(void) {
    const SZrExecIrFunction *original = ZrTests_SsaLiteralScriptFixture_Function(&g_fixture);
    SZrExecIrDiagnostic diagnostic = {0};
    SZrExecIrLayout row;
    SZrExecIrRange range;
    TZrTypeId type = return_type();
    TZrUInt64 sourceBefore = source_observation();
    TZrUInt32 index;
    TEST_ASSERT_TRUE_MESSAGE(ZrCore_ExecIr_VerifyFunction(original,
            ZR_EXEC_IR_VERIFY_ALL, &diagnostic), "PRECONDITION: actual original graph");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &g_fixture.compiler.preSemanticIr, g_fixture.compiler.semanticContext,
            original, &g_compacted, &diagnostic), "PRECONDITION: actual dead-place compaction");
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_observation());
    TEST_ASSERT_EQUAL_UINT32(original->id, g_compacted.id);
    TEST_ASSERT_EQUAL_UINT32(original->functionToken, g_compacted.functionToken);
    TEST_ASSERT_EQUAL_UINT64(original->signatureHash, g_compacted.signatureHash);
    TEST_ASSERT_EQUAL_UINT64(original->contract.moduleHash, g_compacted.contract.moduleHash);
    TEST_ASSERT_EQUAL_UINT32(3u, g_compacted.instructionCount);
    TEST_ASSERT_EQUAL_UINT32(1u, g_compacted.valueCount);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_CONSTANT, g_compacted.instructions[0].opcode);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_NOP, g_compacted.instructions[1].opcode);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_RETURN, g_compacted.instructions[2].opcode);
    for (index = 0u; index < g_compacted.instructionCount; ++index) {
        TEST_ASSERT_EQUAL_UINT32(original->instructions[index].sourceId,
                g_compacted.instructions[index].sourceId);
    }
    ZrTests_SsaLiteralScriptFixture_AssertSourceMaps(&g_fixture, &g_compacted);
    TEST_ASSERT_EQUAL_UINT32(type, g_compacted.values[0].typeToken);
    TEST_ASSERT_EQUAL_UINT32(1u, g_compacted.values[0].id);
    TEST_ASSERT_EQUAL_UINT32(original->values[0].definitionInstructionId,
            g_compacted.values[0].definitionInstructionId);
    TEST_ASSERT_EQUAL_UINT32(original->entryBlockId, g_compacted.entryBlockId);
    TEST_ASSERT_EQUAL_UINT32(original->blocks[0].terminatorInstructionId,
            g_compacted.blocks[0].terminatorInstructionId);
    TEST_ASSERT_EQUAL_UINT32(0u, g_fixture.module.layoutCount);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_MakeHostPrimitiveLayout(
            g_fixture.compiler.semanticContext, type, 1u, &row, &diagnostic),
            "PRECONDITION: real host layout row");
    TEST_ASSERT_EQUAL_UINT32(type, row.typeToken);
    TEST_ASSERT_EQUAL_UINT32(sizeof(TZrInt64), row.byteSize);
    TEST_ASSERT_EQUAL_UINT32(alignof(TZrInt64), row.byteAlign);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, row.layoutHash);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ModuleAppendLayout(&g_fixture.module, &row, 1u, &range));
    TEST_ASSERT_EQUAL_UINT32(0u, range.offset);
    TEST_ASSERT_EQUAL_UINT32(1u, range.count);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_AttachPrimitiveSourceFrame(&g_compacted,
            g_fixture.compiler.semanticContext, g_fixture.module.layouts,
            g_fixture.module.layoutCount, 0u, &diagnostic), "PRECONDITION: owned primitive frame");
    TEST_ASSERT_NOT_NULL(g_compacted.frameLayout);
    TEST_ASSERT_EQUAL_UINT32(1u, g_compacted.frameLayout->slotCount);
    TEST_ASSERT_EQUAL_UINT32(g_compacted.values[0].id, g_compacted.frameLayout->slots[0].slotId);
    TEST_ASSERT_EQUAL_UINT32(type, g_compacted.frameLayout->slots[0].typeToken);
    TEST_ASSERT_EQUAL_UINT32(row.byteSize, g_compacted.frameLayout->slots[0].byteSize);
    TEST_ASSERT_EQUAL_UINT32(row.byteAlign, g_compacted.frameLayout->slots[0].byteAlign);
    TEST_ASSERT_EQUAL_UINT64(g_compacted.frameLayout->layoutHash, g_compacted.contract.layoutHash);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(&g_compacted, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_LowerAotWithCanonicalCallable(&g_compacted,
            g_fixture.module.constants, g_fixture.module.constantCount,
            g_fixture.module.layouts, g_fixture.module.layoutCount,
            g_fixture.compiler.semanticContext, g_fixture.compiler.preSemanticIr.callableTypeId,
            &g_projection, &diagnostic), "PRECONDITION: real canonical projection");
    TEST_ASSERT_EQUAL_UINT32(g_compacted.functionToken, g_projection.functionToken);
    TEST_ASSERT_EQUAL_UINT64(g_compacted.signatureHash, g_projection.signatureHash);
    TEST_ASSERT_EQUAL_UINT64(g_compacted.contract.moduleHash, g_projection.contract.moduleHash);
    TEST_ASSERT_EQUAL_UINT32(3u, g_projection.instructionCount);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_OPCODE_CONSTANT, g_projection.opcodes[0]);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_OPCODE_NOP, g_projection.opcodes[1]);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_OPCODE_RETURN, g_projection.opcodes[2]);
    TEST_ASSERT_EQUAL_UINT32(1u, g_projection.frameSlotCount);
    TEST_ASSERT_EQUAL_MEMORY(g_compacted.frameLayout->slots, g_projection.frameSlots,
            sizeof(*g_projection.frameSlots));
    TEST_ASSERT_EQUAL_UINT64(g_compacted.frameLayout->layoutHash, g_projection.frameLayoutHash);
    TEST_ASSERT_EQUAL_UINT32(1u, g_projection.valueSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, g_projection.physicalSlotCount);
    TEST_ASSERT_EQUAL_UINT32(0u, g_projection.valueSlots[0]);
    TEST_ASSERT_EQUAL_MEMORY(g_compacted.values, g_projection.slotValues, sizeof(*g_compacted.values));
    TEST_ASSERT_EQUAL_UINT32(g_fixture.module.constantCount, g_projection.constantCount);
    TEST_ASSERT_EQUAL_MEMORY(g_fixture.module.constants, g_projection.constants,
            (size_t)g_projection.constantCount * sizeof(*g_projection.constants));
    TEST_ASSERT_EQUAL_INT64(g_fixture.expectedReturn, (TZrInt64)g_projection.constants[0].bits);
    TEST_ASSERT_EQUAL_UINT32(1u, g_projection.layoutCount);
    TEST_ASSERT_EQUAL_MEMORY(g_fixture.module.layouts, g_projection.layouts, sizeof(row));
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
    TEST_ASSERT_EQUAL_UINT32(original->stateMap->functionToken, g_compacted.stateMap->functionToken);
    TEST_ASSERT_EQUAL_UINT64(original->stateMap->signatureHash, g_compacted.stateMap->signatureHash);
    TEST_ASSERT_EQUAL_UINT64(original->stateMap->generation, g_compacted.stateMap->generation);
    TEST_ASSERT_EQUAL_UINT32(g_compacted.stateMap->functionToken, g_projection.stateMap.functionToken);
    TEST_ASSERT_EQUAL_UINT64(g_compacted.stateMap->signatureHash, g_projection.stateMap.signatureHash);
    TEST_ASSERT_EQUAL_UINT64(g_compacted.stateMap->generation, g_projection.stateMap.generation);
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

static void make_target(void) {
    SZrAotIrDiagnostic diagnostic = {0};
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_MakeHostNoArgsI64AotTarget(
            g_fixture.compiler.semanticContext, g_fixture.compiler.preSemanticIr.callableTypeId,
            &g_fixture.module.layouts[0], &g_target, &diagnostic),
            "PRECONDITION: already-GREEN actual host target");
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, diagnostic.status);
}

static void prerequisite(EZrSsaLiteralScriptSource source) {
    SZrAotIrDiagnostic diagnostic = {0};
    SZrAotIrProjectionDescriptor empty;
    TZrUInt64 sourceBefore, moduleBefore, projectionBefore;
    prepare_source(source);
    memset(&empty, 0, sizeof(empty));
    project_source();
    make_target();
    sourceBefore = source_observation();
    moduleBefore = module_observation();
    projectionBefore = projection_observation();
    TEST_ASSERT_FALSE(ZrParser_AotIrProjection_BuildDescriptor(&g_projection, &g_target,
            &g_fixture.module.contract, &g_descriptor, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_VERSION_MISMATCH, diagnostic.status);
    TEST_ASSERT_EQUAL_UINT64(ZR_EXECUTION_CONTRACT_SCHEMA_VERSION, diagnostic.expected);
    TEST_ASSERT_EQUAL_UINT64(0u, diagnostic.actual);
    TEST_ASSERT_EQUAL_MEMORY(&empty, &g_descriptor, sizeof(empty));
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_observation());
    TEST_ASSERT_EQUAL_UINT64(moduleBefore, module_observation());
    TEST_ASSERT_EQUAL_UINT64(projectionBefore, projection_observation());
}

static void bind_source(void) {
    SZrExecIrFunction *function = ZrTests_SsaLiteralScriptFixture_Function(&g_fixture);
    SZrExecIrModule expectedModule;
    SZrExecIrFunction expectedFunction;
    SZrExecutionContract expectedContract = {0};
    SZrExecutionContract oldContract;
    SZrExecIrDiagnostic diagnostic = {0};
    TZrUInt64 sourceBefore = source_observation();
    TZrUInt64 graphBefore = ZrTests_SsaLiteralScriptFixture_FunctionDigest(function);
    TZrUInt64 moduleBefore = module_observation();
    TZrUInt64 functionHashBefore = function->contract.moduleHash;
    SZrMetadataTokenRecord *row = module_row();
    expectedContract.schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION;
    expectedContract.abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION;
    expectedContract.logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION;
    expectedContract.targetToken = row->token;
    expectedContract.moduleHash = g_fixture.compiler.currentFunction->moduleSignatureHash;
    expectedContract.signatureHash = row->signatureHash;
    expectedContract.generation = function->contract.generation;
    memcpy(&expectedModule, &g_fixture.module, sizeof(expectedModule));
    memcpy(&expectedFunction, function, sizeof(expectedFunction));
    expectedModule.contract = expectedContract;
    expectedFunction.contract.moduleHash = expectedContract.moduleHash;
    memcpy(&oldContract, &g_fixture.module.contract, sizeof(oldContract));
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_BindSourceModuleContract(&g_fixture.compiler,
            &g_fixture.module, &diagnostic), "FEATURE RED: source module binder must publish real contract");
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_EQUAL_MEMORY(&expectedModule, &g_fixture.module, sizeof(expectedModule));
    TEST_ASSERT_EQUAL_MEMORY(&expectedFunction, function, sizeof(expectedFunction));
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_observation());
    /* Exclude only the two authorized published fields from deep observations. */
    g_fixture.module.contract = oldContract;
    function->contract.moduleHash = functionHashBefore;
    {
        TZrUInt64 observedGraph = ZrTests_SsaLiteralScriptFixture_FunctionDigest(function);
        TZrUInt64 observedModule = module_observation();
        g_fixture.module.contract = expectedContract;
        function->contract.moduleHash = expectedContract.moduleHash;
        TEST_ASSERT_EQUAL_UINT64(graphBefore, observedGraph);
        TEST_ASSERT_EQUAL_UINT64(moduleBefore, observedModule);
    }
    TEST_ASSERT_EQUAL_UINT32(g_fixture.module.moduleToken, expectedContract.targetToken);
    TEST_ASSERT_EQUAL_UINT64(g_fixture.module.moduleHash, expectedContract.moduleHash);
    TEST_ASSERT_EQUAL_UINT64(1u, expectedContract.generation);
    TEST_ASSERT_EQUAL_UINT64(0u, expectedContract.layoutHash);
    TEST_ASSERT_NOT_EQUAL_UINT64(expectedContract.signatureHash, function->signatureHash);
    TEST_ASSERT_NOT_EQUAL_UINT64(expectedContract.moduleHash, function->signatureHash);
    TEST_ASSERT_NOT_EQUAL_UINT64(expectedContract.moduleHash, expectedContract.signatureHash);
}

static void descriptor_success(void) {
    SZrAotIrDiagnostic diagnostic = {0};
    SZrAotIrCallableAbi abi = {0};
    TZrUInt64 sourceBefore = source_observation();
    TZrUInt64 moduleBefore = module_observation();
    TZrUInt64 projectionBefore = projection_observation();
    TEST_ASSERT_TRUE(ZrParser_AotIrProjection_BuildDescriptor(&g_projection, &g_target,
            &g_fixture.module.contract, &g_descriptor, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, diagnostic.status);
    TEST_ASSERT_EQUAL_PTR(&g_projection, g_descriptor.owner);
    TEST_ASSERT_EQUAL_PTR(&g_descriptor.function, g_descriptor.module.functions);
    TEST_ASSERT_EQUAL_MEMORY(&g_fixture.module.contract, &g_descriptor.module.contract,
            sizeof(g_fixture.module.contract));
    TEST_ASSERT_EQUAL_UINT64(g_fixture.module.moduleHash, g_descriptor.module.moduleHash);
    TEST_ASSERT_EQUAL_UINT32(1u, g_descriptor.module.functionCount);
    TEST_ASSERT_EQUAL_UINT32(g_projection.functionId, g_descriptor.function.id);
    TEST_ASSERT_EQUAL_UINT32(3u, g_descriptor.function.instructionCount);
    TEST_ASSERT_EQUAL_UINT32(g_projection.functionToken, g_descriptor.function.functionToken);
    TEST_ASSERT_EQUAL_UINT64(g_projection.signatureHash, g_descriptor.function.signatureHash);
    TEST_ASSERT_EQUAL_MEMORY(&g_projection.contract, &g_descriptor.function.contract,
            sizeof(g_projection.contract));
    TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_OPCODE_CONSTANT, g_descriptor.function.instructions[0].opcode);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_OPCODE_NOP, g_descriptor.function.instructions[1].opcode);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_OPCODE_RETURN, g_descriptor.function.instructions[2].opcode);
    TEST_ASSERT_EQUAL_UINT32(g_projection.constantCount, g_descriptor.module.constantCount);
    TEST_ASSERT_EQUAL_MEMORY(g_projection.constants, g_descriptor.module.constantPool,
            (size_t)g_projection.constantCount * sizeof(*g_projection.constants));
    TEST_ASSERT_EQUAL_INT64(g_fixture.expectedReturn, (TZrInt64)g_descriptor.module.constantPool[0].bits);
    TEST_ASSERT_EQUAL_UINT32(g_projection.layoutCount, g_descriptor.module.layoutCount);
    TEST_ASSERT_EQUAL_MEMORY(g_projection.layouts, g_descriptor.module.layoutPool,
            (size_t)g_projection.layoutCount * sizeof(*g_projection.layouts));
    TEST_ASSERT_EQUAL_UINT32(g_projection.frameSlotCount, g_descriptor.function.frameSlotCount);
    TEST_ASSERT_EQUAL_UINT32(g_projection.valueSlotCount, g_descriptor.function.valueSlotCount);
    TEST_ASSERT_EQUAL_MEMORY(g_projection.valueSlots, g_descriptor.function.valueSlotPool,
            (size_t)g_projection.valueSlotCount * sizeof(*g_projection.valueSlots));
    TEST_ASSERT_EQUAL_UINT32(g_projection.frameSlots[0].slotId, g_descriptor.function.frameSlots[0].slotId);
    TEST_ASSERT_EQUAL_UINT32(g_projection.frameSlots[0].typeToken, g_descriptor.function.frameSlots[0].typeToken);
    TEST_ASSERT_EQUAL_UINT32(g_projection.frameSlots[0].byteSize, g_descriptor.function.frameSlots[0].byteSize);
    TEST_ASSERT_EQUAL_UINT32(g_projection.frameSlots[0].byteAlign, g_descriptor.function.frameSlots[0].byteAlign);
    TEST_ASSERT_EQUAL_UINT64(g_projection.frameLayoutHash, g_descriptor.function.frameLayout.layoutHash);
    TEST_ASSERT_EQUAL_UINT32(g_projection.sourceMapCount, g_descriptor.function.sourceMapCount);
    {
        TZrUInt32 index;
        for (index = 0u; index < g_projection.sourceMapCount; ++index) {
            const SZrExecIrProjectionSourceMap *map = &g_projection.sourceMaps[index];
            const SZrAotIrSourceMap *out = &g_descriptor.function.sourceMaps[index];
            TEST_ASSERT_EQUAL_UINT32(map->pc + 1u, out->instructionId);
            TEST_ASSERT_EQUAL_UINT32(map->sourceId, out->sourceId);
            TEST_ASSERT_EQUAL_UINT32(map->startOffset, out->startOffset);
            TEST_ASSERT_EQUAL_UINT32(map->endOffset, out->endOffset);
            TEST_ASSERT_EQUAL_UINT32(map->startLine, out->startLine);
            TEST_ASSERT_EQUAL_UINT32(map->startColumn, out->startColumn);
            TEST_ASSERT_EQUAL_UINT32(map->endLine, out->endLine);
            TEST_ASSERT_EQUAL_UINT32(map->endColumn, out->endColumn);
        }
    }
    TEST_ASSERT_EQUAL_PTR(&g_projection.stateMap, g_descriptor.function.logicalStateMap);
    TEST_ASSERT_EQUAL_MEMORY(&g_projection.stateMap, g_descriptor.function.logicalStateMap,
            sizeof(g_projection.stateMap));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_ValidateModule(&g_descriptor.module, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_RequireExecutableAbi(&g_descriptor.module,
            g_descriptor.function.id, &abi, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64, abi.kind);
    TEST_ASSERT_EQUAL_UINT32(return_type(), abi.returnTypeToken);
    TEST_ASSERT_FALSE(g_projection.runnable);
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_observation());
    TEST_ASSERT_EQUAL_UINT64(moduleBefore, module_observation());
    TEST_ASSERT_EQUAL_UINT64(projectionBefore, projection_observation());
}

static void feature(EZrSsaLiteralScriptSource source) {
    prepare_source(source);
    bind_source();
    project_source();
    make_target();
    descriptor_success();
}

static void test_prerequisite_real_nine_unbound_descriptor(void) { prerequisite(ZR_TEST_SSA_LITERAL_SCRIPT_NINE); }
static void test_prerequisite_real_eight_unbound_descriptor(void) { prerequisite(ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT); }
static void test_real_nine_source_descriptor(void) { feature(ZR_TEST_SSA_LITERAL_SCRIPT_NINE); }
static void test_real_eight_source_descriptor(void) { feature(ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT); }

static void test_repeat_binding_and_failed_owned_descriptor_replacement(void) {
    SZrExecIrDiagnostic diagnostic = {0};
    SZrAotIrDiagnostic aotDiagnostic = {0};
    SZrAotIrProjectionDescriptor before;
    SZrExecutionContract contradiction;
    TZrUInt64 sourceBefore, moduleBefore, projectionBefore, descriptorHash;
    feature(ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    sourceBefore = source_observation();
    moduleBefore = module_observation();
    projectionBefore = projection_observation();
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BindSourceModuleContract(&g_fixture.compiler,
            &g_fixture.module, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_BindSourceModuleContract(&g_fixture.compiler,
            &g_fixture.module, ZR_NULL));
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_observation());
    TEST_ASSERT_EQUAL_UINT64(moduleBefore, module_observation());
    TEST_ASSERT_EQUAL_UINT64(projectionBefore, projection_observation());
    memcpy(&before, &g_descriptor, sizeof(before));
    descriptorHash = ZrCore_AotIr_HashModule(&g_descriptor.module);
    contradiction = g_fixture.module.contract;
    contradiction.moduleHash ^= 1u;
    TEST_ASSERT_FALSE(ZrParser_AotIrProjection_BuildDescriptor(&g_projection, &g_target,
            &contradiction, &g_descriptor, &aotDiagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_CONTRACT, aotDiagnostic.status);
    TEST_ASSERT_EQUAL_MEMORY(&before, &g_descriptor, sizeof(before));
    TEST_ASSERT_EQUAL_UINT64(descriptorHash, ZrCore_AotIr_HashModule(&g_descriptor.module));
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_observation());
    TEST_ASSERT_EQUAL_UINT64(moduleBefore, module_observation());
    TEST_ASSERT_EQUAL_UINT64(projectionBefore, projection_observation());
}

static void assert_refusal(TZrBool result, const SZrExecIrDiagnostic *diagnostic,
        EZrExecutionDiagnosticCode code, TZrUInt64 sourceBefore, TZrUInt64 moduleBefore) {
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_INT(code, diagnostic->code);
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_observation());
    TEST_ASSERT_EQUAL_UINT64(moduleBefore, module_observation());
}

static void test_null_required_arguments(void) {
    SZrExecIrDiagnostic diagnostic = {0};
    TZrUInt64 sourceBefore, moduleBefore;
    TZrBool result;
    prepare_source(ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    sourceBefore = source_observation();
    moduleBefore = module_observation();
    result = ZrParser_ExecIr_BindSourceModuleContract(ZR_NULL, &g_fixture.module, &diagnostic);
    assert_refusal(result, &diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, sourceBefore, moduleBefore);
    result = ZrParser_ExecIr_BindSourceModuleContract(&g_fixture.compiler, ZR_NULL, &diagnostic);
    assert_refusal(result, &diagnostic, ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, sourceBefore, moduleBefore);
}

typedef enum EContradiction {
    MODULE_TOKEN, MODULE_HASH, ENTRY_TOKEN, ENTRY_CANONICAL_SIGNATURE,
    MODULE_RELATED_TOKEN, SIGNATURE_RELATED_TOKEN, PAIRED_SIGNATURE_HASH,
    MODULE_BLOB_RANGE, MODULE_LAYOUT, SCHEMA_VERSION, ABI_VERSION,
    LOGICAL_VERSION, GENERATION, FUNCTION_MODULE_HASH
} EContradiction;

/* Save all bounded real fields, record mutation while perturbed, then restore
 * before any assertion or digest. No altered header is fed to teardown. */
static void contradict(EContradiction kind, EZrExecutionDiagnosticCode code) {
    SZrExecIrModule savedModule, perturbedModule, observedModule;
    SZrExecIrFunction savedFunction, perturbedFunction, observedFunction;
    SZrFunctionSourceCallableIdentity savedIdentity, perturbedIdentity, observedIdentity;
    SZrMetadataTokenRecord savedRow, perturbedRow, observedRow;
    SZrMetadataTokenRecord savedPair, perturbedPair, observedPair;
    SZrExecIrFunction *function;
    SZrMetadataTokenRecord *row, *pair;
    SZrExecIrDiagnostic diagnostic = {0};
    TZrUInt64 sourceBefore, moduleBefore;
    TZrBool result;
    prepare_source(ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    function = ZrTests_SsaLiteralScriptFixture_Function(&g_fixture);
    row = module_row();
    pair = paired_row(row);
    TEST_ASSERT_NOT_EQUAL_UINT32(row->token, pair->token);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, row->signatureHash);
    TEST_ASSERT_TRUE(row->signatureBlobOffset <= g_fixture.compiler.currentFunction->signatureBlobHeapLength);
    TEST_ASSERT_TRUE(g_fixture.compiler.currentFunction->signatureBlobHeapLength < UINT32_MAX);
    sourceBefore = source_observation();
    moduleBefore = module_observation();
    memcpy(&savedModule, &g_fixture.module, sizeof(savedModule));
    memcpy(&savedFunction, function, sizeof(savedFunction));
    memcpy(&savedIdentity, &g_fixture.compiler.currentFunction->sourceCallableIdentity, sizeof(savedIdentity));
    memcpy(&savedRow, row, sizeof(savedRow));
    memcpy(&savedPair, pair, sizeof(savedPair));
    switch (kind) {
        case MODULE_TOKEN: g_fixture.module.moduleToken ^= 1u; break;
        case MODULE_HASH: g_fixture.module.moduleHash ^= 1u; break;
        case ENTRY_TOKEN: function->functionToken ^= 1u; break;
        case ENTRY_CANONICAL_SIGNATURE:
            g_fixture.compiler.currentFunction->sourceCallableIdentity.canonicalSignatureHash ^= 1u; break;
        case MODULE_RELATED_TOKEN: row->relatedToken = row->token; break;
        case SIGNATURE_RELATED_TOKEN: pair->relatedToken = pair->token; break;
        case PAIRED_SIGNATURE_HASH: pair->signatureHash ^= 1u; break;
        case MODULE_BLOB_RANGE:
            row->signatureBlobLength = g_fixture.compiler.currentFunction->signatureBlobHeapLength
                    - row->signatureBlobOffset + 1u; break;
        case MODULE_LAYOUT: g_fixture.module.contract.layoutHash = row->signatureHash; break;
        case SCHEMA_VERSION: g_fixture.module.contract.schemaVersion = ZR_EXECUTION_CONTRACT_SCHEMA_VERSION + 1u; break;
        case ABI_VERSION: g_fixture.module.contract.abiVersion = ZR_EXECUTION_CONTRACT_ABI_VERSION + 1u; break;
        case LOGICAL_VERSION: g_fixture.module.contract.logicalVersion = ZR_EXECUTION_CONTRACT_LOGICAL_VERSION + 1u; break;
        case GENERATION: g_fixture.module.contract.generation = function->contract.generation + 1u; break;
        case FUNCTION_MODULE_HASH: function->contract.moduleHash = g_fixture.module.moduleHash ^ 1u; break;
    }
    memcpy(&perturbedModule, &g_fixture.module, sizeof(perturbedModule));
    memcpy(&perturbedFunction, function, sizeof(perturbedFunction));
    memcpy(&perturbedIdentity, &g_fixture.compiler.currentFunction->sourceCallableIdentity, sizeof(perturbedIdentity));
    memcpy(&perturbedRow, row, sizeof(perturbedRow));
    memcpy(&perturbedPair, pair, sizeof(perturbedPair));
    result = ZrParser_ExecIr_BindSourceModuleContract(&g_fixture.compiler, &g_fixture.module, &diagnostic);
    memcpy(&observedModule, &g_fixture.module, sizeof(observedModule));
    memcpy(&observedFunction, function, sizeof(observedFunction));
    memcpy(&observedIdentity, &g_fixture.compiler.currentFunction->sourceCallableIdentity, sizeof(observedIdentity));
    memcpy(&observedRow, row, sizeof(observedRow));
    memcpy(&observedPair, pair, sizeof(observedPair));
    memcpy(&g_fixture.module, &savedModule, sizeof(savedModule));
    memcpy(function, &savedFunction, sizeof(savedFunction));
    memcpy(&g_fixture.compiler.currentFunction->sourceCallableIdentity, &savedIdentity, sizeof(savedIdentity));
    memcpy(row, &savedRow, sizeof(savedRow));
    memcpy(pair, &savedPair, sizeof(savedPair));
    TEST_ASSERT_EQUAL_MEMORY(&perturbedModule, &observedModule, sizeof(observedModule));
    TEST_ASSERT_EQUAL_MEMORY(&perturbedFunction, &observedFunction, sizeof(observedFunction));
    TEST_ASSERT_EQUAL_MEMORY(&perturbedIdentity, &observedIdentity, sizeof(observedIdentity));
    TEST_ASSERT_EQUAL_MEMORY(&perturbedRow, &observedRow, sizeof(observedRow));
    TEST_ASSERT_EQUAL_MEMORY(&perturbedPair, &observedPair, sizeof(observedPair));
    assert_refusal(result, &diagnostic, code, sourceBefore, moduleBefore);
}

static void test_module_token_contradiction(void) { contradict(MODULE_TOKEN, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH); }
static void test_module_hash_contradiction(void) { contradict(MODULE_HASH, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH); }
static void test_entry_target_contradiction(void) { contradict(ENTRY_TOKEN, ZR_EXECUTION_DIAGNOSTIC_TARGET_MISMATCH); }
static void test_entry_canonical_signature_contradiction(void) { contradict(ENTRY_CANONICAL_SIGNATURE, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH); }
static void test_module_related_token_contradiction(void) { contradict(MODULE_RELATED_TOKEN, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH); }
static void test_signature_related_token_contradiction(void) { contradict(SIGNATURE_RELATED_TOKEN, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH); }
static void test_paired_signature_hash_contradiction(void) { contradict(PAIRED_SIGNATURE_HASH, ZR_EXECUTION_DIAGNOSTIC_SIGNATURE_MISMATCH); }
static void test_actual_module_blob_exceeds_heap(void) { contradict(MODULE_BLOB_RANGE, ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE); }
static void test_existing_module_layout_refused(void) { contradict(MODULE_LAYOUT, ZR_EXECUTION_DIAGNOSTIC_LAYOUT_MISMATCH); }
static void test_existing_schema_version_refused(void) { contradict(SCHEMA_VERSION, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH); }
static void test_existing_abi_version_refused(void) { contradict(ABI_VERSION, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH); }
static void test_existing_logical_version_refused(void) { contradict(LOGICAL_VERSION, ZR_EXECUTION_DIAGNOSTIC_VERSION_MISMATCH); }
static void test_existing_generation_refused(void) { contradict(GENERATION, ZR_EXECUTION_DIAGNOSTIC_STALE_GENERATION); }
static void test_original_function_module_hash_refused(void) { contradict(FUNCTION_MODULE_HASH, ZR_EXECUTION_DIAGNOSTIC_MODULE_MISMATCH); }

#else
static void test_other_host_actual_target_is_unsupported(void) {
    SZrAotIrDiagnostic diagnostic = {0};
    SZrAotIrTargetContract before;
    TZrUInt64 sourceBefore, moduleBefore;
    TZrBool result;
    prepare_source(ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    project_source();
    sourceBefore = source_observation();
    moduleBefore = module_observation();
    memset(&g_target, 0xa5, sizeof(g_target));
    memcpy(&before, &g_target, sizeof(before));
    result = ZrParser_ExecIr_MakeHostNoArgsI64AotTarget(g_fixture.compiler.semanticContext,
            g_fixture.compiler.preSemanticIr.callableTypeId, &g_fixture.module.layouts[0],
            &g_target, &diagnostic);
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_UNSUPPORTED, diagnostic.status);
    TEST_ASSERT_EQUAL_MEMORY(&before, &g_target, sizeof(before));
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_observation());
    TEST_ASSERT_EQUAL_UINT64(moduleBefore, module_observation());
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
        RUN_TEST(test_prerequisite_real_nine_unbound_descriptor);
        RUN_TEST(test_prerequisite_real_eight_unbound_descriptor);
    }
    if (!prerequisitesOnly) {
        RUN_TEST(test_real_nine_source_descriptor);
        RUN_TEST(test_real_eight_source_descriptor);
        if (!featuresOnly) {
            RUN_TEST(test_repeat_binding_and_failed_owned_descriptor_replacement);
            RUN_TEST(test_null_required_arguments);
            RUN_TEST(test_module_token_contradiction);
            RUN_TEST(test_module_hash_contradiction);
            RUN_TEST(test_entry_target_contradiction);
            RUN_TEST(test_entry_canonical_signature_contradiction);
            RUN_TEST(test_module_related_token_contradiction);
            RUN_TEST(test_signature_related_token_contradiction);
            RUN_TEST(test_paired_signature_hash_contradiction);
            RUN_TEST(test_actual_module_blob_exceeds_heap);
            RUN_TEST(test_existing_module_layout_refused);
            RUN_TEST(test_existing_schema_version_refused);
            RUN_TEST(test_existing_abi_version_refused);
            RUN_TEST(test_existing_logical_version_refused);
            RUN_TEST(test_existing_generation_refused);
            RUN_TEST(test_original_function_module_hash_refused);
        }
    }
#else
    (void)prerequisitesOnly;
    (void)featuresOnly;
    RUN_TEST(test_other_host_actual_target_is_unsupported);
#endif
    return UNITY_END();
}
