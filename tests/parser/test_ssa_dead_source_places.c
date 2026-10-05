#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* CRT declarations precede Unity's noreturn macro on Windows. */
#include "unity.h"
#include "harness/runtime_support.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash.h"
#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_core/string.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/exec_ir_builder.h"
#include "zr_vm_parser/exec_ir_dead_source_places.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/project_imports.h"
#include "../../zr_vm_parser/src/zr_vm_parser/compiler/module_init_analysis.h"

enum { DEAD_PLACE_FIXTURE_COUNT = 2, DEAD_PLACE_ORACLE_STEPS = 32 };

typedef struct SZrDeadPlaceFixture {
    SZrAstNode *ast;
    SZrCompilerState compiler;
    TZrBool compilerInitialized;
    TZrBool functionRooted;
    SZrExecIrModule module;
    SZrExecIrOracleValue *constants;
    SZrExecIrOracleValue *initialValues;
    TZrUInt32 placeCalls;
    TZrInt64 expectedReturn;
} SZrDeadPlaceFixture;

static SZrState *g_state;
static SZrDeadPlaceFixture g_fixtures[DEAD_PLACE_FIXTURE_COUNT];
static SZrExecIrFunction g_output;
static SZrExecIrFunction g_mutated;
static SZrExecIrOracleExecutionResult g_oracle;

void setUp(void) {
    memset(g_fixtures, 0, sizeof(g_fixtures));
    ZrCore_ExecIr_FunctionInit(&g_output);
    ZrCore_ExecIr_FunctionInit(&g_mutated);
    ZrCore_ExecIr_OracleResultInit(&g_oracle);
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    TZrUInt32 index;
    ZrCore_ExecIr_OracleResultFree(&g_oracle);
    ZrCore_ExecIr_FreeFunction(&g_output);
    ZrCore_ExecIr_FreeFunction(&g_mutated);
    for (index = 0u; index < DEAD_PLACE_FIXTURE_COUNT; ++index) {
        SZrDeadPlaceFixture *fixture = &g_fixtures[index];
        ZrCore_ExecIr_FreeModule(&fixture->module);
        free(fixture->constants);
        free(fixture->initialValues);
        if (fixture->compilerInitialized) {
            if (fixture->compiler.currentFunction != ZR_NULL) {
                if (fixture->functionRooted)
                    ZrCore_GarbageCollector_UnignoreObject(g_state->global,
                            &fixture->compiler.currentFunction->super);
                ZrCore_Function_Free(g_state, fixture->compiler.currentFunction);
                fixture->compiler.currentFunction = ZR_NULL;
            }
            ZrParser_CompilerState_Free(&fixture->compiler);
        }
        if (fixture->ast != ZR_NULL) {
            ZrParser_ModuleInitAnalysis_ClearAstIdentity(g_state->global, fixture->ast);
            ZrParser_Ast_Free(g_state, fixture->ast);
        }
    }
    if (g_state != ZR_NULL) ZrTests_Runtime_State_Destroy(g_state);
    g_state = ZR_NULL;
}

static void assert_api(TZrBool success, const SZrExecIrDiagnostic *diagnostic,
                       const char *message) {
    if (!success && diagnostic != ZR_NULL) {
        (void)printf("%s: code=%u token=%u instruction=%u source=%u\n", message,
                (unsigned)diagnostic->code, (unsigned)diagnostic->functionToken,
                (unsigned)diagnostic->instructionId, (unsigned)diagnostic->sourceId);
    }
    TEST_ASSERT_TRUE_MESSAGE(success, message);
}

static SZrExecIrFunction *fixture_function(SZrDeadPlaceFixture *fixture) {
    return ZrCore_ExecIr_ModuleFunctionAt(&fixture->module, 1u);
}

/* These digests detect in-place changes, including allocation identity and
 * capacities. They are observations within one test, never ABI identities. */
static TZrUInt64 digest_bytes(TZrUInt64 previous, const void *bytes, size_t size) {
    return previous ^ ZrCore_Hash_CreateStable64WithPrefix(
            (const TZrByte *)&previous, sizeof(previous),
            (const TZrByte *)bytes, size);
}

static TZrUInt64 digest_array(TZrUInt64 hash, const SZrArray *array) {
    return digest_bytes(hash, array->head, array->length * array->elementSize);
}

static TZrUInt64 function_digest(const SZrExecIrFunction *function) {
    TZrUInt64 hash = digest_bytes(0u, function, sizeof(*function));
#define HASH_POOL(field, count) \
    hash = digest_bytes(hash, function->field, \
            (size_t)function->count * sizeof(*function->field))
    HASH_POOL(values, valueCount);
    HASH_POOL(instructions, instructionCount);
    HASH_POOL(blocks, blockCount);
    HASH_POOL(operandPool, operandCount);
    HASH_POOL(resultPool, resultCount);
    HASH_POOL(memoryTokenPool, memoryTokenCount);
    HASH_POOL(phiPool, phiCount);
    HASH_POOL(phiIncoming, phiIncomingCount);
    HASH_POOL(predecessors, predecessorCount);
    HASH_POOL(successors, successorCount);
    HASH_POOL(gcRoots, gcRootCount);
    HASH_POOL(deoptStates, deoptStateCount);
    HASH_POOL(deoptValues, deoptValueCount);
    HASH_POOL(deoptAggregates, deoptAggregateCount);
    HASH_POOL(deoptAggregateFields, deoptAggregateFieldCount);
    HASH_POOL(sourceMaps, sourceMapCount);
    HASH_POOL(bindingRows, bindingRowCount);
#undef HASH_POOL
    if (function->stateMap != ZR_NULL) {
        const SZrExecIrStateMap *map = function->stateMap;
        hash = digest_bytes(hash, map, sizeof(*map));
        hash = digest_bytes(hash, map->entries, (size_t)map->entryCount * sizeof(*map->entries));
        hash = digest_bytes(hash, map->valuePool, (size_t)map->valueCount * sizeof(*map->valuePool));
        hash = digest_bytes(hash, map->rootPool, (size_t)map->rootCount * sizeof(*map->rootPool));
        hash = digest_bytes(hash, map->ownerStatePool,
                (size_t)map->ownerStateCount * sizeof(*map->ownerStatePool));
    }
    return hash;
}

static TZrUInt64 source_digest(const SZrDeadPlaceFixture *fixture) {
    const SZrSemanticIrFunction *semantic = &fixture->compiler.preSemanticIr;
    const SZrSemanticContext *context = fixture->compiler.semanticContext;
    TZrUInt64 hash = digest_bytes(0u, semantic, sizeof(*semantic));
#define HASH_SEM(field) hash = digest_array(hash, &semantic->field)
    HASH_SEM(places.places);
    HASH_SEM(cfg.blocks);
    HASH_SEM(locals);
    HASH_SEM(values);
    HASH_SEM(instructions);
    HASH_SEM(valueOperands);
    HASH_SEM(regions);
    HASH_SEM(cleanupScopes);
    HASH_SEM(sourceMap);
    HASH_SEM(loanFacts);
    HASH_SEM(escapeFacts);
    HASH_SEM(contiguousViewFacts);
    HASH_SEM(boundsFacts);
    HASH_SEM(scalarScratchProofs);
#undef HASH_SEM
    hash = digest_bytes(hash, context, sizeof(*context));
#define HASH_CONTEXT(field) hash = digest_array(hash, &context->field)
    HASH_CONTEXT(canonicalTypes);
    HASH_CONTEXT(canonicalTypeHashBuckets);
    HASH_CONTEXT(canonicalTypeHashNext);
    HASH_CONTEXT(canonicalTypeDefinitions);
    HASH_CONTEXT(types);
    HASH_CONTEXT(symbols);
    HASH_CONTEXT(scopeFacts);
    HASH_CONTEXT(visibleSymbolFacts);
    HASH_CONTEXT(overloadSets);
    HASH_CONTEXT(cleanupPlan);
    HASH_CONTEXT(templateSegments);
    HASH_CONTEXT(queryDiagnostics);
    HASH_CONTEXT(expressionFacts);
    HASH_CONTEXT(referenceFacts);
    HASH_CONTEXT(numericFacts);
    HASH_CONTEXT(reachabilityFacts);
    HASH_CONTEXT(logicalFacts);
    HASH_CONTEXT(ownershipFacts);
    HASH_CONTEXT(ownershipIntrinsicFacts);
    HASH_CONTEXT(receiverGuardFacts);
    HASH_CONTEXT(diagnosticFacts);
    HASH_CONTEXT(propertyContracts);
    HASH_CONTEXT(relationFacts);
    HASH_CONTEXT(callEdgeFacts);
    HASH_CONTEXT(typeDisplayAliasFacts);
    HASH_CONTEXT(documentationFacts);
#undef HASH_CONTEXT
    return digest_array(hash, &fixture->compiler.constants);
}

static TZrBool literal_place_resolver(void *userData,
        const SZrExecIrInstruction *instruction, const SZrExecIrOracleValue *operands,
        TZrUInt32 operandCount, SZrExecIrOracleValue *result) {
    SZrDeadPlaceFixture *fixture = (SZrDeadPlaceFixture *)userData;
    const SZrSemanticIrInstruction *source;
    const SZrParserPlace *place;
    if (fixture == ZR_NULL || instruction == ZR_NULL || result == ZR_NULL ||
        instruction->opcode != ZR_EXEC_IR_OPCODE_PLACE_BASE ||
        instruction->sourceId == 0u || operandCount != 1u || operands == ZR_NULL)
        return ZR_FALSE;
    source = ZrParser_SemanticIr_InstructionAt(
            &fixture->compiler.preSemanticIr, instruction->sourceId - 1u);
    if (source == ZR_NULL || source->opcode != ZR_SEMANTIC_IR_PLACE_BASE)
        return ZR_FALSE;
    place = ZrParser_PlaceGraph_Get(&fixture->compiler.preSemanticIr.places, source->placeId);
    if (place == ZR_NULL || place->base.kind != ZR_PARSER_PLACE_BASE_TEMPORARY ||
        operands[0].kind != ZR_EXEC_IR_ORACLE_VALUE_SIGNED ||
        operands[0].as.signedInteger != (TZrInt64)place->base.identity)
        return ZR_FALSE;
    result->kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    result->as.signedInteger = (TZrInt64)place->id;
    ++fixture->placeCalls;
    return ZR_TRUE;
}

static void attach_actual_constants(SZrDeadPlaceFixture *fixture) {
    SZrCompilerState *compiler = &fixture->compiler;
    TZrSize index;
    TEST_ASSERT_GREATER_THAN_UINT32(0u, (TZrUInt32)compiler->constants.length);
    fixture->constants = (SZrExecIrOracleValue *)calloc(
            compiler->constants.length, sizeof(*fixture->constants));
    TEST_ASSERT_NOT_NULL(fixture->constants);
    for (index = 0u; index < compiler->constants.length; ++index) {
        const SZrTypeValue *value = (const SZrTypeValue *)ZrCore_Array_Get(&compiler->constants, index);
        const SZrSemanticIrInstruction *source = ZrParser_SemanticIr_InstructionAt(
                &compiler->preSemanticIr, 0u);
        const SZrCanonicalTypeNode *type;
        SZrExecIrConstant constant = {0};
        SZrExecIrRange range;
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, value->type);
        TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_CONSTANT, source->opcode);
        TEST_ASSERT_TRUE(source->hasConstantPoolIndex);
        TEST_ASSERT_EQUAL_UINT32((TZrUInt32)index, source->constantPoolIndex);
        type = ZrParser_CanonicalType_Find(compiler->semanticContext, source->typeId);
        TEST_ASSERT_NOT_NULL(type);
        TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_PRIMITIVE, type->kind);
        TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, type->data.primitive.valueType);
        constant.typeToken = source->typeId;
        constant.bits = (TZrUInt64)value->value.nativeObject.nativeInt64;
        TEST_ASSERT_TRUE(ZrCore_ExecIr_ModuleAppendConstant(&fixture->module, &constant, 1u, &range));
        TEST_ASSERT_EQUAL_UINT32((TZrUInt32)index, range.offset);
        fixture->constants[index].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
        fixture->constants[index].as.signedInteger = value->value.nativeObject.nativeInt64;
    }
}

static void assert_source_maps(const SZrDeadPlaceFixture *fixture,
                               const SZrExecIrFunction *function) {
    const SZrSemanticIrFunction *semantic = &fixture->compiler.preSemanticIr;
    TZrUInt32 index;
    TEST_ASSERT_EQUAL_UINT32((TZrUInt32)semantic->sourceMap.length, function->sourceMapCount);
    for (index = 0u; index < function->instructionCount; ++index) {
        const SZrExecIrInstruction *instruction = &function->instructions[index];
        const SZrSemanticIrInstruction *source;
        const SZrExecIrSourceMap *map = ZR_NULL;
        TZrUInt32 mapIndex;
        TEST_ASSERT_TRUE(instruction->sourceId > 0u && instruction->sourceId <= semantic->instructions.length);
        source = ZrParser_SemanticIr_InstructionAt(semantic, instruction->sourceId - 1u);
        for (mapIndex = 0u; mapIndex < function->sourceMapCount; ++mapIndex) {
            if (function->sourceMaps[mapIndex].instructionId != index + 1u) continue;
            TEST_ASSERT_NULL_MESSAGE(map, "PRECONDITION: unique instruction source map");
            map = &function->sourceMaps[mapIndex];
        }
        TEST_ASSERT_NOT_NULL(map);
        TEST_ASSERT_EQUAL_UINT32(source->id, map->sourceId);
        TEST_ASSERT_EQUAL_UINT32(source->sourceRange.start.offset, map->startOffset);
        TEST_ASSERT_EQUAL_UINT32(source->sourceRange.end.offset, map->endOffset);
        TEST_ASSERT_EQUAL_UINT32(source->sourceRange.start.line, map->startLine);
        TEST_ASSERT_EQUAL_UINT32(source->sourceRange.start.column, map->startColumn);
        TEST_ASSERT_EQUAL_UINT32(source->sourceRange.end.line, map->endLine);
        TEST_ASSERT_EQUAL_UINT32(source->sourceRange.end.column, map->endColumn);
    }
}

static void assert_oracle(SZrDeadPlaceFixture *fixture,
                          const SZrExecIrFunction *function, TZrUInt32 expectedPlaceCalls) {
    SZrExecIrOracleInput input = {0};
    SZrExecIrDiagnostic diagnostic = {0};
    fixture->placeCalls = 0u;
    input.function = function;
    input.constants = fixture->constants;
    input.constantCount = fixture->module.constantCount;
    input.initialValues = fixture->initialValues;
    input.initialValueCount = function->valueCount;
    input.place = literal_place_resolver;
    input.placeUserData = fixture;
    input.maxSteps = DEAD_PLACE_ORACLE_STEPS;
    assert_api(ZrCore_ExecIr_RunOracleEx(&input, &g_oracle, &diagnostic), &diagnostic,
            "PRECONDITION: actual canonical graph Core Oracle");
    TEST_ASSERT_TRUE(g_oracle.returned);
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_ORACLE_VALUE_SIGNED, g_oracle.returnValue.kind);
    TEST_ASSERT_EQUAL_INT64(fixture->expectedReturn, g_oracle.returnValue.as.signedInteger);
    TEST_ASSERT_EQUAL_UINT32(0u, g_oracle.eventCount);
    TEST_ASSERT_EQUAL_UINT32(expectedPlaceCalls, fixture->placeCalls);
    ZrCore_ExecIr_OracleResultFree(&g_oracle);
    ZrCore_ExecIr_OracleResultInit(&g_oracle);
}

static SZrDeadPlaceFixture *prepare_fixture(TZrUInt32 index) {
    static const char *const sources[DEAD_PLACE_FIXTURE_COUNT] = {"return 9;\n", "return 8;\n"};
    static const char *const names[DEAD_PLACE_FIXTURE_COUNT] = {
        "ssa_dead_source_places_nine.zr", "ssa_dead_source_places_eight.zr"};
    SZrDeadPlaceFixture *fixture = &g_fixtures[index];
    SZrCompilerState *compiler = &fixture->compiler;
    SZrString *name, *moduleKey = ZR_NULL;
    TZrChar error[ZR_PARSER_ERROR_BUFFER_LENGTH] = {0};
    SZrFileRange errorRange = {0};
    SZrExecIrBuildInput build = {0};
    SZrExecIrDiagnostic diagnostic = {0};
    const SZrMetadataTokenRecord *entry = ZR_NULL, *module = ZR_NULL;
    const SZrCanonicalTypeNode *callable;
    SZrExecIrFunction *function;
    const SZrSemanticIrInstruction *placeSource;
    const SZrParserPlace *place;
    TZrUInt32 row;
    fixture->expectedReturn = index == 0u ? 9 : 8;
    name = ZrCore_String_CreateFromNative(g_state, names[index]);
    TEST_ASSERT_NOT_NULL(name);
    fixture->ast = ZrParser_Parse(g_state, sources[index], strlen(sources[index]), name);
    TEST_ASSERT_NOT_NULL_MESSAGE(fixture->ast, "PRECONDITION: real source AST");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ProjectImports_CanonicalizeAst(g_state, fixture->ast,
            name, &moduleKey, error, sizeof(error), &errorRange), error);
    TEST_ASSERT_NOT_NULL(moduleKey);
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ModuleInitAnalysis_PrepareCurrentSourceModule(
            g_state, moduleKey, fixture->ast), "PRECONDITION: ModuleInit Prepare");
    ZrParser_CompilerState_Init(compiler, g_state);
    fixture->compilerInitialized = ZR_TRUE;
    compiler->currentAst = fixture->ast;
    compiler->currentModuleKey = moduleKey;
    compiler->currentFunction = ZrCore_Function_New(g_state);
    TEST_ASSERT_NOT_NULL(compiler->currentFunction);
    fixture->functionRooted = ZrCore_GarbageCollector_IgnoreObject(g_state,
            &compiler->currentFunction->super);
    TEST_ASSERT_TRUE(fixture->functionRooted);
    compile_script(compiler, fixture->ast);
    TEST_ASSERT_FALSE_MESSAGE(compiler->hasError, "PRECONDITION: production compile_script");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_Compiler_ValidatePreSemanticIr(compiler),
            "PRECONDITION: source semantic validation");
    TEST_ASSERT_TRUE(compiler->preSemanticIrCfgActive);
    TEST_ASSERT_TRUE(compiler->preSemanticIrCfgTerminated);
    TEST_ASSERT_TRUE_MESSAGE(compiler_assemble_final_function(compiler,
            compiler->currentFunction, fixture->ast, ZR_TRUE, ZR_FALSE),
            "PRECONDITION: production assembly before Finalize");
    TEST_ASSERT_TRUE_MESSAGE(ZrParser_ModuleInitAnalysis_FinalizeCurrentSourceModule(
            compiler, moduleKey, compiler->currentFunction), "PRECONDITION: ModuleInit Finalize");
    TEST_ASSERT_TRUE_MESSAGE(compiler->currentFunction->hasSourceCallableIdentity,
            "PRECONDITION: production literal SCRIPT canonical identity");
    TEST_ASSERT_TRUE(ZrParser_SemanticIr_Validate(&compiler->preSemanticIr));
    callable = ZrParser_CanonicalType_Find(compiler->semanticContext,
            compiler->preSemanticIr.callableTypeId);
    TEST_ASSERT_NOT_NULL(callable);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, callable->kind);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, callable->structuralHash);
    TEST_ASSERT_EQUAL_UINT64(callable->structuralHash,
            compiler->currentFunction->sourceCallableIdentity.canonicalSignatureHash);
    for (row = 0u; row < compiler->currentFunction->metadataTokenRecordLength; ++row) {
        const SZrMetadataTokenRecord *record = &compiler->currentFunction->metadataTokenRecords[row];
        if (ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_MODULE) {
            TEST_ASSERT_NULL(module);
            module = record;
        }
        if (ZR_METADATA_TOKEN_TABLE(record->token) == ZR_METADATA_TABLE_MEMBER_DEF &&
            record->reserved0 == ZR_METADATA_TOKEN_RECORD_SCRIPT_ENTRY) {
            TEST_ASSERT_NULL(entry);
            entry = record;
        }
    }
    TEST_ASSERT_NOT_NULL_MESSAGE(entry, "PRECONDITION: actual SCRIPT_ENTRY MEMBER_DEF");
    TEST_ASSERT_NOT_NULL(module);
    TEST_ASSERT_EQUAL_UINT32(module->token, entry->ownerToken);
    TEST_ASSERT_NOT_EQUAL_UINT64(0u, compiler->currentFunction->moduleSignatureHash);
    ZrCore_ExecIr_ModuleInit(&fixture->module);
    fixture->module.moduleToken = module->token;
    fixture->module.moduleHash = compiler->currentFunction->moduleSignatureHash;
    fixture->module.contract.moduleHash = compiler->currentFunction->moduleSignatureHash;
    build.semanticFunction = &compiler->preSemanticIr;
    build.functionToken = entry->token;
    build.signatureHash = callable->structuralHash;
    build.options.preserveSourceMaps = ZR_TRUE;
    build.options.preserveDeoptStates = ZR_TRUE;
    assert_api(ZrParser_ExecIr_BuildModule(&build, &fixture->module, &diagnostic), &diagnostic,
            "PRECONDITION: BuildModule with production identity");
    function = fixture_function(fixture);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_EQUAL_UINT32(entry->token, function->functionToken);
    TEST_ASSERT_EQUAL_UINT64(callable->structuralHash, function->signatureHash);
    TEST_ASSERT_EQUAL_UINT32(entry->token, function->contract.targetToken);
    TEST_ASSERT_EQUAL_UINT64(callable->structuralHash, function->contract.signatureHash);
    attach_actual_constants(fixture);
    assert_api(ZrCore_ExecIr_VerifyFunction(function, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
            &diagnostic, "PRECONDITION: input VERIFY_ALL");
    TEST_ASSERT_EQUAL_UINT32(3u, function->instructionCount);
    TEST_ASSERT_EQUAL_UINT32(3u, function->valueCount);
    TEST_ASSERT_EQUAL_UINT32(1u, function->blockCount);
    TEST_ASSERT_EQUAL_UINT32(0u, function->phiCount);
    TEST_ASSERT_EQUAL_UINT32(0u, function->memoryTokenCount);
    TEST_ASSERT_EQUAL_UINT32(0u, function->deoptStateCount);
    TEST_ASSERT_EQUAL_UINT32(0u, function->bindingRowCount);
    TEST_ASSERT_EQUAL_UINT32(0u, function->gcMapCount);
    TEST_ASSERT_NULL(function->frameLayout);
    TEST_ASSERT_FALSE(function->sealed);
    assert_source_maps(fixture, function);
    placeSource = ZrParser_SemanticIr_InstructionAt(&compiler->preSemanticIr, 1u);
    TEST_ASSERT_EQUAL_INT(ZR_SEMANTIC_IR_PLACE_BASE, placeSource->opcode);
    place = ZrParser_PlaceGraph_Get(&compiler->preSemanticIr.places, placeSource->placeId);
    TEST_ASSERT_NOT_NULL(place);
    TEST_ASSERT_EQUAL_INT(ZR_PARSER_PLACE_BASE_TEMPORARY, place->base.kind);
    TEST_ASSERT_EQUAL_UINT32(0u, (TZrUInt32)place->projections.length);
    TEST_ASSERT_EQUAL_UINT32(placeSource->typeId, place->typeId);
    TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_PLACE_BASE, function->instructions[1].opcode);
    TEST_ASSERT_EQUAL_UINT32(placeSource->id, function->instructions[1].sourceId);
    TEST_ASSERT_EQUAL_UINT32(2u, function->resultPool[function->instructions[1].results.offset]);
    TEST_ASSERT_EQUAL_UINT32(3u, function->operandPool[function->instructions[1].operands.offset]);
    TEST_ASSERT_TRUE((function->values[1].flags & ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS) != 0u);
    TEST_ASSERT_TRUE((function->values[2].flags & ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY) != 0u);
    TEST_ASSERT_EQUAL_UINT32(2u, function->values[1].definition);
    TEST_ASSERT_EQUAL_UINT32(0u, function->values[2].definition);
    if (function->stateMap != ZR_NULL) {
        TEST_ASSERT_EQUAL_UINT32(0u, function->stateMap->entryCount);
        TEST_ASSERT_EQUAL_UINT32(0u, function->stateMap->valueCount);
        TEST_ASSERT_EQUAL_UINT32(0u, function->stateMap->rootCount);
        TEST_ASSERT_EQUAL_UINT32(0u, function->stateMap->ownerStateCount);
    }
    fixture->initialValues = (SZrExecIrOracleValue *)calloc(function->valueCount,
            sizeof(*fixture->initialValues));
    TEST_ASSERT_NOT_NULL(fixture->initialValues);
    fixture->initialValues[2].kind = ZR_EXEC_IR_ORACLE_VALUE_SIGNED;
    fixture->initialValues[2].as.signedInteger = (TZrInt64)place->base.identity;
    assert_oracle(fixture, function, 1u);
    return fixture;
}

static void assert_candidate(SZrDeadPlaceFixture *fixture, const SZrExecIrFunction *candidate) {
    const SZrExecIrFunction *input = fixture_function(fixture);
    SZrExecIrDiagnostic diagnostic = {0};
    TZrUInt32 index, operandOffset = 0u, resultOffset = 0u;
    TEST_ASSERT_EQUAL_UINT32(input->id, candidate->id);
    TEST_ASSERT_EQUAL_UINT32(input->functionToken, candidate->functionToken);
    TEST_ASSERT_EQUAL_UINT64(input->signatureHash, candidate->signatureHash);
    TEST_ASSERT_EQUAL_MEMORY(&input->contract, &candidate->contract, sizeof(input->contract));
    TEST_ASSERT_EQUAL_UINT32(input->instructionCount, candidate->instructionCount);
    TEST_ASSERT_EQUAL_UINT32(input->blockCount, candidate->blockCount);
    TEST_ASSERT_EQUAL_MEMORY(input->blocks, candidate->blocks, input->blockCount * sizeof(*input->blocks));
    TEST_ASSERT_EQUAL_UINT32(input->sourceMapCount, candidate->sourceMapCount);
    TEST_ASSERT_EQUAL_MEMORY(input->sourceMaps, candidate->sourceMaps,
            input->sourceMapCount * sizeof(*input->sourceMaps));
    TEST_ASSERT_EQUAL_UINT32(1u, candidate->valueCount);
    TEST_ASSERT_EQUAL_UINT32(input->valueCount - 2u, candidate->valueCount);
    TEST_ASSERT_EQUAL_MEMORY(&input->values[0], &candidate->values[0], sizeof(*input->values));
    for (index = 0u; index < candidate->valueCount; ++index) {
        TEST_ASSERT_EQUAL_UINT32(index + 1u, candidate->values[index].id);
        TEST_ASSERT_EQUAL_UINT32(0u, candidate->values[index].flags &
                (ZR_EXEC_IR_VALUE_FLAG_PLACE_ADDRESS | ZR_EXEC_IR_VALUE_FLAG_EXTERNAL_ENTRY));
    }
    for (index = 0u; index < candidate->instructionCount; ++index) {
        SZrExecIrInstruction before = input->instructions[index];
        SZrExecIrInstruction after = candidate->instructions[index];
        TZrUInt32 poolIndex;
        TEST_ASSERT_EQUAL_UINT32(before.sourceId, after.sourceId);
        TEST_ASSERT_EQUAL_UINT32(operandOffset, after.operands.offset);
        TEST_ASSERT_EQUAL_UINT32(resultOffset, after.results.offset);
        operandOffset += after.operands.count;
        resultOffset += after.results.count;
        for (poolIndex = 0u; poolIndex < after.operands.count; ++poolIndex) {
            TZrExecIrValueId value = candidate->operandPool[after.operands.offset + poolIndex];
            TEST_ASSERT_TRUE(value > 0u && value <= candidate->valueCount);
        }
        for (poolIndex = 0u; poolIndex < after.results.count; ++poolIndex) {
            TZrExecIrValueId value = candidate->resultPool[after.results.offset + poolIndex];
            TEST_ASSERT_TRUE(value > 0u && value <= candidate->valueCount);
            TEST_ASSERT_EQUAL_UINT32(index + 1u, candidate->values[value - 1u].definition);
        }
        if (before.opcode == ZR_EXEC_IR_OPCODE_PLACE_BASE) {
            TEST_ASSERT_EQUAL_UINT16(ZR_EXEC_IR_OPCODE_NOP, after.opcode);
            TEST_ASSERT_EQUAL_UINT32(0u, after.operands.count);
            TEST_ASSERT_EQUAL_UINT32(0u, after.results.count);
            TEST_ASSERT_EQUAL_UINT16(0u, after.flags);
            TEST_ASSERT_EQUAL_UINT32(0u, after.effectIn);
            TEST_ASSERT_EQUAL_UINT32(0u, after.effectOut);
            TEST_ASSERT_EQUAL_UINT32(0u, after.memoryIn.count);
            TEST_ASSERT_EQUAL_UINT32(0u, after.memoryOut.count);
            TEST_ASSERT_EQUAL_UINT32(0u, after.deoptId);
            TEST_ASSERT_EQUAL_UINT32(0u, after.bindingRow);
        } else {
            memset(&before.operands, 0, sizeof(before.operands));
            memset(&before.results, 0, sizeof(before.results));
            memset(&after.operands, 0, sizeof(after.operands));
            memset(&after.results, 0, sizeof(after.results));
            TEST_ASSERT_EQUAL_MEMORY(&before, &after, sizeof(before));
        }
    }
    TEST_ASSERT_EQUAL_UINT32(operandOffset, candidate->operandCount);
    TEST_ASSERT_EQUAL_UINT32(resultOffset, candidate->resultCount);
    TEST_ASSERT_EQUAL_UINT32(1u, candidate->operandCount);
    TEST_ASSERT_EQUAL_UINT32(1u, candidate->resultCount);
    if (input->stateMap != ZR_NULL) {
        TEST_ASSERT_NOT_NULL(candidate->stateMap);
        TEST_ASSERT_EQUAL_UINT32(input->stateMap->functionToken, candidate->stateMap->functionToken);
        TEST_ASSERT_EQUAL_UINT64(input->stateMap->signatureHash, candidate->stateMap->signatureHash);
        TEST_ASSERT_EQUAL_UINT64(input->stateMap->generation, candidate->stateMap->generation);
        TEST_ASSERT_EQUAL_UINT32(0u, candidate->stateMap->entryCount);
        TEST_ASSERT_EQUAL_UINT32(0u, candidate->stateMap->valueCount);
        TEST_ASSERT_EQUAL_UINT32(0u, candidate->stateMap->rootCount);
        TEST_ASSERT_EQUAL_UINT32(0u, candidate->stateMap->ownerStateCount);
    }
    assert_api(ZrCore_ExecIr_VerifyFunction(candidate, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
            &diagnostic, "candidate VERIFY_ALL after publication");
    assert_source_maps(fixture, candidate);
    assert_oracle(fixture, candidate, 0u);
}

static void eliminate_and_assert(SZrDeadPlaceFixture *fixture) {
    SZrExecIrDiagnostic diagnostic = {0};
    SZrExecIrFunction *input = fixture_function(fixture);
    TZrUInt64 before = function_digest(input), sourceBefore = source_digest(fixture);
    TZrUInt64 constantsBefore = digest_bytes(0u, fixture->module.constants,
            fixture->module.constantCount * sizeof(*fixture->module.constants));
    TZrBool success = ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &fixture->compiler.preSemanticIr, fixture->compiler.semanticContext,
            input, &g_output, &diagnostic);
    TEST_ASSERT_EQUAL_UINT64(before, function_digest(input));
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_digest(fixture));
    TEST_ASSERT_EQUAL_UINT64(constantsBefore, digest_bytes(0u, fixture->module.constants,
            fixture->module.constantCount * sizeof(*fixture->module.constants)));
    assert_api(success, &diagnostic,
            "FEATURE RED: verified literal source dead temporary place must eliminate");
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_NONE, diagnostic.code);
    assert_candidate(fixture, &g_output);
}

static void test_prerequisite_return_nine(void) { (void)prepare_fixture(0u); }
static void test_prerequisite_return_eight(void) { (void)prepare_fixture(1u); }
static void test_eliminate_return_nine(void) { eliminate_and_assert(prepare_fixture(0u)); }
static void test_eliminate_return_eight(void) { eliminate_and_assert(prepare_fixture(1u)); }

static void test_repeat_original_input(void) {
    SZrDeadPlaceFixture *fixture = prepare_fixture(0u);
    eliminate_and_assert(fixture);
    eliminate_and_assert(fixture);
}

static void test_replace_actual_eight_output(void) {
    SZrDeadPlaceFixture *nine = prepare_fixture(0u), *eight = prepare_fixture(1u);
    SZrExecIrDiagnostic diagnostic = {0};
    assert_api(ZrCore_ExecIr_CloneFunction(fixture_function(eight), &g_output, &diagnostic),
            &diagnostic, "PRECONDITION: actual eight output");
    eliminate_and_assert(nine);
}

static void assert_refused(SZrDeadPlaceFixture *fixture,
        const SZrExecIrFunction *input, EZrExecutionDiagnosticCode expectedCode) {
    SZrExecIrDiagnostic diagnostic = {0};
    TZrUInt64 inputBefore = function_digest(input);
    TZrUInt64 outputBefore = function_digest(&g_output);
    TZrUInt64 sourceBefore = source_digest(fixture);
    TEST_ASSERT_FALSE(ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &fixture->compiler.preSemanticIr, fixture->compiler.semanticContext,
            input, &g_output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(expectedCode, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT64(inputBefore, function_digest(input));
    TEST_ASSERT_EQUAL_UINT64(outputBefore, function_digest(&g_output));
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_digest(fixture));
}

static SZrDeadPlaceFixture *prepare_guard(void) {
    SZrDeadPlaceFixture *nine = prepare_fixture(0u), *eight = prepare_fixture(1u);
    SZrExecIrDiagnostic diagnostic = {0};
    assert_api(ZrCore_ExecIr_CloneFunction(fixture_function(nine), &g_mutated, &diagnostic),
            &diagnostic, "PRECONDITION: source-produced Core clone");
    assert_api(ZrCore_ExecIr_CloneFunction(fixture_function(eight), &g_output, &diagnostic),
            &diagnostic, "PRECONDITION: retained actual eight output");
    return nine;
}

/* Single-field perturbations below retain production IDs/types/AST/context.
 * Valid unsupported graphs are explicitly VERIFY_ALL checked; the invalid
 * range case is kept separate from those semantic refusal boundaries. */
static void test_guard_actual_address_use_preserves_eight(void) {
    SZrDeadPlaceFixture *fixture = prepare_guard();
    SZrExecIrDiagnostic diagnostic = {0};
    g_mutated.operandPool[g_mutated.instructions[2].operands.offset] =
            g_mutated.resultPool[g_mutated.instructions[1].results.offset];
    assert_api(ZrCore_ExecIr_VerifyFunction(&g_mutated, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
            &diagnostic, "PRECONDITION: actual address use remains valid SSA");
    assert_refused(fixture, &g_mutated, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    assert_oracle(&g_fixtures[1], &g_output, 1u);
}

static void test_guard_unknown_provenance_preserves_eight(void) {
    SZrDeadPlaceFixture *fixture = prepare_guard();
    SZrExecIrDiagnostic diagnostic = {0};
    g_mutated.operandPool[g_mutated.instructions[1].operands.offset] =
            g_mutated.resultPool[g_mutated.instructions[0].results.offset];
    assert_api(ZrCore_ExecIr_VerifyFunction(&g_mutated, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
            &diagnostic, "PRECONDITION: non-builder provenance remains valid SSA");
    assert_refused(fixture, &g_mutated, ZR_EXEC_IR_DIAGNOSTIC_UNSUPPORTED);
    assert_oracle(&g_fixtures[1], &g_output, 1u);
}

static void test_guard_sealed_metadata_preserves_eight(void) {
    SZrDeadPlaceFixture *fixture = prepare_guard();
    SZrExecIrDiagnostic diagnostic = {0};
    g_mutated.sealed = ZR_TRUE;
    assert_api(ZrCore_ExecIr_VerifyFunction(&g_mutated, ZR_EXEC_IR_VERIFY_ALL, &diagnostic),
            &diagnostic, "PRECONDITION: sealed graph remains valid");
    assert_refused(fixture, &g_mutated, ZR_EXEC_IR_DIAGNOSTIC_SEALED);
}

static void test_guard_invalid_range_preserves_eight(void) {
    SZrDeadPlaceFixture *fixture = prepare_guard();
    SZrExecIrDiagnostic diagnostic = {0};
    TZrUInt64 before, outputBefore;
    g_mutated.instructions[1].operands.offset = g_mutated.operandCount;
    TEST_ASSERT_FALSE(ZrCore_ExecIr_VerifyFunction(&g_mutated, ZR_EXEC_IR_VERIFY_ALL, &diagnostic));
    before = function_digest(&g_mutated);
    outputBefore = function_digest(&g_output);
    TEST_ASSERT_FALSE(ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &fixture->compiler.preSemanticIr, fixture->compiler.semanticContext,
            &g_mutated, &g_output, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXEC_IR_DIAGNOSTIC_INVALID_RANGE, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT64(before, function_digest(&g_mutated));
    TEST_ASSERT_EQUAL_UINT64(outputBefore, function_digest(&g_output));
}

static void test_guard_input_output_alias(void) {
    SZrDeadPlaceFixture *fixture = prepare_fixture(0u);
    SZrExecIrFunction *input = fixture_function(fixture);
    SZrExecIrDiagnostic diagnostic = {0};
    TZrUInt64 before = function_digest(input), sourceBefore = source_digest(fixture);
    TEST_ASSERT_FALSE(ZrParser_ExecIr_EliminateDeadSourcePlaces(
            &fixture->compiler.preSemanticIr, fixture->compiler.semanticContext,
            input, input, &diagnostic));
    TEST_ASSERT_EQUAL_INT(ZR_EXECUTION_DIAGNOSTIC_INVALID_ARGUMENT, diagnostic.code);
    TEST_ASSERT_EQUAL_UINT64(before, function_digest(input));
    TEST_ASSERT_EQUAL_UINT64(sourceBefore, source_digest(fixture));
}

int main(int argc, char **argv) {
    TZrBool prerequisites = ZR_TRUE, features = ZR_TRUE, guards = ZR_TRUE;
    if (argc == 2 && strcmp(argv[1], "--prerequisites-only") == 0) {
        features = guards = ZR_FALSE;
    } else if (argc == 2 && strcmp(argv[1], "--features-only") == 0) {
        prerequisites = guards = ZR_FALSE;
    } else if (argc == 2 && strcmp(argv[1], "--guards-only") == 0) {
        prerequisites = features = ZR_FALSE;
    } else if (argc != 1) {
        (void)fprintf(stderr, "usage: %s [--prerequisites-only|--features-only|--guards-only]\n", argv[0]);
        return 2;
    }
    UNITY_BEGIN();
    if (prerequisites) {
        RUN_TEST(test_prerequisite_return_nine);
        RUN_TEST(test_prerequisite_return_eight);
    }
    if (features) {
        RUN_TEST(test_eliminate_return_nine);
        RUN_TEST(test_eliminate_return_eight);
        RUN_TEST(test_repeat_original_input);
        RUN_TEST(test_replace_actual_eight_output);
    }
    if (guards) {
        RUN_TEST(test_guard_actual_address_use_preserves_eight);
        RUN_TEST(test_guard_unknown_provenance_preserves_eight);
        RUN_TEST(test_guard_sealed_metadata_preserves_eight);
        RUN_TEST(test_guard_invalid_range_preserves_eight);
        RUN_TEST(test_guard_input_output_alias);
    }
    return UNITY_END();
}
