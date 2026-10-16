#include <stdint.h>
#include <string.h>

#include "unity.h"
#include "harness/runtime_support.h"
#include "backend_aot/backend_aot_ir_scalar_text.h"
#include "aot/aot_ir_owned_module.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "exec_ir/aot_ir_projection_descriptor.h"
#include "zr_vm_parser/canonical_type.h"
#include "exec_ir/exec_ir_dead_source_places.h"
#include "exec_ir/exec_ir_host_aot_target.h"
#include "exec_ir/exec_ir_host_primitive_layout.h"
#include "exec_ir/source/exec_ir_source_frame.h"
#include "exec_ir/source/exec_ir_source_module_contract.h"
#include "support/ssa_literal_script_fixture.h"

/* Every resource is registered before the first Unity assertion. */
static SZrState *g_state;
static SZrSsaLiteralScriptFixture g_fixture;
static SZrExecIrFunction g_compacted;
static SZrAotIrProjection g_projection;
static SZrAotIrProjectionDescriptor g_descriptor;
static SZrAotIrTargetContract g_target;
static SZrAotIrOwnedModule *g_owner;
static SZrAotIrOwnedModule *g_second;

typedef EZrAotIrStatus (*FEmitter)(const SZrAotIrModule *, TZrUInt32,
        char *, size_t, size_t *, SZrAotIrDiagnostic *);

/* Values only: no producer address survives in this lifetime witness. */
typedef struct SLifetimeWitness {
    TZrUInt32 functionId;
    TZrInt64 literal;
    TZrUInt64 semanticHash;
    TZrMetadataToken functionToken;
    TZrUInt64 signatureHash;
    TZrUInt64 generation;
    SZrAotIrTargetContract target;
    SZrExecutionContract moduleContract;
    SZrExecutionContract functionContract;
    SZrAotIrCallableAbi abi;
} SLifetimeWitness;

static void release_producers(void) {
    ZrParser_AotIrProjection_FreeDescriptor(&g_descriptor);
    memset(&g_descriptor, 0, sizeof(g_descriptor));
    ZrParser_AotIrProjection_Free(&g_projection);
    memset(&g_projection, 0, sizeof(g_projection));
    ZrCore_ExecIr_FreeFunction(&g_compacted);
    memset(&g_compacted, 0, sizeof(g_compacted));
    ZrTests_SsaLiteralScriptFixture_Free(&g_fixture);
    if (g_state != ZR_NULL) ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
    memset(&g_target, 0, sizeof(g_target));
}

void setUp(void) {
    g_owner = ZR_NULL;
    g_second = ZR_NULL;
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrTests_SsaLiteralScriptFixture_Init(&g_fixture, g_state);
    ZrCore_ExecIr_FunctionInit(&g_compacted);
    memset(&g_projection, 0, sizeof(g_projection));
    memset(&g_descriptor, 0, sizeof(g_descriptor));
    memset(&g_target, 0, sizeof(g_target));
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    release_producers();
    ZrCore_AotIr_OwnedModuleFree(&g_second);
    ZrCore_AotIr_OwnedModuleFree(&g_owner);
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
    TZrUInt32 i;
    ZrTests_SsaLiteralScriptFixture_Prepare(&g_fixture, source);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_BindSourceModuleContract(&g_fixture.compiler,
            &g_fixture.module, &diagnostic), "PRECONDITION: actual source binder GREEN");
    original = ZrTests_SsaLiteralScriptFixture_Function(&g_fixture);
    type = return_type();
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(original, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &g_fixture.compiler.preSemanticIr, g_fixture.compiler.semanticContext,
            original, &g_compacted, &diagnostic), "PRECONDITION: actual compaction");
    TEST_ASSERT_EQUAL_UINT32(original->id, g_compacted.id);
    TEST_ASSERT_EQUAL_UINT32(original->functionToken, g_compacted.functionToken);
    TEST_ASSERT_EQUAL_UINT64(original->signatureHash, g_compacted.signatureHash);
    TEST_ASSERT_EQUAL_UINT64(original->contract.moduleHash, g_compacted.contract.moduleHash);
    TEST_ASSERT_EQUAL_UINT32(3u, g_compacted.instructionCount);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_CONSTANT, g_compacted.instructions[0].opcode);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_NOP, g_compacted.instructions[1].opcode);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_RETURN, g_compacted.instructions[2].opcode);
    for (i = 0u; i < g_compacted.instructionCount; ++i)
        TEST_ASSERT_EQUAL_UINT32(original->instructions[i].sourceId, g_compacted.instructions[i].sourceId);
    ZrTests_SsaLiteralScriptFixture_AssertSourceMaps(&g_fixture, &g_compacted);
    TEST_ASSERT_EQUAL_UINT32(1u, g_compacted.valueCount);
    TEST_ASSERT_EQUAL_UINT32(type, g_compacted.values[0].typeToken);
    TEST_ASSERT_EQUAL_UINT32(0u, g_fixture.module.layoutCount);
    TEST_ASSERT_TRUE(ZrParser_ExecIr_MakeHostPrimitiveLayout(g_fixture.compiler.semanticContext,
            type, 1u, &row, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(sizeof(TZrInt64), row.byteSize);
    TEST_ASSERT_EQUAL_UINT32(alignof(TZrInt64), row.byteAlign);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, row.layoutHash);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_ModuleAppendLayout(&g_fixture.module, &row, 1u, &range));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_AttachPrimitiveSourceFrame(&g_compacted,
            g_fixture.compiler.semanticContext, g_fixture.module.layouts,
            g_fixture.module.layoutCount, 0u, &diagnostic));
    TEST_ASSERT_NOT_NULL(g_compacted.frameLayout);
    TEST_ASSERT_EQUAL_UINT32(1u, g_compacted.frameLayout->slotCount);
    TEST_ASSERT_EQUAL_UINT64(g_compacted.frameLayout->layoutHash, g_compacted.contract.layoutHash);
    TEST_ASSERT_TRUE(ZrCore_ExecIr_VerifyFunction(&g_compacted, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    TEST_ASSERT_TRUE(ZrParser_ExecIr_LowerAotWithCanonicalCallable(&g_compacted,
            g_fixture.module.constants, g_fixture.module.constantCount,
            g_fixture.module.layouts, g_fixture.module.layoutCount,
            g_fixture.compiler.semanticContext, g_fixture.compiler.preSemanticIr.callableTypeId,
            &g_projection, &diagnostic));
    TEST_ASSERT_EQUAL_UINT32(3u, g_projection.instructionCount);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_OPCODE_NOP, g_projection.opcodes[1]);
    TEST_ASSERT_EQUAL_MEMORY(g_compacted.frameLayout->slots, g_projection.frameSlots,
            sizeof(*g_projection.frameSlots));
    TEST_ASSERT_EQUAL_MEMORY(g_fixture.module.layouts, g_projection.layouts,
            (size_t)g_projection.layoutCount * sizeof(*g_projection.layouts));
    TEST_ASSERT_EQUAL_MEMORY(g_fixture.module.constants, g_projection.constants,
            (size_t)g_projection.constantCount * sizeof(*g_projection.constants));
    TEST_ASSERT_EQUAL_UINT32(g_compacted.sourceMapCount, g_projection.sourceMapCount);
    for (i = 0u; i < g_compacted.sourceMapCount; ++i) {
        const SZrExecIrSourceMap *map = &g_compacted.sourceMaps[i];
        const SZrExecIrProjectionSourceMap *out = &g_projection.sourceMaps[i];
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
    TEST_ASSERT_EQUAL_UINT32(0u, g_projection.stateMap.entryCount);
    TEST_ASSERT_EQUAL_UINT32(0u, g_projection.stateMap.valueCount);
    TEST_ASSERT_EQUAL_UINT32(0u, g_projection.stateMap.rootCount);
    TEST_ASSERT_EQUAL_UINT32(0u, g_projection.stateMap.ownerStateCount);
    TEST_ASSERT_FALSE(g_projection.runnable);
}

#if defined(_WIN32) && defined(_WIN64) && defined(_MSC_VER) && \
        (defined(_M_X64) || defined(_M_AMD64)) && !defined(_M_ARM64) && !defined(_M_ARM64EC)

static void prepare(EZrSsaLiteralScriptSource source) {
    SZrAotIrDiagnostic diagnostic = {0};
    TZrUInt32 i;
    project_source(source);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ExecIr_MakeHostNoArgsI64AotTarget(
            g_fixture.compiler.semanticContext, g_fixture.compiler.preSemanticIr.callableTypeId,
            &g_fixture.module.layouts[0], &g_target, &diagnostic), "PRECONDITION: actual host target");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_AotIrProjection_BuildDescriptor(&g_projection, &g_target,
            &g_fixture.module.contract, &g_descriptor, &diagnostic), "PRECONDITION: real descriptor");
    TEST_ASSERT_EQUAL_PTR(&g_projection, g_descriptor.owner);
    TEST_ASSERT_EQUAL_PTR(&g_descriptor.function, g_descriptor.module.functions);
    TEST_ASSERT_EQUAL_MEMORY(&g_fixture.module.contract, &g_descriptor.module.contract,
            sizeof(g_fixture.module.contract));
    TEST_ASSERT_EQUAL_MEMORY(&g_projection.contract, &g_descriptor.function.contract,
            sizeof(g_projection.contract));
    TEST_ASSERT_EQUAL_PTR(&g_projection.stateMap, g_descriptor.function.logicalStateMap);
    TEST_ASSERT_EQUAL_UINT32(3u, g_descriptor.function.instructionCount);
    TEST_ASSERT_EQUAL_UINT32(1u, g_descriptor.module.constantCount);
    TEST_ASSERT_EQUAL_INT64(g_fixture.expectedReturn, (TZrInt64)g_descriptor.constants[0].bits);
    TEST_ASSERT_EQUAL_UINT32(1u, g_descriptor.module.layoutCount);
    TEST_ASSERT_EQUAL_UINT32(1u, g_descriptor.function.frameSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, g_descriptor.function.valueSlotCount);
    TEST_ASSERT_EQUAL_UINT32(g_compacted.blocks[0].terminatorInstructionId, g_descriptor.instructions[2].id);
    TEST_ASSERT_EQUAL_UINT32(g_projection.sourceMapCount, g_descriptor.function.sourceMapCount);
    for (i = 0u; i < g_projection.sourceMapCount; ++i) {
        const SZrExecIrProjectionSourceMap *map = &g_projection.sourceMaps[i];
        const SZrAotIrSourceMap *out = &g_descriptor.sourceMaps[i];
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

static SLifetimeWitness witness(const SZrAotIrModule *module, TZrInt64 literal) {
    const SZrAotIrFunction *function = &module->functions[0];
    SLifetimeWitness result = {0};
    SZrAotIrDiagnostic diagnostic = {0};
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AOT_IR_OK, ZrCore_AotIr_ValidateModule(module, &diagnostic),
            "PRECONDITION: real descriptor Core validity");
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AOT_IR_OK, ZrCore_AotIr_RequireExecutableAbi(
            module, function->id, &result.abi, &diagnostic), "PRECONDITION: actual callable ABI");
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_CALLABLE_ABI_NOARGS_I64, result.abi.kind);
    result.functionId = function->id;
    result.literal = literal;
    result.semanticHash = ZrCore_AotIr_HashModule(module);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, result.semanticHash);
    result.functionToken = function->functionToken;
    result.signatureHash = function->signatureHash;
    TEST_ASSERT_NOT_NULL(function->logicalStateMap);
    result.generation = function->logicalStateMap->generation;
    result.target = module->target;
    result.moduleContract = module->contract;
    result.functionContract = function->contract;
    return result;
}

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

static void parse_text(const char *text, size_t length, TZrBool llvm, const SLifetimeWitness *saved) {
    const char *cursor = text;
    const char *end = text + length;
    TEST_ASSERT_EQUAL_CHAR('\0', text[length]);
    TEST_ASSERT_EQUAL_UINT64(length, strlen(text));
    consume(&cursor, end, llvm ? "define i64 @zr_aot_scalar_fn_"
            : "#include <stdint.h>\nint64_t zr_aot_scalar_fn_");
    TEST_ASSERT_EQUAL_UINT64(saved->functionId, decimal(&cursor, end));
    consume(&cursor, end, llvm ? "() {\nentry:\n  ret i64 " : "(void) {\n    return INT64_C(");
    TEST_ASSERT_EQUAL_UINT64((TZrUInt64)saved->literal, decimal(&cursor, end));
    consume(&cursor, end, llvm ? "\n}\n" : ");\n}\n");
    TEST_ASSERT_EQUAL_PTR(end, cursor);
}

static void emit_text(const SZrAotIrModule *module, const SLifetimeWitness *saved,
        FEmitter emit, TZrBool llvm) {
    char text[256];
    size_t length = 99u;
    SZrAotIrDiagnostic diagnostic = {0};
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AOT_IR_OK, emit(module, saved->functionId,
            text, sizeof(text), &length, &diagnostic), "PRECONDITION: scalar pure buffer GREEN");
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, diagnostic.status);
    TEST_ASSERT_TRUE(length < sizeof(text));
    parse_text(text, length, llvm, saved);
}

static void assert_live_module(const SZrAotIrModule *module, const SLifetimeWitness *saved) {
    const SZrAotIrFunction *function;
    const SZrExecIrStateMap *map;
    SZrAotIrDiagnostic diagnostic = {0};
    SZrAotIrCallableAbi abi = {0};
    TEST_ASSERT_NOT_NULL(module);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_ValidateModule(module, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_RequireExecutableAbi(
            module, saved->functionId, &abi, &diagnostic));
    TEST_ASSERT_EQUAL_MEMORY(&saved->abi, &abi, sizeof(abi));
    TEST_ASSERT_EQUAL_UINT64(saved->semanticHash, ZrCore_AotIr_HashModule(module));
    TEST_ASSERT_EQUAL_MEMORY(&saved->target, &module->target, sizeof(module->target));
    TEST_ASSERT_EQUAL_MEMORY(&saved->moduleContract, &module->contract, sizeof(module->contract));
    TEST_ASSERT_EQUAL_UINT32(1u, module->functionCount);
    TEST_ASSERT_EQUAL_UINT32(1u, module->constantCount);
    TEST_ASSERT_EQUAL_INT64(saved->literal, (TZrInt64)module->constantPool[0].bits);
    TEST_ASSERT_EQUAL_UINT32(1u, module->layoutCount);
    function = &module->functions[0];
    TEST_ASSERT_EQUAL_MEMORY(&saved->functionContract, &function->contract, sizeof(function->contract));
    TEST_ASSERT_EQUAL_UINT32(saved->functionId, function->id);
    TEST_ASSERT_EQUAL_UINT32(saved->functionToken, function->functionToken);
    TEST_ASSERT_EQUAL_UINT64(saved->signatureHash, function->signatureHash);
    TEST_ASSERT_EQUAL_UINT32(3u, function->instructionCount);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_OPCODE_CONSTANT, function->instructions[0].opcode);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_OPCODE_NOP, function->instructions[1].opcode);
    TEST_ASSERT_EQUAL_UINT32(ZR_EXEC_IR_OPCODE_RETURN, function->instructions[2].opcode);
    TEST_ASSERT_EQUAL_UINT32(1u, function->frameSlotCount);
    TEST_ASSERT_EQUAL_UINT32(1u, function->valueSlotCount);
    TEST_ASSERT_TRUE(function->sourceMapCount > 0u);
    map = function->logicalStateMap;
    TEST_ASSERT_NOT_NULL(map);
    TEST_ASSERT_EQUAL_UINT32(saved->functionToken, map->functionToken);
    TEST_ASSERT_EQUAL_UINT64(saved->signatureHash, map->signatureHash);
    TEST_ASSERT_EQUAL_UINT64(saved->generation, map->generation);
    TEST_ASSERT_EQUAL_UINT32(0u, map->entryCount);
    TEST_ASSERT_EQUAL_UINT32(0u, map->valueCount);
    TEST_ASSERT_EQUAL_UINT32(0u, map->rootCount);
    TEST_ASSERT_EQUAL_UINT32(0u, map->ownerStateCount);
    emit_text(module, saved, backend_aot_ir_c_emit_const_i64, ZR_FALSE);
    emit_text(module, saved, backend_aot_ir_llvm_emit_const_i64, ZR_TRUE);
}

/* Compare contents while both owners are live, including every reachable span.
 * Normalize only pointer fields in temporary records, never source capacities. */
static void assert_independent(const SZrAotIrModule *source, const SZrAotIrModule *copy) {
    SZrAotIrModule normalized;
    TZrUInt32 i;
    TEST_ASSERT_NOT_NULL(copy);
    TEST_ASSERT_TRUE(source != copy);
    normalized = *copy;
#define SPAN(s, d, field, count) do { \
    TEST_ASSERT_EQUAL_UINT32((s)->count, (d)->count); \
    if ((s)->count != 0u) { \
        TEST_ASSERT_NOT_NULL((d)->field); \
        TEST_ASSERT_TRUE((s)->field != (d)->field); \
        TEST_ASSERT_EQUAL_MEMORY((s)->field, (d)->field, \
                (size_t)(s)->count * sizeof(*(s)->field)); \
    } else TEST_ASSERT_NULL((d)->field); \
} while (0)
    SPAN(source, copy, constantPool, constantCount);
    SPAN(source, copy, layoutPool, layoutCount);
    TEST_ASSERT_EQUAL_UINT32(source->functionCount, copy->functionCount);
    TEST_ASSERT_NOT_NULL(copy->functions);
    TEST_ASSERT_TRUE(source->functions != copy->functions);
    normalized.functions = source->functions;
    normalized.constantPool = source->constantPool;
    normalized.layoutPool = source->layoutPool;
    TEST_ASSERT_EQUAL_MEMORY(source, &normalized, sizeof(normalized));
    for (i = 0u; i < source->functionCount; ++i) {
        const SZrAotIrFunction *s = &source->functions[i];
        const SZrAotIrFunction *d = &copy->functions[i];
        SZrAotIrFunction f = *d;
#define FUNCTION_SPAN(field, count) SPAN(s, d, field, count); f.field = s->field
        FUNCTION_SPAN(frameSlots, frameSlotCount);
        FUNCTION_SPAN(valueSlotPool, valueSlotCount);
        FUNCTION_SPAN(blocks, blockCount);
        FUNCTION_SPAN(instructions, instructionCount);
        FUNCTION_SPAN(operandPool, operandCount);
        FUNCTION_SPAN(resultPool, resultCount);
        FUNCTION_SPAN(phiIncomingPool, phiIncomingCount);
        FUNCTION_SPAN(successorPool, successorCount);
        FUNCTION_SPAN(memoryTokenPool, memoryTokenCount);
        FUNCTION_SPAN(sourceMaps, sourceMapCount);
        FUNCTION_SPAN(gcRootPool, gcRootCount);
        FUNCTION_SPAN(deoptStates, deoptStateCount);
        FUNCTION_SPAN(deoptValuePool, deoptValueCount);
        FUNCTION_SPAN(deoptAggregates, deoptAggregateCount);
        FUNCTION_SPAN(deoptAggregateFields, deoptAggregateFieldCount);
#undef FUNCTION_SPAN
        if (s->gcMap != ZR_NULL) {
            const SZrExecIrGcMap *a = s->gcMap, *b = d->gcMap;
            TEST_ASSERT_NOT_NULL(b);
            TEST_ASSERT_TRUE(a != b);
            SPAN(a, b, entries, entryCount);
            SPAN(a, b, slotIndexPool, slotIndexCount);
            SPAN(a, b, inlineRefOffsetPool, inlineRefOffsetCount);
            TEST_ASSERT_EQUAL_UINT32(a->safepointId, b->safepointId);
            TEST_ASSERT_EQUAL_UINT32(a->sourceId, b->sourceId);
            TEST_ASSERT_EQUAL_MEMORY(&a->rootRange, &b->rootRange, sizeof(a->rootRange));
            TEST_ASSERT_EQUAL_UINT32(b->entryCount, b->entryCapacity);
            TEST_ASSERT_EQUAL_UINT32(b->slotIndexCount, b->slotIndexCapacity);
            TEST_ASSERT_EQUAL_UINT32(b->inlineRefOffsetCount, b->inlineRefOffsetCapacity);
        } else TEST_ASSERT_NULL(d->gcMap);
        if (s->logicalStateMap != ZR_NULL) {
            const SZrExecIrStateMap *a = s->logicalStateMap, *b = d->logicalStateMap;
            TEST_ASSERT_NOT_NULL(b);
            TEST_ASSERT_TRUE(a != b);
            SPAN(a, b, entries, entryCount);
            SPAN(a, b, valuePool, valueCount);
            SPAN(a, b, rootPool, rootCount);
            SPAN(a, b, ownerStatePool, ownerStateCount);
            TEST_ASSERT_EQUAL_UINT32(a->functionToken, b->functionToken);
            TEST_ASSERT_EQUAL_UINT64(a->signatureHash, b->signatureHash);
            TEST_ASSERT_EQUAL_UINT64(a->generation, b->generation);
            TEST_ASSERT_EQUAL_UINT32(b->entryCount, b->entryCapacity);
            TEST_ASSERT_EQUAL_UINT32(b->valueCount, b->valueCapacity);
            TEST_ASSERT_EQUAL_UINT32(b->rootCount, b->rootCapacity);
            TEST_ASSERT_EQUAL_UINT32(b->ownerStateCount, b->ownerStateCapacity);
        } else TEST_ASSERT_NULL(d->logicalStateMap);
        f.gcMap = s->gcMap;
        f.logicalStateMap = s->logicalStateMap;
        TEST_ASSERT_EQUAL_MEMORY(s, &f, sizeof(f));
    }
#undef SPAN
    TEST_ASSERT_EQUAL_UINT64(ZrCore_AotIr_HashModule(source), ZrCore_AotIr_HashModule(copy));
}

static void clone_success(const SZrAotIrModule *source, SZrAotIrOwnedModule **destination,
        TZrBool nullDiagnostic) {
    SZrAotIrDiagnostic diagnostic = {0};
    EZrAotIrStatus status = ZrCore_AotIr_CloneOwnedModule(source, destination,
            nullDiagnostic ? ZR_NULL : &diagnostic);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_AOT_IR_OK, status,
            "FEATURE RED: actual source module requires independent Core ownership");
    TEST_ASSERT_NOT_NULL(*destination);
    if (!nullDiagnostic) TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, diagnostic.status);
    assert_independent(source, ZrCore_AotIr_OwnedModuleView(*destination));
}

static void prerequisite(EZrSsaLiteralScriptSource source) {
    SLifetimeWitness saved;
    prepare(source);
    saved = witness(&g_descriptor.module, g_fixture.expectedReturn);
    TEST_ASSERT_EQUAL_UINT32(return_type(), saved.abi.returnTypeToken);
    assert_live_module(&g_descriptor.module, &saved);
}
static void test_prerequisite_nine(void) { prerequisite(ZR_TEST_SSA_LITERAL_SCRIPT_NINE); }
static void test_prerequisite_eight(void) { prerequisite(ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT); }

static SLifetimeWitness own_source(EZrSsaLiteralScriptSource source) {
    SLifetimeWitness saved;
    prepare(source);
    saved = witness(&g_descriptor.module, g_fixture.expectedReturn);
    clone_success(&g_descriptor.module, &g_owner, ZR_FALSE);
    release_producers();
    /* All producer globals are now clear. Never inspect their saved addresses. */
    assert_live_module(ZrCore_AotIr_OwnedModuleView(g_owner), &saved);
    return saved;
}
static void test_nine_survives_producers(void) { (void)own_source(ZR_TEST_SSA_LITERAL_SCRIPT_NINE); }
static void test_eight_survives_producers(void) { (void)own_source(ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT); }

static void test_owner_to_owner_survives_first_free(void) {
    SLifetimeWitness saved = own_source(ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    clone_success(ZrCore_AotIr_OwnedModuleView(g_owner), &g_second, ZR_TRUE);
    ZrCore_AotIr_OwnedModuleFree(&g_owner);
    TEST_ASSERT_NULL(g_owner);
    assert_live_module(ZrCore_AotIr_OwnedModuleView(g_second), &saved);
}

static void test_exact_self_source_replaces_arena(void) {
    SLifetimeWitness saved = own_source(ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    const SZrAotIrModule *source = ZrCore_AotIr_OwnedModuleView(g_owner);
    /* Integer address witnesses are compared only, never dereferenced after free. */
    uintptr_t oldView = (uintptr_t)source;
    uintptr_t oldFunctions = (uintptr_t)source->functions;
    uintptr_t oldInstructions = (uintptr_t)source->functions[0].instructions;
    SZrAotIrDiagnostic diagnostic = {0};
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, ZrCore_AotIr_CloneOwnedModule(source, &g_owner, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, diagnostic.status);
    source = ZrCore_AotIr_OwnedModuleView(g_owner);
    TEST_ASSERT_NOT_NULL(source);
    TEST_ASSERT_TRUE(oldView != (uintptr_t)source);
    TEST_ASSERT_TRUE(oldFunctions != (uintptr_t)source->functions);
    TEST_ASSERT_TRUE(oldInstructions != (uintptr_t)source->functions[0].instructions);
    assert_live_module(source, &saved);
}

static void test_replace_nine_with_actual_eight(void) {
    SLifetimeWitness nine = own_source(ZR_TEST_SSA_LITERAL_SCRIPT_NINE), eight;
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    ZrTests_SsaLiteralScriptFixture_Init(&g_fixture, g_state);
    ZrCore_ExecIr_FunctionInit(&g_compacted);
    TEST_ASSERT_NOT_NULL(g_state);
    prepare(ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT);
    eight = witness(&g_descriptor.module, g_fixture.expectedReturn);
    TEST_ASSERT_EQUAL_INT64(9, nine.literal);
    TEST_ASSERT_EQUAL_INT64(8, eight.literal);
    clone_success(&g_descriptor.module, &g_owner, ZR_FALSE);
    release_producers();
    assert_live_module(ZrCore_AotIr_OwnedModuleView(g_owner), &eight);
}

/* This observation includes allocation identities and live storage; it is not
 * an invented module/ABI hash. It is only used to test refusal atomicity. */
static TZrUInt64 owner_observation(const SZrAotIrModule *module) {
    const SZrAotIrFunction *f = &module->functions[0];
    TZrUInt64 hash = ZrTests_SsaLiteralScriptFixture_DigestBytes(0u, module, sizeof(*module));
#define OBSERVE(ptr, size) hash = ZrTests_SsaLiteralScriptFixture_DigestBytes(hash, ptr, size)
#define POOL(field, count) if (f->count != 0u) OBSERVE(f->field, (size_t)f->count * sizeof(*f->field))
    OBSERVE(module->functions, (size_t)module->functionCount * sizeof(*module->functions));
    OBSERVE(module->constantPool, (size_t)module->constantCount * sizeof(*module->constantPool));
    OBSERVE(module->layoutPool, (size_t)module->layoutCount * sizeof(*module->layoutPool));
    POOL(frameSlots, frameSlotCount); POOL(valueSlotPool, valueSlotCount);
    POOL(blocks, blockCount); POOL(instructions, instructionCount);
    POOL(operandPool, operandCount); POOL(resultPool, resultCount);
    POOL(phiIncomingPool, phiIncomingCount); POOL(successorPool, successorCount);
    POOL(memoryTokenPool, memoryTokenCount); POOL(sourceMaps, sourceMapCount);
    POOL(gcRootPool, gcRootCount); POOL(deoptStates, deoptStateCount);
    POOL(deoptValuePool, deoptValueCount); POOL(deoptAggregates, deoptAggregateCount);
    POOL(deoptAggregateFields, deoptAggregateFieldCount);
    /* Actual finite sources have an empty logical header and no GC header. */
    TEST_ASSERT_NULL(f->gcMap);
    TEST_ASSERT_NOT_NULL(f->logicalStateMap);
    OBSERVE(f->logicalStateMap, sizeof(*f->logicalStateMap));
#undef POOL
#undef OBSERVE
    return hash;
}

static void invalid_source(TZrBool opcode) {
    SLifetimeWitness saved;
    SZrAotIrDiagnostic coreDiagnostic = {0}, cloneDiagnostic = {0};
    EZrAotIrStatus coreStatus, cloneStatus;
    SZrAotIrOwnedModule *oldOwner;
    const SZrAotIrModule *oldView;
    TZrUInt64 before, sourceHash, savedContract;
    SZrAotIrInstruction savedInstruction;
    prepare(ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    saved = witness(&g_descriptor.module, g_fixture.expectedReturn);
    clone_success(&g_descriptor.module, &g_owner, ZR_FALSE);
    oldOwner = g_owner;
    oldView = ZrCore_AotIr_OwnedModuleView(g_owner);
    before = owner_observation(oldView);
    sourceHash = ZrCore_AotIr_HashModule(&g_descriptor.module);
    savedInstruction = g_descriptor.instructions[1];
    savedContract = g_descriptor.function.contract.signatureHash;
    if (opcode) g_descriptor.instructions[1].flags |= ZR_EXEC_IR_INSTRUCTION_FLAG_KNOWN_MASK + 1u;
    else g_descriptor.function.contract.signatureHash ^= UINT64_C(1);
    coreStatus = ZrCore_AotIr_ValidateModule(&g_descriptor.module, &coreDiagnostic);
    cloneStatus = ZrCore_AotIr_CloneOwnedModule(&g_descriptor.module, &g_owner, &cloneDiagnostic);
    /* Restore actual bounded fields before any assertion can longjmp. */
    g_descriptor.instructions[1] = savedInstruction;
    g_descriptor.function.contract.signatureHash = savedContract;
    TEST_ASSERT_EQUAL_INT(opcode ? ZR_AOT_IR_INVALID_OPCODE : ZR_AOT_IR_INVALID_CONTRACT, coreStatus);
    TEST_ASSERT_EQUAL_INT(coreStatus, cloneStatus);
    TEST_ASSERT_EQUAL_MEMORY(&coreDiagnostic, &cloneDiagnostic, sizeof(coreDiagnostic));
    TEST_ASSERT_EQUAL_PTR(oldOwner, g_owner);
    TEST_ASSERT_EQUAL_PTR(oldView, ZrCore_AotIr_OwnedModuleView(g_owner));
    TEST_ASSERT_EQUAL_UINT64(before, owner_observation(oldView));
    TEST_ASSERT_EQUAL_UINT64(sourceHash, ZrCore_AotIr_HashModule(&g_descriptor.module));
    release_producers();
    assert_live_module(ZrCore_AotIr_OwnedModuleView(g_owner), &saved);
}
static void test_actual_opcode_error_is_atomic(void) { invalid_source(ZR_TRUE); }
static void test_actual_contract_error_is_atomic(void) { invalid_source(ZR_FALSE); }

static void owned_buffer_boundaries(const SZrAotIrModule *module, const SLifetimeWitness *saved,
        FEmitter emit, TZrBool llvm) {
    char text[256];
    size_t length = 99u, required;
    SZrAotIrDiagnostic diagnostic = {0};
    TZrUInt64 before = owner_observation(module);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, emit(module, saved->functionId,
            text, sizeof(text), &length, ZR_NULL));
    TEST_ASSERT_TRUE(length < sizeof(text));
    parse_text(text, length, llvm, saved);
    required = length;
    memset(text, 0xa5, sizeof(text));
    length = 99u;
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_RANGE, emit(module, saved->functionId,
            text, required, &length, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_RANGE, diagnostic.status);
    TEST_ASSERT_EQUAL_UINT64(required + 1u, diagnostic.expected);
    TEST_ASSERT_EQUAL_UINT64(required, diagnostic.actual);
    TEST_ASSERT_EQUAL_CHAR('\0', text[0]);
    TEST_ASSERT_EQUAL_UINT64(0u, length);
    TEST_ASSERT_EQUAL_UINT8(0xa5u, (unsigned char)text[required]);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_OK, emit(module, saved->functionId,
            text, required + 1u, &length, &diagnostic));
    TEST_ASSERT_EQUAL_UINT64(required, length);
    parse_text(text, length, llvm, saved);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_ARGUMENT, emit(module, saved->functionId,
            ZR_NULL, sizeof(text), &length, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_ARGUMENT, diagnostic.status);
    TEST_ASSERT_EQUAL_UINT64(0u, length);
    text[0] = 'x';
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_ARGUMENT, emit(module, saved->functionId,
            text, sizeof(text), ZR_NULL, &diagnostic));
    TEST_ASSERT_EQUAL_CHAR('\0', text[0]);
    text[0] = 'x';
    length = 99u;
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_ARGUMENT, emit(module, saved->functionId,
            text, 0u, &length, ZR_NULL));
    TEST_ASSERT_EQUAL_CHAR('x', text[0]);
    TEST_ASSERT_EQUAL_UINT64(0u, length);
    TEST_ASSERT_EQUAL_UINT64(before, owner_observation(module));
}

static void test_required_arguments_and_nullsafe_free(void) {
    SLifetimeWitness saved = own_source(ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    SZrAotIrOwnedModule *oldOwner = g_owner;
    const SZrAotIrModule *view = ZrCore_AotIr_OwnedModuleView(g_owner);
    TZrUInt64 before = owner_observation(view);
    SZrAotIrDiagnostic diagnostic = {0};
    TEST_ASSERT_NULL(ZrCore_AotIr_OwnedModuleView(ZR_NULL));
    ZrCore_AotIr_OwnedModuleFree(ZR_NULL);
    ZrCore_AotIr_OwnedModuleFree(&g_second);
    ZrCore_AotIr_OwnedModuleFree(&g_second);
    TEST_ASSERT_NULL(g_second);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_ARGUMENT,
            ZrCore_AotIr_CloneOwnedModule(ZR_NULL, &g_owner, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_ARGUMENT, diagnostic.status);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_ARGUMENT,
            ZrCore_AotIr_CloneOwnedModule(view, ZR_NULL, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_ARGUMENT, diagnostic.status);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_ARGUMENT,
            ZrCore_AotIr_CloneOwnedModule(ZR_NULL, &g_owner, ZR_NULL));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_ARGUMENT,
            ZrCore_AotIr_CloneOwnedModule(view, ZR_NULL, ZR_NULL));
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_INVALID_ARGUMENT,
            ZrCore_AotIr_CloneOwnedModule(ZR_NULL, &g_second, ZR_NULL));
    TEST_ASSERT_NULL(g_second);
    TEST_ASSERT_EQUAL_PTR(oldOwner, g_owner);
    TEST_ASSERT_EQUAL_PTR(view, ZrCore_AotIr_OwnedModuleView(g_owner));
    TEST_ASSERT_EQUAL_UINT64(before, owner_observation(view));
    assert_live_module(view, &saved);
    owned_buffer_boundaries(view, &saved, backend_aot_ir_c_emit_const_i64, ZR_FALSE);
    owned_buffer_boundaries(view, &saved, backend_aot_ir_llvm_emit_const_i64, ZR_TRUE);
    ZrCore_AotIr_OwnedModuleFree(&g_owner);
    TEST_ASSERT_NULL(g_owner);
    TEST_ASSERT_NULL(ZrCore_AotIr_OwnedModuleView(g_owner));
    ZrCore_AotIr_OwnedModuleFree(&g_owner);
    TEST_ASSERT_NULL(g_owner);
}

#else
static void test_other_host_actual_target_is_unsupported(void) {
    SZrAotIrDiagnostic diagnostic = {0};
    SZrAotIrTargetContract before, observed;
    TZrBool result;
    project_source(ZR_TEST_SSA_LITERAL_SCRIPT_NINE);
    memset(&g_target, 0xa5, sizeof(g_target));
    before = g_target;
    result = ZrParser_ExecIr_MakeHostNoArgsI64AotTarget(g_fixture.compiler.semanticContext,
            g_fixture.compiler.preSemanticIr.callableTypeId, &g_fixture.module.layouts[0],
            &g_target, &diagnostic);
    observed = g_target;
    memset(&g_target, 0, sizeof(g_target));
    TEST_ASSERT_FALSE(result);
    TEST_ASSERT_EQUAL_INT(ZR_AOT_IR_UNSUPPORTED, diagnostic.status);
    TEST_ASSERT_EQUAL_MEMORY(&before, &observed, sizeof(before));
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
        RUN_TEST(test_prerequisite_nine);
        RUN_TEST(test_prerequisite_eight);
    }
    if (!prerequisitesOnly) {
        RUN_TEST(test_nine_survives_producers);
        RUN_TEST(test_eight_survives_producers);
        if (!featuresOnly) {
            RUN_TEST(test_owner_to_owner_survives_first_free);
            RUN_TEST(test_exact_self_source_replaces_arena);
            RUN_TEST(test_replace_nine_with_actual_eight);
            RUN_TEST(test_actual_opcode_error_is_atomic);
            RUN_TEST(test_actual_contract_error_is_atomic);
            RUN_TEST(test_required_arguments_and_nullsafe_free);
        }
    }
#else
    (void)prerequisitesOnly;
    (void)featuresOnly;
    RUN_TEST(test_other_host_actual_target_is_unsupported);
#endif
    return UNITY_END();
}
